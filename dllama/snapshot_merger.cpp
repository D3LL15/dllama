#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <mutex>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#include "snapshot_merger.h"
#include "shared_thread_state.h"
#include "snapshot_manager.h"
#include "llama/ll_mlcsr_helpers.h"

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
	if (file.is_open()) {
		file.write(memblock + 4, bytes_received - 4);
		file.close();
	} else cout << "Rank " << world_rank << " unable to open output file\n";
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
	if (world_rank == 0) {
		string input_file_name = "db0/csr__out__0.dat";
		ifstream file(input_file_name, ios::in | ios::binary | ios::ate);
		if (file.is_open()) {
			int file_size = file.tellg();
			char* memblock = new char [file_size];
			dll_level_meta* meta;

			file.seekg(0, ios::beg);
			file.read(memblock, file_size);
			file.close();

			meta = (dll_level_meta*) memblock;
			cout << "size of level meta " << sizeof(dll_level_meta) << "\n";
			cout << "level " << meta->lm_level << "\n";
			cout << "header offset " << meta->lm_header_offset << "\n";
			cout << "header size " << meta->lm_header_size << "\n";
			cout << "vt offset " << meta->lm_vt_offset << "\n";
			cout << "vt partitions " << meta->lm_vt_partitions << "\n";
			cout << "vt size " << meta->lm_vt_size << "\n\n";
			
			dll_header_t* header = (dll_header_t*) (memblock + meta->lm_header_offset);
			cout << "et size " << header->h_et_size << "\n";
			
			ll_large_persistent_chunk et_chunk = header->h_et_chunk;
			cout << "et level " << et_chunk.pc_level << "\n";
			cout << "et length " << et_chunk.pc_length << "\n";
			cout << "et offset " << et_chunk.pc_offset << "\n\n";
			
			ll_persistent_chunk* indirection_entry = (ll_persistent_chunk*) (memblock + meta->lm_vt_offset /*+ sizeof(ll_persistent_chunk)*/);
			cout << "vertex table chunk level " << indirection_entry->pc_level << "\n";
			cout << "vertex table chunk length " << indirection_entry->pc_length << "\n";
			cout << "vertex table chunk offset " << indirection_entry->pc_offset << "\n\n";
			
			//NB. the edge list offset is based off the start of the edge table, unlike the rest of the offsets
			ll_mlcsr_core__begin_t* vertex_table_entry = (ll_mlcsr_core__begin_t*) (memblock + indirection_entry->pc_offset);
			cout << "edge list level length " << vertex_table_entry->level_length << "\n";
			cout << "edge list start " << vertex_table_entry->adj_list_start << "\n";
			cout << "vertex degree " << vertex_table_entry->degree << "\n\n";
			
			
			LL_DATA_TYPE* neighbour;
			for (unsigned i = 0; i < vertex_table_entry->level_length; i++) {
				neighbour = (LL_DATA_TYPE*) (memblock + et_chunk.pc_offset + vertex_table_entry->adj_list_start + (i * sizeof(LL_DATA_TYPE)));
				cout << "neighbour " << i << " is " << *neighbour << "\n";
			}
			
			

			delete[] memblock;
		} else cout << "Rank " << world_rank << " unable to open snapshot file\n";
		
		read_second_snapshot();
		
	}
}

