#include "simple_dllama.h"

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

#include "shared_thread_state.h"
#include "snapshot_manager.h"
#include "snapshot_merger.h"

using namespace std;

using namespace dllama_ns;

simple_dllama::simple_dllama(string database_location, bool initialise_mpi) {
	this->database_location = database_location;
	if (initialise_mpi) {
		int p = 0;
		int *provided;
		provided = &p;
		MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, provided);
		MPI_Comm_size(MPI_COMM_WORLD, &world_size);
		MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
		if (*provided != MPI_THREAD_MULTIPLE) {
			cout << "ERROR: MPI implementation doesn't provide multi-thread support\n";
		}
	}
	handling_mpi = initialise_mpi;
	
	sstate = new shared_thread_state(this, database_location);

	mpi_listener = new thread(start_mpi_listener);
	
	DEBUG("Rank " << world_rank << " main and mpi_listener threads now execute concurrently...");
	DEBUG("world size: " << world_size);
	
	ostringstream oss;
	oss << database_location << "db" << world_rank;
	const char* database_directory = oss.str().c_str();
	
	database = new ll_database(database_directory);
	//one thread for now
	database->set_num_threads(1);
	graph = database->graph();
	
	sstate->current_snapshot_level = graph->num_levels();
	sstate->dllama_number_of_vertices = graph->max_nodes() - 1;
}

simple_dllama::~simple_dllama() {
	if (handling_mpi) {
		MPI_Finalize();
	}
}

void simple_dllama::load_net_graph(string net_graph) {
	ll_file_loaders loaders;
	ll_file_loader* loader = loaders.loader_for(net_graph.c_str());
	if (loader == NULL) {
		fprintf(stderr, "Error: Unsupported input file type\n");
		return;
	}

	ll_loader_config loader_config;
	loader->load_direct(graph, net_graph.c_str(), &loader_config);
	
	sstate->current_snapshot_level = graph->num_levels();
	sstate->dllama_number_of_vertices = graph->max_nodes() - 1;
	DEBUG("num levels " << graph->num_levels());
	DEBUG("num vertices " << graph->max_nodes());
}

edge_t simple_dllama::add_edge(node_t src, node_t tgt) {
	int edge[2];
	edge[0] = src;
	edge[1] = tgt;
	for (int i = 0; i < world_size; i++) {
		if (i != world_rank) {
			MPI_Send(&edge, 2, MPI_INT, i, NEW_EDGE, MPI_COMM_WORLD);
		}
	}
	edge_t result = graph->add_edge(src, tgt);
	return result;
}

edge_t simple_dllama::force_add_edge(node_t src, node_t tgt) {
	sstate->checkpoint_lock.lock();
	edge_t result =  graph->add_edge(src, tgt);
	sstate->checkpoint_lock.unlock();
	return result;
}

//currently LLAMA does not support deletions properly, so this method also does not
void simple_dllama::delete_edge(node_t src, edge_t edge) {
	graph->delete_edge(src, edge);
}

node_t simple_dllama::max_nodes() {
	return graph->max_nodes();
}

node_t simple_dllama::add_node() {
	return add_nodes(1);
}

node_t simple_dllama::add_nodes(int num_new_nodes) {
	if (num_new_nodes <= 0) {
		cout << "invalid number of nodes added: " << num_new_nodes << "\n";
		return 0;
	}
	//ensure that we are not already adding a node
	sstate->num_new_node_requests_lock.lock();
	sstate->self_adding_node = 1;
	sstate->num_new_node_requests_lock.unlock();
	
	//int new_node_id = graph->max_nodes();
	DEBUG("Rank " << world_rank << " adding nodes");
	//tell all the other machines you want to add a node
	for (int i = 0; i < world_size; i++) {
		if (i != world_rank) {
			MPI_Send(&num_new_nodes, 1, MPI_INT, i, NEW_NODE_REQUEST, MPI_COMM_WORLD);
		}
	}

	//wait for them to acknowledge that you can add a node
	unique_lock<mutex> lk(sstate->num_acks_lock);
	sstate->num_acks_condition.wait(lk, []{return sstate->num_acks == (world_size - 1);});
	sstate->num_acks = 0;
	lk.unlock();
	
	//tell them all to add the node
	for (int i = 0; i < world_size; i++) {
		if (i != world_rank) {
			MPI_Send(&num_new_nodes, 1, MPI_INT, i, NEW_NODE_COMMAND, MPI_COMM_WORLD);
		}
	}

	//add the node yourself
	int new_node_id = 0;
	for (int i = 0; i < num_new_nodes; i++) {
		new_node_id = graph->add_node();
	}
	new_node_id = new_node_id + 1 - num_new_nodes;
	
	//ack all the requests on the stack
	sstate->new_node_ack_stack_lock.lock();
	sstate->self_adding_node = 0;
	int one = 1;
	while (!sstate->new_node_ack_stack.empty()) {
		DEBUG("Rank " << world_rank << "sending ack to rank " << new_node_ack_stack.top());
		MPI_Send(&one, 1, MPI_INT, sstate->new_node_ack_stack.top(), NEW_NODE_ACK, MPI_COMM_WORLD);
		sstate->new_node_ack_stack.pop();
	}
	sstate->new_node_ack_stack_lock.unlock();
	
	return new_node_id;
}

