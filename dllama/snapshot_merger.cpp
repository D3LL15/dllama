#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <mutex>
#include <sys/stat.h>
#include <sys/types.h>

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
    MPI_Recv(memblock, bytes_received, MPI_BYTE, status.MPI_SOURCE, SNAPSHOT_MESSAGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    cout << "Rank " << world_rank << " received file\n";

    uint32_t file_number = 0;
    file_number += memblock[0] << 24;
    file_number += memblock[1] << 16;
    file_number += memblock[2] << 8;
    file_number += memblock[3];
    cout << "Rank " << world_rank << " file number: " << file_number << "\n";

    //we can get away with this for single level files
    received_snapshot_levels[status.MPI_SOURCE] = file_number;
    
    ostringstream oss;
    oss << "db" << world_rank << "/rank" << status.MPI_SOURCE;
    //create receipt directory if it doesn't already exist
    mkdir(oss.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH);
    oss << "/csr__out__" << file_number << ".dat";
    
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
    bool merge_had_started = merge_starting;
    merge_starting = 1;
    merge_starting_lock.unlock();
    int expected_level;
    MPI_Recv(&expected_level, 1, MPI_INT, status.MPI_SOURCE, START_MERGE_REQUEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    
    //TODO: send latest snapshot file if incomplete (only applies with multiple levels per file) may also require modifying snapshot level vectors
    
    //broadcast start merge request if this is the first merge request you have heard
    if (!merge_had_started) {
        for (int i = 0; i < world_size; i++) {
            if (i != world_rank) {
                MPI_Send(&current_snapshot_level, 1, MPI_INT, i, START_MERGE_REQUEST, MPI_COMM_WORLD);
            }
        }
    }
    
    //set sender's value in expected snapshot vector to the snapshot number they sent in merge request (the last snapshot they sent)
    expected_snapshot_levels[status.MPI_SOURCE] = expected_level;
    //if expected snapshot numbers correspond to currently held latest snapshots (the last snapshots we received from those hosts) then merge, else listen for snapshots until they are equal
    bool received_all_snapshots = 1;
    for (int i = 0; i < world_size; i++) {
        if (received_snapshot_levels[i] != expected_snapshot_levels[i]) {
            received_all_snapshots = 0;
        }
    }
    if (received_all_snapshots) {
        cout << "Rank " << world_rank << " received merge requests from all other hosts\n";
        
        //TODO: merge
        
        //TODO: reset main thread llama to use new level 0 snapshot, while retaining in memory deltas, then flush deltas to new snapshot
        
        //clean up after merge
        //setting expected snapshot level to -1 means we have to hear from that host before merging
        for (int i = 0; i < world_size; i++) {
            received_snapshot_levels[i] = 0;
            expected_snapshot_levels[i] = -1;
        }
        //don't need to hear from yourself
        expected_snapshot_levels[world_rank] = 0;
        
        //allow main thread to continue writing snapshots
        merge_starting_lock.lock();
        merge_starting = 0;
        merge_starting_lock.unlock();
    }
}

void snapshot_merger::start_snapshot_listener() {
    cout << "Rank " << world_rank << " mpi_listener running\n";
    
    received_snapshot_levels = new int[world_size]();
    expected_snapshot_levels = new int[world_size];
    for (int i = 0; i < world_size; i++) {
        expected_snapshot_levels[i] = -1;
    }
    
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