void snapshot_merger::read_second_snapshot() {
	string input_file_name = "db0/csr__out__1.dat";
	ifstream file(input_file_name, ios::in | ios::binary | ios::ate);
	if (file.is_open()) {
		int file_size = file.tellg();
		char* memblock = new char [file_size];
		dll_level_meta* meta;

		file.seekg(0, ios::beg);
		file.read(memblock, file_size);
		file.close();

		meta = (dll_level_meta*) memblock;
		cout << "\n\n\nsecond snapshot\n";
		cout << "size of level meta " << sizeof(dll_level_meta) << "\n";
		cout << "level " << meta->lm_level << "\n";
		cout << "header offset " << meta->lm_header_offset << "\n";
		cout << "header size " << meta->lm_header_size << "\n";
		cout << "vt offset " << meta->lm_vt_offset << "\n";
		cout << "vt partitions " << meta->lm_vt_partitions << "\n";
		cout << "vt size " << meta->lm_vt_size << "\n\n";

		dll_header_t* header = (dll_header_t*) (memblock + meta->lm_header_offset);
		cout << "et size " << header->h_et_size << "\n";

		ll_large_persistent_chunk et_chunk = header->h_et_chunk;
		cout << "et level " << et_chunk.pc_level << "\n";
		cout << "et length " << et_chunk.pc_length << "\n";
		cout << "et offset " << et_chunk.pc_offset << "\n\n";

		ll_persistent_chunk* indirection_entry = (ll_persistent_chunk*) (memblock + meta->lm_vt_offset /*+ sizeof(ll_persistent_chunk)*/);
		cout << "vertex table chunk level " << indirection_entry->pc_level << "\n";
		cout << "vertex table chunk length " << indirection_entry->pc_length << "\n";
		cout << "vertex table chunk offset " << indirection_entry->pc_offset << "\n\n";

		//NB. the edge list offset is based off the start of the edge table, unlike the rest of the offsets
		ll_mlcsr_core__begin_t* vertex_table_entry = (ll_mlcsr_core__begin_t*) (memblock + indirection_entry->pc_offset + sizeof(ll_mlcsr_core__begin_t));
		cout << "edge list level length " << vertex_table_entry->level_length << "\n";
		cout << "edge list start " << LL_EDGE_INDEX(vertex_table_entry->adj_list_start) << "\n";
		cout << "vertex degree " << vertex_table_entry->degree << "\n\n";


		LL_DATA_TYPE* neighbour;
		for (unsigned i = 0; i < vertex_table_entry->level_length; i++) {
			neighbour = (LL_DATA_TYPE*) (memblock + et_chunk.pc_offset + LL_EDGE_INDEX(vertex_table_entry->adj_list_start) + (i * sizeof(LL_DATA_TYPE)));
			cout << "neighbour " << i << " is " << *neighbour << "\n";
		}
		
		cout << "ll_mlcsr_core__begin_t size " << sizeof(ll_mlcsr_core__begin_t) << "\n";
		ll_mlcsr_core__begin_t* continuation = (ll_mlcsr_core__begin_t*) (memblock + et_chunk.pc_offset + LL_EDGE_INDEX(vertex_table_entry->adj_list_start) + (vertex_table_entry->level_length * sizeof(LL_DATA_TYPE)));
		cout << "continuation start " << continuation->adj_list_start << "\n";
		cout << "continuation degree " << continuation->degree << "\n";
		cout << "continuation level length " << continuation->level_length << "\n";
		
		delete[] memblock;
	} else cout << "Rank " << world_rank << " unable to open snapshot file\n";
}

void snapshot_merger::merge_snapshots() {
	int num_vertices = 4;
	
	string output_file_name = "new_level0.dat";

	ofstream file(output_file_name, ios::out | ios::binary | ios::trunc);
	if (file.is_open()) {
		//metadata
		dll_level_meta new_meta;
		new_meta.lm_level = 0;
		new_meta.lm_sub_level = 0;
		new_meta.lm_header_size = 32;
		new_meta.lm_base_level = 0;
		//TODO: must be finished later
		
		//edge table
		int rank_snapshots[2] = {2, 2}; //TODO: this cannot be hardcoded
		
		snapshot_manager snapshots(rank_snapshots);
		
		for (int vertex = 0; vertex < num_vertices; vertex++) {
			vector<node_t> neighbours;
			for (int r = 0; r < world_size; r++) {
				//add all neighbours in edge table pointed to by chunk
				vector<node_t> new_neighbours = snapshots.get_neighbours_of_vertex(r, vertex);
				neighbours.insert(neighbours.end(), new_neighbours.begin(), new_neighbours.end());
			}
			//add edges from level 0
		}
		
		//header
		
		//vertex table
		
		file.close();
	} else cout << "Rank " << world_rank << " unable to open output file\n";
}