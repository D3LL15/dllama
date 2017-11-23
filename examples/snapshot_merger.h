#ifndef SNAPSHOT_MERGER_H
#define SNAPSHOT_MERGER_H

#include "llama.h"

class snapshot_merger {
public:
    snapshot_merger();
    virtual ~snapshot_merger();
    void read_snapshots();
private:
protected:
};

#endif /* SNAPSHOT_MERGER_H */

