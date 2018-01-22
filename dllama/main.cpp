#include <thread>
#include <iostream>
#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>
#include <stdlib.h>
#include <chrono>

#include "dllama_test.h"
#include "dllama.h"
#include "snapshot_merger.h"
#include "shared_thread_state.h"

using namespace std;
using namespace std::chrono;
using namespace dllama_ns;

void add_nodes_benchmark(int num_nodes) {
	dllama* dllama_instance = new dllama();
	dllama_instance->load_net_graph("simple_graph.net");
	
	sleep(2);
	
	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	for (int i = 0; i < num_nodes; i++) {
		node_t new_node = dllama_instance->add_nodes(100);
		//cout << "rank " << world_rank << " " << new_node << "\n";
	}
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	
	auto duration = duration_cast<microseconds>(t2 - t1).count();
	float nodes_per_second = (1000000*num_nodes);
	nodes_per_second /= duration;
	cout << "rank " << world_rank << " took " << duration/num_nodes << " per node i.e. " << nodes_per_second << "per second\n";

}

void add_edges_benchmark(int num_nodes) {
	dllama* dllama_instance = new dllama();
	dllama_instance->load_net_graph("simple_graph.net");
	
	sleep(2);
	
	dllama_instance->add_nodes(num_nodes);
	
	sleep(2);
	
	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	for (int i = 1; i < num_nodes + 1; i++) {
		dllama_instance->add_edge(i, i - 1);
	}
	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	
	auto duration = duration_cast<microseconds>(t2 - t1).count();
	float nodes_per_second = (1000000*num_nodes);
	nodes_per_second /= duration;
	cout << "rank " << world_rank << " took " << duration/num_nodes << " per node i.e. " << nodes_per_second << "per second\n";
	
	if (world_rank == 1) {
		dllama_instance->add_edge(num_nodes, num_nodes - 2);
	}
	
	dllama_instance->request_checkpoint();
	cout << "about to begin merge\n";
	sleep(10);
	if (world_rank == 1) {
		dllama_instance->start_merge();
	}
	sleep(10);
	/*if (world_rank == 0) {
		for (int i = 1; i < num_nodes + 1; i++) {
			cout << "rank " << world_rank << " node " << i << " out degree: " << dllama_instance->out_degree(i) << "\n";
		}
	}*/
	
	cout << "rank " << world_rank << " node " << num_nodes << " out degree: " << dllama_instance->out_degree(num_nodes) << "\n";
	
	//dllama_instance->delete_db();
	sleep(5);
	dllama_instance->shutdown();
}

void add_large_graph_benchmark(int idk) {
	
}

void read_all_edges_of_random_nodes_benchmark(int idk) {
	
}

//usage: mpirun -n 2 ./dllama.exe 4
int main(int argc, char** argv) {
	
	//select benchmarks
	if (argc == 3) {
		int second_arg = atoi(argv[2]);
		switch (*argv[1]) {
			case '0':
				add_nodes_benchmark(second_arg);
				break;
			case '1':
				add_edges_benchmark(second_arg);
				break;
			case '2':
				add_large_graph_benchmark(second_arg);
				break;
			case '3':
				read_all_edges_of_random_nodes_benchmark(second_arg);
				break;
			default:
				cout << "invalid benchmark number" << "\n";
		}
	}
	sleep(10);
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

