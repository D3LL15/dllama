#include "snapshot_manager.h"
#include "shared_thread_state.h"

#include <fstream>
#include <string>
#include <sstream>
#include <iostream>

snapshot_manager::snapshot_manager(int* rank_snapshots) {
	rank_num_snapshots = rank_snapshots;
	snapshots = new (char**)[world_size];
	
	for (int r = 0; r < world_size; r++) {
		snapshots[r] = new (char*)[rank_snapshots[r]];
		
		for (int f = 1; f < rank_snapshots[r]; f++) {
			ostringstream oss;
			if (r == world_rank) {
				oss << "csr__out__" << f << ".dat";
			} else {
				oss << "db" << world_rank << "/rank" << r << "/csr_out__" << f << ".dat";
			}

			string input_file_name = oss.str().c_str();
			ifstream file(input_file_name, ios::in | ios::binary | ios::ate);
			if (file.is_open()) {
				int file_size = file.tellg();
				snapshots[r][f] = new char [file_size];

				file.seekg(0, ios::beg);
				file.read(snapshots[r][f], file_size);
				file.close();

			} else cout << "Rank " << world_rank << " snapshot manager unable to open snapshot file\n";
		}
		
	}
}

snapshot_manager::~snapshot_manager() {
	for (int r = 0; r < world_size; r++) {
		for (int f = 1; f < rank_num_snapshots; f++) {
			delete[] snapshots[r][f];
		}
	}
}

ll_persistent_chunk* snapshot_manager::get_ll_persistent_chunk(int rank, int level, int offset) {
	return (ll_persistent_chunk*) (snapshots[rank][level] + offset);
}

dll_level_meta* snapshot_manager::get_dll_level_meta(int rank, int level, int offset) {
	return (dll_level_meta*) (snapshots[rank][level] + offset);
}