//not for manual use
node_t simple_dllama::force_add_nodes(int num_nodes) {
	sstate->checkpoint_lock.lock();
	int result;
	for (int i = 0; i < num_nodes; i++) {
		result = graph->add_node();
	}
	sstate->checkpoint_lock.unlock();
	return result;
}

size_t simple_dllama::out_degree(node_t node) {
	size_t result = graph->out_degree(node);
	return result;
}

void simple_dllama::request_checkpoint() {
	checkpoint();
}

//call after a certain amount of updates
void simple_dllama::auto_checkpoint() {
	//TODO: have this be called automatically
	//check if merge occurring before writing new file, also prevent the other thread sending a merge request before we are done sending our snapshot
	sstate->merge_starting_lock.lock();
	if (!sstate->merge_starting) {
		checkpoint();
	}
	sstate->merge_starting_lock.unlock();
}

void simple_dllama::checkpoint() {
	//DEBUG("current number of levels before checkpoint: " << graph->num_levels());
	//the checkpoint lock ensures that dllama_number_of_vertices is only the number of vertices in snapshots, not in the writable llama
	sstate->checkpoint_lock.lock();
	graph->checkpoint();
	sstate->dllama_number_of_vertices = graph->max_nodes() - 1;
	sstate->checkpoint_lock.unlock();
	sstate->current_snapshot_level = graph->num_levels();
}

//asynchronous
void simple_dllama::start_merge() {
	DEBUG("Rank " << world_rank << " manually starting merge");
	sstate->merge_starting_lock.lock();
	sstate->merge_starting = 1;
	sstate->merge_starting_lock.unlock();
	sstate->merge_lock.lock();
	sstate->snapshot_merger_instance->merge_local_llama();
	refresh_ro_graph();
	sstate->merge_lock.unlock();
	sstate->merge_starting_lock.lock();
	sstate->merge_starting = 0;
	sstate->merge_starting_lock.unlock();
}

//TODO: make these private
void simple_dllama::out_iter_begin(ll_edge_iterator& iter, node_t node) {
	graph->out_iter_begin(iter, node);
}

ITERATOR_DECL bool simple_dllama::out_iter_has_next(ll_edge_iterator& iter) {
	return graph->out_iter_has_next(iter);
}

ITERATOR_DECL edge_t simple_dllama::out_iter_next(ll_edge_iterator& iter) {
	return graph->out_iter_next(iter);
}

vector<node_t> simple_dllama::get_neighbours_of_vertex(node_t vertex) {
	ll_edge_iterator iter;
	out_iter_begin(iter, vertex);
	vector<node_t> result;
	while (out_iter_has_next(iter)) {
		out_iter_next(iter);
		result.push_back(iter.last_node);
	}
	return result;
}

void simple_dllama::add_random_edge() {
	node_t src = graph->pick_random_node();
	node_t tgt = graph->pick_random_node();
	add_edge(src, tgt);
	DEBUG("added edge from " << src << " to " << tgt);
}

void simple_dllama::refresh_ro_graph() {
	DEBUG("refreshing ro graph");
	database->reset_storage();
	ll_persistent_storage* new_storage = database->storage();
	graph->refresh_ro_graph(database, new_storage, world_rank, database_location);
}

void simple_dllama::delete_db() {
	for (unsigned int i = 0; i < graph->num_levels() - 1; i++) {
		ostringstream oss;
		oss << database_location << "db" << world_rank << "/csr__out__" << i << ".dat";
		string file_name = oss.str();
		DEBUG("deleting snapshot '" << file_name << "'");
		remove(file_name.c_str());
	}
}

void simple_dllama::shutdown() {
	int nop = 0;
	MPI_Send(&nop, 1, MPI_INT, world_rank, SHUTDOWN, MPI_COMM_WORLD);
	mpi_listener->join();
	//MPI_Finalize();
}