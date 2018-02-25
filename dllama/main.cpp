#include <thread>
#include <iostream>
#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>
#include <stdlib.h>
#include <chrono>
#include <unordered_set>
#include <queue>

#include "dllama_test.h"
#include "dllama.h"
#include "snapshot_merger.h"
#include "shared_thread_state.h"

using namespace std;
using namespace std::chrono;
using namespace dllama_ns;

string database_location;

//100,000 nodes
void add_nodes_benchmark(int num_nodes, int num_iterations) {
	if (world_rank == 0) {
		cout << BENCHMARK_TYPE << "add_nodes " << world_size << " " << num_nodes << " ";
	}
	MPI_Barrier(MPI_COMM_WORLD);
	
	for (int j = 0; j < num_iterations; j++) {
		dllama* dllama_instance = new dllama(database_location, false);
		dllama_instance->load_net_graph("empty_graph.net");
		if (world_rank == 0) {
			high_resolution_clock::time_point t1 = high_resolution_clock::now();
			node_t new_node = dllama_instance->add_nodes(num_nodes);
			dllama_instance->request_checkpoint();
			high_resolution_clock::time_point t2 = high_resolution_clock::now();

			auto duration = duration_cast<microseconds>(t2 - t1).count();
			cout << duration << " ";
		}
		MPI_Barrier(MPI_COMM_WORLD);
		dllama_instance->delete_db();
		dllama_instance->shutdown();
		delete dllama_instance;
		
		//dllama_instance->load_net_graph("empty_graph.net");
		MPI_Barrier(MPI_COMM_WORLD);
	}
	if (world_rank == 0) {
		cout << "\n";
	}
	//dllama_instance->shutdown();
}

//10000 nodes
void add_edges_benchmark(int num_nodes, int num_iterations) {
	MPI_Barrier(MPI_COMM_WORLD);
	
	if (world_rank == 0) {
		cout << BENCHMARK_TYPE << "add_edges " << world_size << " " << num_nodes << " ";
	}
	
	for (int j = 0; j < num_iterations; j++) {
		MPI_Barrier(MPI_COMM_WORLD);
		dllama* my_dllama_instance = new dllama(database_location, false);
		my_dllama_instance->load_net_graph("empty_graph.net");
		if (world_rank == 0) {
			my_dllama_instance->add_nodes(num_nodes);
			my_dllama_instance->request_checkpoint();
			high_resolution_clock::time_point t1 = high_resolution_clock::now();
			for (int i = 1; i <= num_nodes; i++) {
				for (int k = 0; k < 100; k++) {
					my_dllama_instance->add_edge(i, k);
				}
			}
			my_dllama_instance->request_checkpoint();
			high_resolution_clock::time_point t2 = high_resolution_clock::now();
			auto duration = duration_cast<microseconds>(t2 - t1).count();
			cout << duration << " ";
		}
		MPI_Barrier(MPI_COMM_WORLD);
		my_dllama_instance->delete_db();
		my_dllama_instance->shutdown();
		delete my_dllama_instance;
		
	}
	if (world_rank == 0) {
		cout << "\n";
	}
	
}

void read_edges_benchmark(int num_nodes, int num_iterations) {
	MPI_Barrier(MPI_COMM_WORLD);
	
	if (world_rank == 0) {
		cout << BENCHMARK_TYPE << "read_edges " << world_size << " " << num_nodes << " ";
	}
	
	for (int j = 0; j < num_iterations; j++) {
		MPI_Barrier(MPI_COMM_WORLD);
		dllama* my_dllama_instance = new dllama(database_location, false);
		my_dllama_instance->load_net_graph("empty_graph.net");
		if (world_rank == 0) {
			my_dllama_instance->add_nodes(num_nodes);
			my_dllama_instance->request_checkpoint();
			for (int i = 1; i <= num_nodes; i++) {
				for (int k = 0; k < 100; k++) {
					my_dllama_instance->add_edge(i, k);
				}
			}
			my_dllama_instance->request_checkpoint();
			high_resolution_clock::time_point t1 = high_resolution_clock::now();
			
			for (int i = 1; i <= num_nodes; i++) {
				my_dllama_instance->get_neighbours_of_vertex(i);
			}
			
			high_resolution_clock::time_point t2 = high_resolution_clock::now();
			auto duration = duration_cast<microseconds>(t2 - t1).count();
			cout << duration << " ";
		}
		MPI_Barrier(MPI_COMM_WORLD);
		my_dllama_instance->delete_db();
		my_dllama_instance->shutdown();
		delete my_dllama_instance;
		
	}
	if (world_rank == 0) {
		cout << "\n";
	}
	
}

