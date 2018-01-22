#include <cstdlib>
#include <iostream>
#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>
#include <vector>
#include <thread>
#include <stack>
#include <stdio.h>
#include <condition_variable>

#include "dllama.h"
#include "shared_thread_state.h"

using namespace std;

namespace dllama_ns {
	int world_size;
	int world_rank;
	bool merge_starting;
	mutex merge_starting_lock;
	mutex merge_lock;
	mutex ro_graph_lock;
	mutex checkpoint_lock;
	int current_snapshot_level;
	int dllama_number_of_vertices;
	dllama* dllama_instance;
	snapshot_merger* snapshot_merger_instance;
	stack<int> new_node_ack_stack;
	bool self_adding_node;
	mutex num_new_node_requests_lock;
	int num_new_node_requests;
	mutex new_node_ack_stack_lock;
	int num_acks;
	mutex num_acks_lock;
	condition_variable num_acks_condition;

	void start_mpi_listener() {
		snapshot_merger_instance->start_snapshot_listener();
	}
}

using namespace dllama_ns;

dllama::dllama(bool initialise_mpi) {
	if (initialise_mpi) {
		MPI_Init(NULL, NULL);
		MPI_Comm_size(MPI_COMM_WORLD, &world_size);
		MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	}
	handling_mpi = initialise_mpi;
	
	dllama_instance = this;
	merge_starting = 0;
	current_snapshot_level = 0;
	dllama_number_of_vertices = 0;
	self_adding_node = 0;
	num_new_node_requests = 0;
	
	snapshot_merger_instance = new snapshot_merger(); //
	mpi_listener = new thread(start_mpi_listener);
	
	DEBUG("Rank " << world_rank << " main and mpi_listener threads now execute concurrently...");
	DEBUG("world size: " << world_size);
	
	//initialise llama
	char* database_directory = (char*) alloca(20);

	//this only applies if we are simulating on a single machine
	ostringstream oss;
	oss << "db" << world_rank;
	strcpy(database_directory, oss.str().c_str());

	database = new ll_database(database_directory);
	//one thread for now
	database->set_num_threads(1);
	graph = database->graph();
	
	current_snapshot_level = graph->num_levels();
	dllama_number_of_vertices = graph->max_nodes();
}

dllama::~dllama() {
	if (handling_mpi) {
		MPI_Finalize();
	}
}

void dllama::load_net_graph(string net_graph) {
	merge_lock.lock();
	ll_file_loaders loaders;
	ll_file_loader* loader = loaders.loader_for(net_graph.c_str());
	if (loader == NULL) {
		fprintf(stderr, "Error: Unsupported input file type\n");
		return;
	}

	ll_loader_config loader_config;
	loader->load_direct(graph, net_graph.c_str(), &loader_config);
	
	current_snapshot_level = graph->num_levels();
	dllama_number_of_vertices = graph->max_nodes();
	DEBUG("num levels " << graph->num_levels());
	DEBUG("num vertices " << graph->max_nodes());
	merge_lock.unlock();
}

edge_t dllama::add_edge(node_t src, node_t tgt) {
	return graph->add_edge(src, tgt);
}

//currently LLAMA does not support deletions properly, so this method also does not
void dllama::delete_edge(node_t src, edge_t edge) {
	graph->delete_edge(src, edge);
}

node_t dllama::max_nodes() {
	return graph->max_nodes();
}

node_t dllama::add_node() {
	//ensure that we are not already adding a node
	num_new_node_requests_lock.lock();
	self_adding_node = 1;
	num_new_node_requests_lock.unlock();
	
	int new_node_id = graph->max_nodes();
	DEBUG("Rank " << world_rank << " new node id: " << new_node_id);
	//tell all the other machines you want to add a node
	for (int i = 0; i < world_size; i++) {
		if (i != world_rank) {
			MPI_Send(&new_node_id, 1, MPI_INT, i, NEW_NODE_REQUEST, MPI_COMM_WORLD);
		}
	}

	//wait for them to acknowledge that you can add a node
	unique_lock<mutex> lk(num_acks_lock);
	num_acks_condition.wait(lk, []{return num_acks == (world_size - 1);});
	num_acks = 0;
	lk.unlock();
	
	//adjust the new node id in case it changed in the previous phase
	new_node_id = graph->max_nodes();
	
	//tell them all to add the node
	for (int i = 0; i < world_size; i++) {
		if (i != world_rank) {
			MPI_Send(&new_node_id, 1, MPI_INT, i, NEW_NODE_COMMAND, MPI_COMM_WORLD);
		}
	}

	//add the node yourself
	new_node_id = graph->add_node();
	
	//ack all the requests on the stack
	new_node_ack_stack_lock.lock();
	self_adding_node = 0;
	int one = 1;
	while (!new_node_ack_stack.empty()) {
		MPI_Send(&one, 1, MPI_INT, new_node_ack_stack.top(), NEW_NODE_ACK, MPI_COMM_WORLD);
		new_node_ack_stack.pop();
	}
	new_node_ack_stack_lock.unlock();
	
	return new_node_id;
}

//not for manual use
node_t dllama::add_node(node_t id) {
	checkpoint_lock.lock();
	int result = graph->add_node();
	checkpoint_lock.unlock();
	return result;
}

