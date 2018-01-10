#ifndef SNAPSHOT_MERGER_H
#define SNAPSHOT_MERGER_H

#include <mpi.h>
#include <string>
#include <mutex>
#include "llama.h"

class snapshot_merger {
public:
    snapshot_merger();
    virtual ~snapshot_merger();
    void read_snapshots(std::string input_file_name = "db0/csr__out__0.dat");
    void read_second_snapshot();
    void start_snapshot_listener();
    void merge_snapshots(int* rank_snapshots);
    void begin_merge();
private:
protected:
    void handle_snapshot_message(MPI_Status status);
    void handle_merge_request(int source);
    void handle_new_node_request(MPI_Status status);
    void handle_new_node_command(MPI_Status status);
    int* received_snapshot_levels;
    int* expected_snapshot_levels;
    int* received_num_vertices;
    std::mutex listener_lock;

};

/**
 * The metadata for each level
 */
struct dll_level_meta {
    unsigned lm_level; // The level
    unsigned lm_sub_level; // The sub-level for 2D level structures

    unsigned lm_header_size; // In non-zero, the length of the header
    unsigned lm_base_level; // If non-zero, the level that this copies

    size_t lm_vt_size; // Vertex table size (number of nodes)
    size_t lm_vt_partitions; // The number of vertex table chunks/pages

    size_t lm_vt_offset; // VT offset within the level file
    size_t lm_header_offset; // The header offset
};

/// The header for the level

typedef struct {
    ll_large_persistent_chunk h_et_chunk; //level, length and offset
    size_t h_et_size;
} dll_header_t;

//we also use ll_persistent_chunk for each item in indirection table (level, length and offset)

#endif /* SNAPSHOT_MERGER_H */

