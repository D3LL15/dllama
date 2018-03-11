#include "shared_thread_state.h"
#include <string>

namespace dllama_ns {
	
	shared_thread_state::shared_thread_state(graph_database* d, std::string database_location) {
		merge_starting = 0;
		current_snapshot_level = 0;
		dllama_number_of_vertices = 0;
		self_adding_node = 0;
		num_new_node_requests = 0;
		num_acks = 0;
		
		dllama_instance = d;
		snapshot_merger_instance = new snapshot_merger(database_location); 
	}
	
}