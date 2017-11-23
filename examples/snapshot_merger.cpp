#include "snapshot_merger.h"

using namespace std;

snapshot_merger::snapshot_merger() {
}

snapshot_merger::~snapshot_merger() {
}

void snapshot_merger::read_snapshots() {
    std::string dir = "db";
    ll_persistent_storage* storage = new ll_persistent_storage(dir.c_str());
    std::vector<std::string> csrs = storage->list_context_names("csr");
    
}