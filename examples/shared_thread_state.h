#ifndef SHARED_THREAD_STATE_H
#define SHARED_THREAD_STATE_H

#include <mutex>

#define SINGLE_MACHINE 1

#define SNAPSHOT_MESSAGE 0
#define START_MERGE_REQUEST 1

using namespace std;

extern int world_size;
extern int world_rank;

extern bool merge_starting;
extern mutex merge_starting_lock;

extern int current_snapshot_level;

#endif /* SHARED_THREAD_STATE_H */