//10000 nodes
void merge_benchmark(int num_nodes, int num_iterations) {
	MPI_Barrier(MPI_COMM_WORLD);
	
	for (int k = 0; k < 10; k++) {
		int num_nodes_this_iter = num_nodes - k*(num_nodes/10);
		if (world_rank == 0) {
			cout << BENCHMARK_TYPE << "merge_benchmark " << world_size << " " << num_nodes_this_iter << " ";
		}

		for (int j = 0; j < num_iterations; j++) {
			dllama* my_dllama_instance = new dllama(database_location, false);
			my_dllama_instance->load_net_graph("empty_graph.net");
			MPI_Barrier(MPI_COMM_WORLD);
			if (world_rank == 0) {
				my_dllama_instance->add_nodes(num_nodes_this_iter);
				for (int z = 0; z < 10; z++) {
					for (int i = 1; i <= num_nodes_this_iter; i++) {
						for (int k = 100*z; k < 100*(z+1); k++) {
							my_dllama_instance->add_edge(i, k);
						}
					}
					my_dllama_instance->request_checkpoint();
				}
				my_dllama_instance->start_merge();
			}
			sleep(10);

			MPI_Barrier(MPI_COMM_WORLD);
			my_dllama_instance->delete_db();
			my_dllama_instance->shutdown();
			delete my_dllama_instance;
		}
		if (world_rank == 0) {
			cout << "\n";
		}
	}
	
	
}

void add_and_read_graph(string input_file, int num_nodes, int num_iterations) {
	/*if (world_rank == 0) {
		cout << "NB: interleaved results (time to load then time to read for each iteration)\n";
	}*/
	MPI_Barrier(MPI_COMM_WORLD);

	for (int j = 0; j < num_iterations; j++) {
		dllama* my_dllama_instance = new dllama(database_location, false);
		my_dllama_instance->load_net_graph("empty_graph.net");
		MPI_Barrier(MPI_COMM_WORLD);
		if (world_rank == 0) {
			ifstream file(input_file);
			if (!file.is_open()) {
				cout << "cannot open graph net file\n";
				return;
			}
			high_resolution_clock::time_point t1 = high_resolution_clock::now();
			my_dllama_instance->add_nodes(num_nodes);
			string line;
			int from;
			int to;
			while (getline(file, line)) {
				sscanf(line.c_str(), "%d	%d", &from, &to);
				my_dllama_instance->add_edge(from, to);
			}
			my_dllama_instance->request_checkpoint();
			//my_dllama_instance->load_net_graph("kronecker_graph.net");
			
			high_resolution_clock::time_point t2 = high_resolution_clock::now();
			auto duration = duration_cast<microseconds>(t2 - t1).count();
			//cout << duration << ",";
			
			t1 = high_resolution_clock::now();
			for (int i = 0; i < num_nodes; i++) {
				my_dllama_instance->get_neighbours_of_vertex(i);
			}
			t2 = high_resolution_clock::now();
			duration = duration_cast<microseconds>(t2 - t1).count();
			cout << duration << " ";
			file.close();
		}
		MPI_Barrier(MPI_COMM_WORLD);
		my_dllama_instance->delete_db();
		my_dllama_instance->shutdown();
		delete my_dllama_instance;
	}
	if (world_rank == 0) {
		cout << "\n";
	}
	
	//my_dllama_instance->shutdown();
}

void add_and_read_kronecker_graph(int num_iterations) {
	if (world_rank == 0) {
		cout << BENCHMARK_TYPE <<"kronecker " << world_size << " 1024 ";
	}
	add_and_read_graph("kronecker_graph.net", 1024, num_iterations);
}

void add_and_read_power_graph(int num_iterations) {
	if (world_rank == 0) {
		cout << BENCHMARK_TYPE <<"power " << world_size << " 1000 ";
	}
	add_and_read_graph("powerlaw.net", 1000, num_iterations);
}

void add_and_read_kronecker_graph2(int num_iterations) {
	if (world_rank == 0) {
		cout << BENCHMARK_TYPE <<"large_kronecker " << world_size << " 131072 ";
	}
	add_and_read_graph("krongraph2.net", 131072, num_iterations);
}

void add_and_read_power_graph2(int num_iterations) {
	if (world_rank == 0) {
		cout << BENCHMARK_TYPE <<"large_power " << world_size << " 50000 ";
	}
	add_and_read_graph("powerlaw2.net", 50000, num_iterations);
}

