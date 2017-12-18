#include <cstdlib>
#include <iostream>
#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>
#include <vector>

#include "dllama.h"
#include "shared_thread_state.h"

using namespace std;

dllama::dllama() {
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
}

dllama::~dllama() {
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

	cout << "num levels " << graph->num_levels() << "\n";
}

edge_t dllama::add_edge(node_t src, node_t tgt) {
	return graph->add_edge(src, tgt);
}

node_t dllama::add_node() {
	//TODO: get vertex number from vertex allocator service
	return graph->add_node();
}

size_t dllama::out_degree(node_t node) {
	return graph->out_degree(node);
}

//call after a certain amount of updates

void dllama::auto_checkpoint() {
	//TODO: have this be called automatically
	//check if merge occurring before writing new file, also prevent the other thread sending a merge request before we are done sending our snapshot
	merge_starting_lock.lock();
	//TODO: check if there has been a merge while we were waiting for the lock, may not be necessary
	
	if (!merge_starting) {
		graph->checkpoint();

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
	merge_starting_lock.unlock();
}

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
	graph->add_edge(src, tgt);
	cout << "added edge from " << src << " to " << tgt << "\n";
}

void dllama::refresh_ro_graph() {
	cout << "refreshing ro graph\n";
	char* database_directory = (char*) alloca(20);
	ostringstream oss;
	oss << "db" << world_rank;
	strcpy(database_directory, oss.str().c_str());
	//ll_persistent_storage* new_storage = new ll_persistent_storage(database_directory);
	//database->storage()->~ll_persistent_storage();
	//ll_persistent_storage* old_storage = database->storage();
	//old_storage = new_storage;
	database->reset_storage();
	ll_persistent_storage* new_storage = database->storage();
	graph->refresh_ro_graph(database, new_storage, world_rank);
}