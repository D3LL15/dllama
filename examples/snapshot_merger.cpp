#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <mutex>

#include "snapshot_merger.h"
#include "shared_thread_state.h"

using namespace std;

snapshot_merger::snapshot_merger() {
}

snapshot_merger::~snapshot_merger() {
}

void snapshot_merger::handle_snapshot_message(MPI_Status status) {
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

void snapshot_merger::handle_merge_request(MPI_Status status) {
    //stop main thread writing snapshots
    merge_starting_lock.lock();
    merge_starting = 1;
    merge_starting_lock.unlock();
    //TODO: send latest snapshot file if incomplete, only applies with multiple levels per file
    //TODO: broadcast start merge request
    //TODO: set sender's position in merge request vector to received latest snapshot number (from merge request message)
    //TODO: if vector numbers correspond to currently held latest snapshots (apart from us) then merge, else listen for snapshots until they are equal
    //TODO: set merge request vector to all 0s
    //TODO: reset main thread llama to use new level 0 snapshot, while retaining in memory deltas, then flush deltas to new snapshot

    //allow main thread to continue writing snapshots
    merge_starting_lock.lock();
    merge_starting = 0;
    merge_starting_lock.unlock();
}

void snapshot_merger::start_snapshot_listener() {
    cout << "Rank " << world_rank << " mpi_listener running\n";
    
    received_snapshot_levels = new int[world_size]();
    expected_snapshot_levels = new int[world_size]();
    
    MPI_Status status;
    while (true) {
        MPI_Probe(MPI_ANY_SOURCE, SNAPSHOT_MESSAGE, MPI_COMM_WORLD, &status);
        switch (status.MPI_TAG) {
            case SNAPSHOT_MESSAGE:
                handle_snapshot_message(status);
                break;
            case START_MERGE_REQUEST:
                handle_merge_request(status);
                break;
            default:
                cout << "Rank " << world_rank << " received message with unknown tag\n";
        }
    }
}

void snapshot_merger::read_snapshots() {
    //std::string dir = "db";
    //ll_persistent_storage* storage = new ll_persistent_storage(dir.c_str());
    //std::vector<std::string> csrs = storage->list_context_names("csr");
}