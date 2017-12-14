#ifndef SNAPSHOT_MANAGER_H
#define SNAPSHOT_MANAGER_H

#include <vector>
#include "llama.h"
#include "snapshot_merger.h"

using namespace std;

class snapshot_manager {
public:
    snapshot_manager(int* rank_snapshots);
    ~snapshot_manager();
    ll_persistent_chunk* get_ll_persistent_chunk(int rank, int level, int offset);
    dll_level_meta* get_dll_level_meta(int rank, int level);
    ll_mlcsr_core__begin_t* get_vertex_table_entry(int rank, int level, int offset);
    LL_DATA_TYPE* get_edge_table_entry(int rank, int level, int offset);
    dll_header_t* get_header(int rank, int level, int offset);
    vector<node_t> get_neighbours_of_vertex(int rank, node_t vertex);
protected:
    char*** snapshots;
    int* rank_num_snapshots;
    char* level_0_snapshot; //TODO: set this and use it
};

#endif /* SNAPSHOT_MANAGER_H */