#include "dllama_test.h"
#include "dllama.h"
#include "snapshot_merger.h"
#include <thread>
#include <iostream>
#include <mpi.h>
#include <fstream>

using namespace std;

int world_size;
int world_rank;

void mpi_listener() {
    cout << "mpi_listener running\n";
    if (world_rank == 1) {
        MPI_Status status;
        MPI_Probe(0, 0, MPI_COMM_WORLD, &status);
        int number_amount;
        MPI_Get_count(&status, MPI_BYTE, &number_amount);
        cout << "number being received: " << number_amount << "\n";
        char* memblock = new char [number_amount];
        MPI_Recv(memblock, number_amount, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        cout << "received file\n";

        delete[] memblock;
    }
}

int main(int argc, char** argv) {
    //initialise MPI
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    
    thread first (mpi_listener);
    cout << "main, mpi_listener now execute concurrently...\n";
    
    
    if (world_rank == 0) {
        streampos size;
        char * memblock;

        ifstream file ("/home/dan/project/current/dllama/examples/db/csr__out__0.dat", ios::in|ios::binary|ios::ate);
        if (file.is_open())
        {
            size = file.tellg();
            memblock = new char [size];
            file.seekg (0, ios::beg);
            file.read (memblock, size);
            file.close();

            cout << "the entire file content is in memory\n";
            for (int i = 0; i < world_size; i++) {
                if (i != world_rank) {
                    MPI_Send(memblock, size, MPI_BYTE, i, 0, MPI_COMM_WORLD);
                }
            }

            delete[] memblock;
        }
        else cout << "Unable to open file\n";
    } else {
        /*MPI_Status status;
        MPI_Probe(0, 0, MPI_COMM_WORLD, &status);
        int number_amount;
        MPI_Get_count(&status, MPI_BYTE, &number_amount);
        cout << "number being received: " << number_amount << "\n";
        char* memblock = new char [number_amount];
        MPI_Recv(memblock, number_amount, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        cout << "received file\n";

        delete[] memblock;*/
    }

    first.join();
    cout << "mpi_listener completed.\n";
    MPI_Finalize();
    
    if (argc == 2) {
        dllama_test x = dllama_test();
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
                dllama y = dllama();
                //y.load_SNAP_graph();
                break;
        }
    }
    return 0;
}

