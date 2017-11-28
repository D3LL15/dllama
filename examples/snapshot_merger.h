#ifndef SNAPSHOT_MERGER_H
#define SNAPSHOT_MERGER_H

#include <mpi.h>
#include "llama.h"

class snapshot_merger {
public:
    snapshot_merger();
    virtual ~snapshot_merger();
    void read_snapshots();
    void start_snapshot_listener();
private:
protected:
    void handle_snapshot_message(MPI_Status status);
    void handle_merge_request(MPI_Status status);
    int* received_snapshot_levels;
    int* expected_snapshot_levels;
};

#endif /* SNAPSHOT_MERGER_H */

