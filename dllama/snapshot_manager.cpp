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
			if (r == world_rank) {
				oss << "db" << world_rank << "/csr__out__" << f+1 << ".dat";
			} else {
				oss << "db" << world_rank << "/rank" << r << "/csr__out__" << f+1 << ".dat";
			}
			//oss << "db" << world_rank << "/csr__out__" << 0 << ".dat";
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
	ostringstream oss;
	oss << "db" << world_rank << "/csr__out__" << 0 << ".dat"; //TODO: switch around comment
	string input_file_name = oss.str().c_str();
	ifstream file(input_file_name, ios::in | ios::binary | ios::ate);
	if (file.is_open()) {
		int file_size = file.tellg();
		cout << "file size " << file_size << "\n";
		level_0_snapshot = new char [file_size];
		file.seekg(0, ios::beg);
		file.read(level_0_snapshot, file_size);
		file.close();
	} else cout << "Rank " << world_rank << " snapshot manager unable to open snapshot file\n";
}

snapshot_manager::~snapshot_manager() {
	for (int r = 0; r < world_size; r++) {
		for (int f = 0; f < rank_num_snapshots[r]; f++) {
			delete[] snapshots[r][f];
		}
		delete[] snapshots[r];
	}
	delete[] snapshots;
	delete[] level_0_snapshot;
}

ll_persistent_chunk* snapshot_manager::get_ll_persistent_chunk(int rank, int level, int offset) {
	return (ll_persistent_chunk*) (snapshots[rank][level - 1] + offset);
}

dll_level_meta* snapshot_manager::get_dll_level_meta(int rank, int level) {
	return (dll_level_meta*) (snapshots[rank][level - 1]);
}

ll_mlcsr_core__begin_t* snapshot_manager::get_vertex_table_entry(int rank, int level, int offset) {
	return (ll_mlcsr_core__begin_t*) (snapshots[rank][level - 1] + offset);
}

LL_DATA_TYPE* snapshot_manager::get_edge_table_entry(int rank, int level, int offset) {
	return (LL_DATA_TYPE*) (snapshots[rank][level - 1] + offset);
}

dll_header_t* snapshot_manager::get_header(int rank, int level, int offset) {
	return (dll_header_t*) (snapshots[rank][level - 1] + offset);
}

vector<node_t> snapshot_manager::get_neighbours_of_vertex(int rank, node_t vertex) {
	cout << "\n\ngetting neighbours of vertex " << vertex << "\n";
	int latest_snapshot_number = rank_num_snapshots[rank];
	vector<node_t> neighbours;
	if (latest_snapshot_number != 0) {
		dll_level_meta* meta = get_dll_level_meta(rank, latest_snapshot_number);
	
		int vt_offset = meta->lm_vt_offset;
		cout << "vt offset " << vt_offset << "\n";
		
		dll_header_t* header = get_header(rank, latest_snapshot_number, meta->lm_header_offset);
		ll_large_persistent_chunk et_chunk = header->h_et_chunk;
		
		int page_number = vertex/LL_ENTRIES_PER_PAGE;
		cout << "page number " << page_number << "\n";
		int indirection_offset = page_number*sizeof(ll_persistent_chunk);
		ll_persistent_chunk* indirection_entry = get_ll_persistent_chunk(rank, latest_snapshot_number, vt_offset + indirection_offset);

		unsigned page_level = indirection_entry->pc_level;
		cout << "vertex table chunk level " << page_level << "\n";
		cout << "vertex table chunk length " << indirection_entry->pc_length << "\n";
		cout << "vertex table chunk offset " << indirection_entry->pc_offset << "\n";
		if (page_level == 0) {
			//continue; //to avoid repeatedly adding level 0
			cout << "vertex chunk is in level 0\n";
		} else {
			int vertex_offset = vertex%LL_ENTRIES_PER_PAGE;
			ll_mlcsr_core__begin_t* vertex_table_entry = get_vertex_table_entry(rank, page_level, indirection_entry->pc_offset + vertex_offset*sizeof(ll_mlcsr_core__begin_t));
			cout << "edge list level length " << vertex_table_entry->level_length << "\n";
			cout << "edge list start " << LL_EDGE_INDEX(vertex_table_entry->adj_list_start) << "\n";
			cout << "edge list level " << LL_EDGE_LEVEL(vertex_table_entry->adj_list_start) << "\n";
			cout << "vertex degree " << vertex_table_entry->degree << "\n\n";
			
			int vertex_degree = vertex_table_entry->degree;
			while (true) {
				unsigned edge_level = LL_EDGE_LEVEL(vertex_table_entry->adj_list_start);
				if (edge_level == 0) {
					cout << "edges are in level 0\n";
					break;
				} else {
					//TODO: could get edge table offset for the page level, but it will be the same as this level anyway
					LL_DATA_TYPE* neighbour;
					for (unsigned i = 0; i < vertex_table_entry->level_length; i++) {
						neighbour = get_edge_table_entry(rank, edge_level, et_chunk.pc_offset + ((LL_EDGE_INDEX(vertex_table_entry->adj_list_start) + i) * sizeof(LL_DATA_TYPE)));
						cout << "vertex " << vertex << " neighbour " << i << " is " << *neighbour << "\n";
					}

					vertex_degree -= vertex_table_entry->level_length;
					if (vertex_degree > 0) {
						//follow the indirection
						int continuation_offset = et_chunk.pc_offset + ((LL_EDGE_INDEX(vertex_table_entry->adj_list_start) + vertex_table_entry->level_length) * sizeof(LL_DATA_TYPE));
						vertex_table_entry = get_vertex_table_entry(rank, edge_level, continuation_offset);
						if (vertex_table_entry->level_length == 0) {
							//deletion has occurred
						}
					} else {
						cout << "read all the edges\n";
						break;
					}
				}
			}
		}		
	}
	
	return neighbours;
}