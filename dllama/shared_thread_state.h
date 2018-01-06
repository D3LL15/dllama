#ifndef SHARED_THREAD_STATE_H
#define SHARED_THREAD_STATE_H

#include <mutex>
#include "dllama.h"
#include "snapshot_merger.h"

#define SINGLE_MACHINE 1

#define SNAPSHOT_MESSAGE 0
#define START_MERGE_REQUEST 1

extern int world_size;
extern int world_rank;

extern bool merge_starting;
extern std::mutex merge_starting_lock;
extern std::mutex ro_graph_lock;

extern int current_snapshot_level;

extern dllama* dllama_instance;
extern snapshot_merger* snapshot_merger_instance;

extern int dllama_number_of_vertices;

#endif /* SHARED_THREAD_STATE_H */