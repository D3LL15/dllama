#ifndef SHARED_THREAD_STATE_H
#define SHARED_THREAD_STATE_H

#include <mutex>
#include <stack>
#include "dllama.h"
#include "snapshot_merger.h"
#include <condition_variable>

namespace dllama_ns {
    #define SINGLE_MACHINE 1

    #define SNAPSHOT_MESSAGE 0
    #define START_MERGE_REQUEST 1
    #define NEW_NODE_REQUEST 2
    #define NEW_NODE_ACK 3
    #define NEW_NODE_COMMAND 4
    #define NEW_EDGE 5

#ifdef DEBUG_ENABLED
    #define DEBUG(x) std::cout << x << std::endl;
    #define debug_enabled 1
#else
    #define DEBUG(x) 
    #define debug_enabled 0
#endif
    

    extern int world_size;
    extern int world_rank;

    extern bool merge_starting;
    extern std::mutex merge_starting_lock;
    extern std::mutex merge_lock;
    extern std::mutex ro_graph_lock;
    extern std::mutex checkpoint_lock;

    extern int current_snapshot_level;

    extern dllama* dllama_instance;
    extern snapshot_merger* snapshot_merger_instance;

    //protected by merge lock and merge starting lock
    extern int dllama_number_of_vertices;

    extern std::stack<int> new_node_ack_stack;
    extern bool self_adding_node;
    extern std::mutex num_new_node_requests_lock;
    extern int num_new_node_requests;
    extern std::mutex new_node_ack_stack_lock;
    
    extern int num_acks;
    extern std::mutex num_acks_lock;
    extern std::condition_variable num_acks_condition;
}



#endif /* SHARED_THREAD_STATE_H */