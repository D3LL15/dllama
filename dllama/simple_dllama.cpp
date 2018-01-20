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

#include "simple_dllama.h"
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

	void start_mpi_listener() {
		snapshot_merger_instance->start_snapshot_listener();
	}
}

using namespace dllama_ns;

simple_dllama::simple_dllama(bool initialise_mpi) {
	if (initialise_mpi) {
		MPI_Init(NULL, NULL);
		MPI_Comm_size(MPI_COMM_WORLD, &world_size);
		MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	}
	handling_mpi = initialise_mpi;
	
	merge_starting = 0;
	current_snapshot_level = 0;
	dllama_number_of_vertices = 0;
	self_adding_node = 0;
	num_new_node_requests = 0;
	
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
	
	current_snapshot_level = graph->num_levels();
	dllama_number_of_vertices = graph->max_nodes();
	cout << "num levels " << graph->num_levels() << "\n";
	cout << "num vertices " << graph->max_nodes() << "\n";
}

edge_t simple_dllama::add_edge(node_t src, node_t tgt) {
	node_t edge[2];
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

//currently LLAMA does not support deletions properly, so this method also does not
void simple_dllama::delete_edge(node_t src, edge_t edge) {
	graph->delete_edge(src, edge);
}

node_t simple_dllama::add_node() {
	//check that we are not already adding a node
	num_new_node_requests_lock.lock();
	self_adding_node = 1;
	num_new_node_requests_lock.unlock();
	
	int new_node_id = graph->max_nodes();
	cout << "new node id: " << new_node_id << "\n";
	//tell all the other machines you want to add a node
	for (int i = 0; i < world_size; i++) {
		if (i != world_rank) {
			MPI_Send(&new_node_id, 1, MPI_INT, i, NEW_NODE_REQUEST, MPI_COMM_WORLD);
		}
	}

	//wait for them to acknowledge that you can add a node
	int ack;
	for (int i = 0; i < world_size; i++) {
		if (i != world_rank) {
			MPI_Recv(&ack, 1, MPI_INT, MPI_ANY_SOURCE, NEW_NODE_ACK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}

	//adjust the new node id in case it changed in the previous phase
	new_node_id = graph->max_nodes();
	
	//tell them all to add the node
	for (int i = 0; i < world_size; i++) {
		if (i != world_rank) {
			MPI_Send(&new_node_id, 1, MPI_INT, i, NEW_NODE_COMMAND, MPI_COMM_WORLD);
		}
	}

	//add the node yourself
	graph->add_node(new_node_id);
	
	self_adding_node = 0;
	
	//ack all the requests on the stack
	new_node_ack_stack_lock.lock();
	int one = 1;
	while (!new_node_ack_stack.empty()) {
		MPI_Send(&one, 1, MPI_INT, new_node_ack_stack.top(), NEW_NODE_ACK, MPI_COMM_WORLD);
		new_node_ack_stack.pop();
	}
	new_node_ack_stack_lock.unlock();
	
	return new_node_id;
}

//not for manual use
void simple_dllama::add_node(node_t id) {
	graph->add_node(id);
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
	checkpoint();
}

void simple_dllama::checkpoint() {
	graph->checkpoint();
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
	cout << "added edge from " << src << " to " << tgt << "\n";
}