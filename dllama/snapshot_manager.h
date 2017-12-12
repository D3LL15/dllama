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
    dll_level_meta* get_dll_level_meta(int rank, int level, int offset);
protected:
    char*** snapshots;
    int* rank_num_snapshots;
};

#endif /* SNAPSHOT_MANAGER_H */