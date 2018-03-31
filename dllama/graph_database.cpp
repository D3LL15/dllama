#include "graph_database.h"
#include "shared_thread_state.h"

namespace dllama_ns {
	int world_size;
	int world_rank;
	shared_thread_state* sstate;

	void start_mpi_listener() {
		sstate->snapshot_merger_instance->start_snapshot_listener();
	}
}

using namespace dllama_ns;
graph_database::~graph_database() {
	delete database;
}

