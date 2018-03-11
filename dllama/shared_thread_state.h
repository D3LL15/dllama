#ifndef SHARED_THREAD_STATE_H
#define SHARED_THREAD_STATE_H

#include <mutex>
#include <stack>
#include "graph_database.h"
#include "snapshot_merger.h"
#include <condition_variable>
#include <string>

namespace dllama_ns {
    #define SINGLE_MACHINE 1

    #define SNAPSHOT_MESSAGE 0
    #define START_MERGE_REQUEST 1
    #define NEW_NODE_REQUEST 2
    #define NEW_NODE_ACK 3
    #define NEW_NODE_COMMAND 4
    #define NEW_EDGE 5
    #define SHUTDOWN 6

#ifdef DEBUG_ENABLED
    #define DEBUG(x) std::cout << x << std::endl;
    #define debug_enabled 1
#else
    #define DEBUG(x) 
    #define debug_enabled 0
#endif

#ifdef SIMPLE_DLLAMA
    #define BENCHMARK_TYPE 1
#else
    #ifdef LLAMA_BENCHMARK
        #define BENCHMARK_TYPE 2
    #else
        #define BENCHMARK_TYPE 0
    #endif
#endif
    
    extern int world_size;
    extern int world_rank;

    class shared_thread_state {
        public:

            bool merge_starting;
            std::mutex merge_starting_lock;
            std::mutex merge_lock;
            std::mutex ro_graph_lock;
            std::mutex checkpoint_lock;

            int current_snapshot_level;
            unsigned int dllama_number_of_vertices; //protected by merge lock and merge starting lock

            graph_database* dllama_instance;
            snapshot_merger* snapshot_merger_instance;

            std::stack<int> new_node_ack_stack;
            bool self_adding_node;
            std::mutex num_new_node_requests_lock;
            int num_new_node_requests;
            std::mutex new_node_ack_stack_lock;

            int num_acks;
            std::mutex num_acks_lock;
            std::condition_variable num_acks_condition;
            
            shared_thread_state(graph_database* d, std::string database_location);
    };
    
    extern shared_thread_state* sstate;
}

#endif /* SHARED_THREAD_STATE_H */