size_t dllama::out_degree(node_t node) {
	ro_graph_lock.lock();
	size_t result = graph->out_degree(node);
	ro_graph_lock.unlock();
	return result;
}

void dllama::request_checkpoint() {
	merge_lock.lock();
	checkpoint();
	merge_lock.unlock();
}

//call after a certain amount of updates
void dllama::auto_checkpoint() {
	//TODO: have this be called automatically
	//check if merge occurring before writing new file, also prevent the other thread sending a merge request before we are done sending our snapshot
	merge_starting_lock.lock();
	if (!merge_starting) {
		checkpoint();
	}
	merge_starting_lock.unlock();
}

void dllama::checkpoint() {
	DEBUG("current number of levels before checkpoint: " << graph->num_levels());
	//the checkpoint lock ensures that dllama_number_of_vertices is only the number of vertices in snapshots, not in the writable llama
	checkpoint_lock.lock();
	graph->checkpoint();
	dllama_number_of_vertices = graph->max_nodes();
	checkpoint_lock.unlock();
	current_snapshot_level = graph->num_levels();

	uint32_t file_number = (graph->num_levels() - 2) / LL_LEVELS_PER_ML_FILE;
	if ((graph->num_levels() - 1) % LL_LEVELS_PER_ML_FILE == 0) {
		DEBUG("Rank " << world_rank << " sending snapshot file");
		streampos file_size;
		char * memblock;

		ostringstream oss;
		oss << "db" << world_rank << "/csr__out__" << file_number << ".dat";

		string input_file_name = oss.str();

		ifstream file(input_file_name, ios::in | ios::binary | ios::ate);
		if (file.is_open()) {
			file_size = file.tellg();
			int memblock_size = file_size;
			memblock_size += 8;
			memblock = new char [memblock_size];
			memblock[0] = (file_number >> 24) & 0xFF;
			memblock[1] = (file_number >> 16) & 0xFF;
			memblock[2] = (file_number >> 8) & 0xFF;
			memblock[3] = file_number & 0xFF;
			memblock[4] = (dllama_number_of_vertices >> 24) & 0xFF;
			memblock[5] = (dllama_number_of_vertices >> 16) & 0xFF;
			memblock[6] = (dllama_number_of_vertices >> 8) & 0xFF;
			memblock[7] = dllama_number_of_vertices & 0xFF;
			file.seekg(0, ios::beg);
			file.read(memblock + 8, memblock_size);
			file.close();

			for (int i = 0; i < world_size; i++) {
				if (i != world_rank) {
					MPI_Send(memblock, memblock_size, MPI_BYTE, i, SNAPSHOT_MESSAGE, MPI_COMM_WORLD);
					DEBUG("sent snapshot: " << file_number);
				}
			}

			delete[] memblock;
		} else cout << "Rank " << world_rank << " unable to open input snapshot file" << "\n";
	}
}

//asynchronous
void dllama::start_merge() {
	//MPI_Request mpi_req;
	//MPI_Isend(&current_snapshot_level, 1, MPI_INT, world_rank, START_MERGE_REQUEST, MPI_COMM_WORLD, &mpi_req);
	//MPI_Request_free(&mpi_req);
	DEBUG("Rank " << world_rank << " manually starting merge");
	snapshot_merger_instance->begin_merge();
}

//TODO: make these private
void dllama::out_iter_begin(ll_edge_iterator& iter, node_t node) {
	graph->out_iter_begin(iter, node);
}

ITERATOR_DECL bool dllama::out_iter_has_next(ll_edge_iterator& iter) {
	return graph->out_iter_has_next(iter);
}

ITERATOR_DECL edge_t dllama::out_iter_next(ll_edge_iterator& iter) {
	return graph->out_iter_next(iter);
}

vector<node_t> dllama::get_neighbours_of_vertex(node_t vertex) {
	ro_graph_lock.lock();
	ll_edge_iterator iter;
	out_iter_begin(iter, vertex);
	vector<node_t> result;
	while (out_iter_has_next(iter)) {
		out_iter_next(iter);
		result.push_back(iter.last_node);
	}
	ro_graph_lock.unlock();
	return result;
}

void dllama::add_random_edge() {
	node_t src = graph->pick_random_node();
	node_t tgt = graph->pick_random_node();
	add_edge(src, tgt);
	DEBUG("added edge from " << src << " to " << tgt);
}

void dllama::refresh_ro_graph() {
	DEBUG("refreshing ro graph");
	char* database_directory = (char*) alloca(20);
	ostringstream oss;
	oss << "db" << world_rank;
	strcpy(database_directory, oss.str().c_str());

	database->reset_storage();
	ll_persistent_storage* new_storage = database->storage();
	graph->refresh_ro_graph(database, new_storage, world_rank);
}

void dllama::delete_db() {
	for (unsigned int i = 0; i < graph->num_levels() - 1; i++) {
		ostringstream oss;
		oss << "db" << world_rank << "/csr__out__" << i << ".dat";
		string file_name = oss.str();
		DEBUG("deleting snapshot '" << file_name << "'");
		remove(file_name.c_str());
	}
}