void breadth_first_search(int num_iterations) {
	int num_nodes = 1024;
	string input_file = "kronecker_graph.net";
	dllama* my_dllama_instance = new dllama(database_location, false);
	my_dllama_instance->load_net_graph("empty_graph.net");
	MPI_Barrier(MPI_COMM_WORLD);
	if (world_rank == 0) {
		cout << BENCHMARK_TYPE << "breadth_first " << world_size << " 1024 ";
		ifstream file(input_file);
		if (!file.is_open()) {
			cout << "cannot open graph net file\n";
			return;
		}
		my_dllama_instance->add_nodes(num_nodes);
		string line;
		int from;
		int to;
		while (getline(file, line)) {
			sscanf(line.c_str(), "%d	%d", &from, &to);
			my_dllama_instance->add_edge(from, to);
		}
		file.close();
		my_dllama_instance->request_checkpoint();
	}
	MPI_Barrier(MPI_COMM_WORLD);

	node_t end_id = 692;
	for (int j = 0; j < num_iterations*5; j++) {
		if (world_rank == 0) {
			high_resolution_clock::time_point t1 = high_resolution_clock::now();
			
			for (int i = 0; i < num_nodes; i++) {
				my_dllama_instance->get_neighbours_of_vertex(i);
			}
			queue<node_t> next_nodes;
			unordered_set<node_t> seen_nodes;
			
			node_t n = 0;
			seen_nodes.insert(n);
			
			bool found_node = false;
			while (!found_node) {
				vector<node_t> neighbours = my_dllama_instance->get_neighbours_of_vertex(n);
				for (int i = 0; i < neighbours.size(); i++) {
					node_t destination = neighbours.at(i);
					if (destination == (end_id + j)) {
						found_node = true;
						break;
					}
					if (seen_nodes.find(destination) == seen_nodes.end()) {
						next_nodes.push(destination);
						seen_nodes.insert(destination);
					}
				}
				
				if (next_nodes.empty()) {
					break;
				}

				n = next_nodes.front();
				next_nodes.pop();
			}
			
			high_resolution_clock::time_point t2 = high_resolution_clock::now();
			auto duration = duration_cast<microseconds>(t2 - t1).count();
			cout << duration << " ";
			
		}
		MPI_Barrier(MPI_COMM_WORLD);
		
	}
	if (world_rank == 0) {
		cout << "\n";
	}
	my_dllama_instance->delete_db();
	my_dllama_instance->shutdown();
	delete my_dllama_instance;
}

//usage: mpirun -n 2 ./dllama.exe 4 10000 10 database/
int main(int argc, char** argv) {
	int p = 0;
	int *provided;
	provided = &p;
	MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, provided);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	if (*provided != MPI_THREAD_MULTIPLE) {
		cout << "ERROR: MPI implementation doesn't provide multi-thread support\n";
	}
	//select benchmarks
	if (world_rank == 0) {
		cout << "started benchmark\n";
	}
    if (argc == 5) {
		int second_arg = atoi(argv[2]);
		int third_arg = atoi(argv[3]);
		database_location = argv[4];
		switch (*argv[1]) {
			case '0':
				add_nodes_benchmark(second_arg, third_arg);
				break;
			case '1':
				add_edges_benchmark(second_arg, third_arg);
				break;
			case '2':
				merge_benchmark(second_arg, third_arg);
				break;
			case '3':
				add_and_read_kronecker_graph(third_arg);
				break;
			case '4':
				add_and_read_power_graph(third_arg);
				break;
			case '5':
				//simple dllama benchmark
				add_nodes_benchmark(second_arg*10, third_arg);
				add_edges_benchmark(second_arg, third_arg);
				//read_edges_benchmark(second_arg, third_arg);
				//merge_benchmark(second_arg, third_arg);
				add_and_read_kronecker_graph(third_arg);
				add_and_read_power_graph(third_arg);
				break;
			case '6':
				//dllama benchmark
				add_nodes_benchmark(second_arg*10, third_arg);
				add_edges_benchmark(second_arg, third_arg);
				//read_edges_benchmark(second_arg, third_arg);
				//merge_benchmark(second_arg, third_arg);
				add_and_read_kronecker_graph(third_arg);
				add_and_read_power_graph(third_arg);
				add_and_read_kronecker_graph2(third_arg);
				add_and_read_power_graph2(third_arg);
				break;
			case '7':
				read_edges_benchmark(second_arg, third_arg);
				break;
			case '8':
				add_and_read_kronecker_graph2(third_arg);
				break;
			case '9':
				add_and_read_power_graph2(third_arg);
				break;
			case 'a':
				breadth_first_search(third_arg);
				break;
			case 'b':
				read_edges_benchmark(second_arg, third_arg);
				merge_benchmark(second_arg, third_arg);
				breadth_first_search(third_arg);
                add_and_read_power_graph(third_arg);
                add_and_read_kronecker_graph(third_arg);
				break;
            case 'c':
                add_nodes_benchmark(second_arg*10, third_arg);
                add_edges_benchmark(second_arg, third_arg);
                break;
			default:
				cout << "provide benchmark number, number of nodes, number of iterations, location to store database" << "\n";
		}
	}
	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();
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

