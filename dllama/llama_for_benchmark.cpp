#include "dllama.h"

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

namespace dllama_ns {
	int world_size;
	int world_rank;
	bool merge_starting;
	mutex merge_starting_lock;
	mutex merge_lock;
	mutex ro_graph_lock;
	mutex checkpoint_lock;
	int current_snapshot_level;
	unsigned int dllama_number_of_vertices;
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

dllama::dllama(string database_location, bool initialise_mpi) {
	this->database_location = database_location;
	handling_mpi = initialise_mpi;
	
	dllama_instance = this;
	merge_starting = 0;
	current_snapshot_level = 0;
	dllama_number_of_vertices = 0;
	self_adding_node = 0;
	num_new_node_requests = 0;
	
	snapshot_merger_instance = new snapshot_merger(database_location); //
	
	DEBUG("world size: " << world_size);
	
	//initialise llama

	ostringstream oss;
	oss << database_location << "db" << world_rank;
	const char* database_directory = oss.str().c_str();
	
	database = new ll_database(database_directory);
	//one thread for now
	database->set_num_threads(1);
	graph = database->graph();
	
	current_snapshot_level = graph->num_levels();
	dllama_number_of_vertices = graph->max_nodes() - 1;
}

dllama::~dllama() {
	//noop
}

void dllama::load_net_graph(string net_graph) {
	ll_file_loaders loaders;
	ll_file_loader* loader = loaders.loader_for(net_graph.c_str());
	if (loader == NULL) {
		fprintf(stderr, "Error: Unsupported input file type\n");
		return;
	}

	ll_loader_config loader_config;
	loader->load_direct(graph, net_graph.c_str(), &loader_config);
	
	current_snapshot_level = graph->num_levels();
	dllama_number_of_vertices = graph->max_nodes() - 1;
	DEBUG("num levels " << graph->num_levels());
	DEBUG("num vertices " << graph->max_nodes());
}

edge_t dllama::add_edge(node_t src, node_t tgt) {
	edge_t result = graph->add_edge(src, tgt);
	return result;
}

edge_t dllama::force_add_edge(node_t src, node_t tgt) {
	edge_t result = graph->add_edge(src, tgt);
	return result;
}

//currently LLAMA does not support deletions properly, so this method also does not
void dllama::delete_edge(node_t src, edge_t edge) {
	graph->delete_edge(src, edge);
}

node_t dllama::max_nodes() {
	return graph->max_nodes();
}

node_t dllama::add_node() {
	return add_nodes(1);
}

node_t dllama::add_nodes(int num_new_nodes) {
	if (num_new_nodes <= 0) {
		cout << "invalid number of nodes added: " << num_new_nodes << "\n";
		return 0;
	}
	int result = graph->add_node();
	return result;
}

//not for manual use
node_t dllama::force_add_node() {
	int result = graph->add_node();
	return result;
}

size_t dllama::out_degree(node_t node) {
	size_t result = graph->out_degree(node);
	return result;
}

void dllama::request_checkpoint() {
	checkpoint();
}

//call after a certain amount of updates
void dllama::auto_checkpoint() {
	//TODO: have this be called automatically
	checkpoint();
}

void dllama::checkpoint() {
	//DEBUG("current number of levels before checkpoint: " << graph->num_levels());
	graph->checkpoint();
	dllama_number_of_vertices = graph->max_nodes() - 1;
	current_snapshot_level = graph->num_levels();
}

//asynchronous
void dllama::start_merge() {
	DEBUG("Rank " << world_rank << " manually starting merge");
	snapshot_merger_instance->merge_local_llama();
	refresh_ro_graph();

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
	ll_edge_iterator iter;
	out_iter_begin(iter, vertex);
	vector<node_t> result;
	while (out_iter_has_next(iter)) {
		out_iter_next(iter);
		result.push_back(iter.last_node);
	}
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
	//char* database_directory = (char*) alloca(20);
	ostringstream oss;
	oss << database_location << "db" << world_rank;
	//strcpy(database_directory, oss.str().c_str());
	const char* database_directory = oss.str().c_str();
	
	database->reset_storage();
	ll_persistent_storage* new_storage = database->storage();
	graph->refresh_ro_graph(database, new_storage, world_rank, database_location);
}

void dllama::delete_db() {
	for (unsigned int i = 0; i < graph->num_levels() - 1; i++) {
		ostringstream oss;
		oss << database_location << "db" << world_rank << "/csr__out__" << i << ".dat";
		string file_name = oss.str();
		DEBUG("deleting snapshot '" << file_name << "'");
		remove(file_name.c_str());
	}
}

void dllama::shutdown() {
	//noop
}