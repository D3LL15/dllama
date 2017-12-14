#include <thread>
#include <iostream>
#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <mutex>

#include "dllama_test.h"
#include "dllama.h"
#include "snapshot_merger.h"
#include "shared_thread_state.h"

using namespace std;

int world_size;
int world_rank;
bool merge_starting;
mutex merge_starting_lock;
int current_snapshot_level;

void start_mpi_listener() {
	snapshot_merger sm = snapshot_merger();
	sm.start_snapshot_listener();
}

//usage: mpirun -n 2 ./dllama.exe 4

int main(int argc, char** argv) {
	//initialise MPI
	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	merge_starting = 0;
	current_snapshot_level = 0;

	//start MPI listener thread
	thread mpi_listener(start_mpi_listener);
	cout << "Rank " << world_rank << " main and mpi_listener threads now execute concurrently...\n";

	if (argc == 2) {
		dllama_test test_instance = dllama_test();
		dllama dllama_instance = dllama();
		snapshot_merger sm = snapshot_merger();
		switch (*argv[1]) {
			case '1':
				test_instance.full_test();
				break;
			case '2':
				test_instance.test_llama_init();
				break;
			case '3':
				test_instance.test_llama_print_neighbours();
				break;
			case '4':
				dllama_instance.load_net_graph("/home/dan/NetBeansProjects/Part2Project/graph.net");
				break;
			case '5':
				if (world_rank == 0) {
					streampos file_size;
					char * memblock;
					uint32_t file_number = 5;

					ifstream file("db/csr__out__0.dat", ios::in | ios::binary | ios::ate);
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
					} else cout << "Rank " << world_rank << " unable to open input file\n";
				}
				break;
			case '6':
				dllama_instance.add_random_edge();
				dllama_instance.auto_checkpoint();
				break;
			case '7':
				dllama_instance.load_net_graph("simple_graph.net");
				break;
			case '8':
				sm.read_snapshots();
				break;
			case '9':
				if (world_rank == 0) {
					sm.merge_snapshots();
				}
				break;
			case 'a':
				dllama_instance.load_net_graph("simple_graph.net");
				dllama_instance.add_edge(1, 0);
				dllama_instance.add_edge(2, 1);
				dllama_instance.auto_checkpoint();
				dllama_instance.add_edge(2, 0);
				dllama_instance.auto_checkpoint();
				sm.read_snapshots();
				break;
		}
	}

	mpi_listener.join();
	cout << "Rank " << world_rank << " mpi_listener thread terminated.\n";
	MPI_Finalize();
	return 0;
}

