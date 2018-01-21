#include <thread>
#include <iostream>
#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>
#include <stdlib.h>

#include "dllama_test.h"
#include "dllama.h"
#include "snapshot_merger.h"
#include "shared_thread_state.h"

using namespace std;
using namespace dllama_ns;

void add_nodes_benchmark() {
	dllama* dllama_instance = new dllama();
}

void add_edges_benchmark() {
	
}

void add_large_graph_benchmark() {
	
}

void read_all_edges_of_random_nodes_benchmark() {
	
}

//usage: mpirun -n 2 ./dllama.exe 4
int main(int argc, char** argv) {
	
	//select benchmarks
	if (argc == 2) {
		switch (*argv[1]) {
			case '0':
				add_nodes_benchmark();
				break;
			case '1':
				add_edges_benchmark();
				break;
			case '2':
				add_large_graph_benchmark();
				break;
			case '3':
				read_all_edges_of_random_nodes_benchmark();
				break;
			default:
				cout << "invalid benchmark number" << "\n";
		}
	}
	/*
	if (argc == 2) {
		switch (*argv[1]) {
			case '8':
				snapshot_merger_instance->read_snapshots();
				break;
			case '9':
				if (world_rank == 0) {
					int rank_snapshots[2] = {2, 2};
					snapshot_merger_instance->merge_snapshots(rank_snapshots);
				}
				break;
			case 'a':
				dllama_instance->load_net_graph("simple_graph.net");
				dllama_instance->add_edge(1, 0);
				dllama_instance->add_edge(2, 1);
				dllama_instance->auto_checkpoint();
				dllama_instance->add_edge(2, 0);
				dllama_instance->auto_checkpoint();
				snapshot_merger_instance->read_snapshots();
				break;
			case 'c':
				dllama_instance->load_net_graph("simple_graph.net");
				dllama_instance->add_edge(1, 0);
				dllama_instance->add_edge(2, 1);
				dllama_instance->auto_checkpoint();
				
				
				break;
		}
	}
	*/
	return 0;
}

