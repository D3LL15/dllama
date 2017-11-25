#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <time.h>
#include <mpi.h>

using namespace std;

int main(int argc, char** argv) {
  MPI_Init(NULL, NULL);
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

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
      MPI_Send(memblock, size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);

      delete[] memblock;
    }
    else cout << "Unable to open file\n";
  } else {
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
  
  MPI_Finalize();
  return 0;
}
