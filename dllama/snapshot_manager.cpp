#include "snapshot_manager.h"
#include "shared_thread_state.h"

#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>

snapshot_manager::snapshot_manager(int* rank_snapshots) {
	rank_num_snapshots = rank_snapshots;
	snapshots = new char** [world_size];
	for (int r = 0; r < world_size; r++) {
		snapshots[r] = new char* [rank_snapshots[r]];
		for (int f = 0; f < rank_snapshots[r]; f++) {
			ostringstream oss;
			/*if (r == world_rank) {
				oss << "db" << world_rank << "/csr__out__" << f+1 << ".dat";
			} else {
				oss << "db" << world_rank << "/rank" << r << "/csr__out__" << f+1 << ".dat";
			}*/
			oss << "db" << world_rank << "/csr__out__" << 0 << ".dat"; //TODO: switch around comment
			string input_file_name = oss.str().c_str();
			ifstream file(input_file_name, ios::in | ios::binary | ios::ate);
			if (file.is_open()) {
				int file_size = file.tellg();
				cout << "file size " << file_size << "\n";
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
		for (int f = 0; f < rank_num_snapshots[r]; f++) {
			delete[] snapshots[r][f];
		}
	}
}

ll_persistent_chunk* snapshot_manager::get_ll_persistent_chunk(int rank, int level, int offset) {
	return (ll_persistent_chunk*) (snapshots[rank][level - 1] + offset);
}

dll_level_meta* snapshot_manager::get_dll_level_meta(int rank, int level, int offset) {
	return (dll_level_meta*) (snapshots[rank][level - 1] + offset);
}

vector<node_t> snapshot_manager::get_neighbours_of_vertex(int rank, node_t vertex) {
	int latest_snapshot_number = rank_num_snapshots[rank];
	vector<node_t> neighbours;
	if (latest_snapshot_number != 0) {
		dll_level_meta* meta = get_dll_level_meta(rank, latest_snapshot_number, 0);
	
		int vt_offset = meta->lm_vt_offset;
		cout << "vt offset " << vt_offset << "\n";
		int page_number = vertex/LL_ENTRIES_PER_PAGE;
		cout << "page number " << page_number << "\n";
		int indirection_offset = page_number*sizeof(ll_persistent_chunk);
		ll_persistent_chunk* indirection_entry = (ll_persistent_chunk*) (snapshots[rank][latest_snapshot_number - 1] + vt_offset + indirection_offset);

		int page_level = indirection_entry->pc_level;
		cout << "vertex table chunk level " << page_level << "\n";
		cout << "vertex table chunk length " << indirection_entry->pc_length << "\n";
		cout << "vertex table chunk offset " << indirection_entry->pc_offset << "\n";
		if (page_level == 0) {
			//continue; //to avoid repeatedly adding level 0
		} 
		cout << "snapshots[rank][page_level - 1]" << snapshots[rank][0] << "\n"; //snapshots[rank][page_level - 1]
		
		//vertex 0
		//size_t edges_offset = (size_t) (snapshots[rank][page_level - 1] + vt_offset + indirection_entry->pc_offset);
		//size_t* edge1 = (size_t*) (snapshots[rank][page_level - 1] + indirection_entry->pc_offset);
		//dll_header_t* header = get_dll_header(rank, latest_snapshot_number, )
		//cout << "edges offset " << edges_offset << "\n";
		//cout << "edge1 " << *edge1 << "\n";
		//TODO: get neighbours
	}
	
	return neighbours;
}