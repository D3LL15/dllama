#include <cstdlib>
#include <iostream>
#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>
#include <vector>
#include <thread>

#include "dllama.h"
#include "shared_thread_state.h"

using namespace std;

int world_size;
int world_rank;
bool merge_starting;
mutex merge_starting_lock;
mutex ro_graph_lock;
int current_snapshot_level;
int dllama_number_of_vertices;
dllama* dllama_instance;
snapshot_merger* snapshot_merger_instance;

void start_mpi_listener() {
	snapshot_merger_instance->start_snapshot_listener();
}

dllama::dllama() {
	
	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	
	merge_starting = 0;
	current_snapshot_level = 0;
	dllama_number_of_vertices = 0;
	
	snapshot_merger_instance = new snapshot_merger(); //
	mpi_listener = new thread(start_mpi_listener);
	
	cout << "Rank " << world_rank << " main and mpi_listener threads now execute concurrently...\n";
	cout << "world size: " << world_size << "\n";
	
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
	MPI_Finalize();
}

void dllama::load_net_graph(string net_graph) {
	merge_starting_lock.lock();
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
	cout << "num levels " << graph->num_levels() << "\n";
	cout << "num vertices " << graph->max_nodes() << "\n";
	merge_starting_lock.unlock();
}

edge_t dllama::add_edge(node_t src, node_t tgt) {
	return graph->add_edge(src, tgt);
}

//currently LLAMA does not support deletions properly, so this method also does not
void dllama::delete_edge(node_t src, edge_t edge) {
	graph->delete_edge(src, edge);
}

node_t dllama::add_node() {
	//TODO: get vertex number from vertex allocator service
	return graph->add_node();
}

size_t dllama::out_degree(node_t node) {
	ro_graph_lock.lock();
	size_t result = graph->out_degree(node);
	ro_graph_lock.unlock();
	return result;
}

void dllama::request_checkpoint() {
	while (true) {
		merge_starting_lock.lock();
		if (!merge_starting) {
			checkpoint();
			break;
		}
		merge_starting_lock.unlock();
	}
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
	graph->checkpoint();
	current_snapshot_level = graph->num_levels();

	uint32_t file_number = (graph->num_levels() - 2) / LL_LEVELS_PER_ML_FILE;
	if ((graph->num_levels() - 1) % LL_LEVELS_PER_ML_FILE == 0) {
		//if we have written to the snapshot file for the final time then send it, only applies if you have multiple levels per file
		cout << "Rank " << world_rank << " sending snapshot file\n";
		streampos file_size;
		char * memblock;

		ostringstream oss;
		oss << "db" << world_rank << "/csr__out__" << file_number << ".dat";

		string input_file_name = oss.str();

		ifstream file(input_file_name, ios::in | ios::binary | ios::ate);
		if (file.is_open()) {
			file_size = file.tellg();
			int memblock_size = file_size;
			memblock_size += 4;
			memblock = new char [memblock_size];
			memblock[0] = (file_number >> 24) & 0xFF;
			memblock[1] = (file_number >> 16) & 0xFF;
			memblock[2] = (file_number >> 8) & 0xFF;
			memblock[3] = file_number & 0xFF;
			file.seekg(0, ios::beg);
			file.read(memblock + 4, memblock_size);
			file.close();

			for (int i = 0; i < world_size; i++) {
				if (i != world_rank) {
					MPI_Send(memblock, memblock_size, MPI_BYTE, i, SNAPSHOT_MESSAGE, MPI_COMM_WORLD);
				}
			}

			delete[] memblock;
		} else cout << "Rank " << world_rank << " unable to open input snapshot file\n";
	}
}

//asynchronous
void dllama::start_merge() {
	//MPI_Request mpi_req;
	//MPI_Isend(&current_snapshot_level, 1, MPI_INT, world_rank, START_MERGE_REQUEST, MPI_COMM_WORLD, &mpi_req);
	//MPI_Request_free(&mpi_req);
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
	graph->add_edge(src, tgt);
	cout << "added edge from " << src << " to " << tgt << "\n";
}

void dllama::refresh_ro_graph() {
	cout << "refreshing ro graph\n";
	char* database_directory = (char*) alloca(20);
	ostringstream oss;
	oss << "db" << world_rank;
	strcpy(database_directory, oss.str().c_str());

	database->reset_storage();
	ll_persistent_storage* new_storage = database->storage();
	graph->refresh_ro_graph(database, new_storage, world_rank);
}