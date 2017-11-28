#include <thread>
#include <iostream>
#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>

#include "dllama_test.h"
#include "dllama.h"
#include "snapshot_merger.h"
#include "shared_thread_state.h"

using namespace std;

int world_size;
int world_rank;

void handle_snapshot_message(MPI_Status status) {
    int bytes_received;
    MPI_Get_count(&status, MPI_BYTE, &bytes_received);
    cout << "Rank " << world_rank << " number of bytes being received: " << bytes_received << "\n";
    char* memblock = new char [bytes_received];
    MPI_Recv(memblock, bytes_received, MPI_BYTE, MPI_ANY_SOURCE, SNAPSHOT_MESSAGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    cout << "Rank " << world_rank << " received file\n";

    uint32_t file_number = 0;
    file_number += memblock[0] << 24;
    file_number += memblock[1] << 16;
    file_number += memblock[2] << 8;
    file_number += memblock[3];
    cout << "Rank " << world_rank << " file number: " << file_number << "\n";

    ostringstream oss;
    oss << "db" << world_rank << "/rank" << status.MPI_SOURCE << "/csr__out__" << file_number << ".dat";
    string output_file_name = oss.str();

    ofstream file(output_file_name, ios::out | ios::binary | ios::trunc);
    if (file.is_open())
    {
        file.write(memblock + 4, bytes_received - 4);
        file.close();
    }
    else cout << "Rank " << world_rank << " unable to open output file\n";
    delete[] memblock;
}

void start_mpi_listener() {
    cout << "Rank " << world_rank << " mpi_listener running\n";
    MPI_Status status;
    while (true) {
        MPI_Probe(MPI_ANY_SOURCE, SNAPSHOT_MESSAGE, MPI_COMM_WORLD, &status);
        switch (status.MPI_TAG) {
            case SNAPSHOT_MESSAGE:
                handle_snapshot_message(status);
                break;
            case START_MERGE_REQUEST:
                //TODO: alert main thread to stop writing snapshots
                //TODO: send latest snapshot file if incomplete
                //TODO: broadcast start merge request (if not waiting on acks for snapshot broadcasts?)
                //TODO: set sender's position in merge request vector to 1
                //TODO: if vector all 1 (apart from us) then merge
                //TODO: set merge request vector to all 0s
                //TODO: allow main thread to continue writing snapshots
                break;
            default:
                cout << "Rank " << world_rank << " received message with unknown tag\n";
        }
    }
}

//usage: mpirun -n 2 ./dllama.exe 4
int main(int argc, char** argv) {
    //initialise MPI
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    
    //start MPI listener thread
    thread mpi_listener (start_mpi_listener);
    cout << "Rank " << world_rank << " main and mpi_listener threads now execute concurrently...\n";

    if (argc == 2) {
        dllama_test x = dllama_test();
        dllama y = dllama();
        switch (*argv[1]) {
            case '1':
                x.full_test();
                break;
            case '2':
                x.test_llama_init();
                break;
            case '3':
                x.test_llama_print_neighbours();
                break;
            case '4':
                y.load_SNAP_graph();
                break;
            case '5':
                if (world_rank == 0) {
                    streampos file_size;
                    char * memblock;
                    uint32_t file_number = 5;

                    ifstream file ("db/csr__out__0.dat", ios::in|ios::binary|ios::ate);
                    if (file.is_open())
                    {
                        file_size = file.tellg();
                        int memblock_size = file_size;
                        memblock_size += 4;
                        memblock = new char [memblock_size];
                        memblock[0] = (file_number >> 24) & 0xFF;
                        memblock[1] = (file_number >> 16) & 0xFF;
                        memblock[2] = (file_number >> 8) & 0xFF;
                        memblock[3] = file_number & 0xFF;
                        file.seekg (0, ios::beg);
                        file.read (memblock + 4, memblock_size);
                        file.close();

                        for (int i = 0; i < world_size; i++) {
                            if (i != world_rank) {
                                MPI_Send(memblock, memblock_size, MPI_BYTE, i, SNAPSHOT_MESSAGE, MPI_COMM_WORLD);
                            }
                        }

                        delete[] memblock;
                    }
                    else cout << "Rank " << world_rank << " unable to open input file\n";
                }
                break;
            case '6':
                y.add_random_edge();
                y.auto_checkpoint();
                break;
        }
    }
    
    mpi_listener.join();
    cout << "Rank " << world_rank << " mpi_listener thread terminated.\n";
    MPI_Finalize();
    return 0;
}

