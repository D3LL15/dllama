#include <mpi.h>
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <mutex>
#include <sys/stat.h>
#include <sys/types.h>
#include <set>
#include <vector>
#include <cstdio>
#include <condition_variable>
#include <chrono>

#include "snapshot_merger.h"
#include "shared_thread_state.h"
#include "snapshot_manager.h"
#include "llama/ll_mlcsr_helpers.h"

using namespace std;
using namespace std::chrono;
using namespace dllama_ns;

snapshot_merger::snapshot_merger(string database_location) {
	this->database_location = database_location;
	
	received_snapshot_levels = new int[world_size]();
	expected_snapshot_levels = new int[world_size];
	received_num_vertices = new int[world_size]();
	for (int i = 0; i < world_size; i++) {
		expected_snapshot_levels[i] = -1;
	}
	//don't need to hear from yourself
	expected_snapshot_levels[world_rank] = 0;
}

snapshot_merger::~snapshot_merger() {
	delete[] received_snapshot_levels;
	delete[] expected_snapshot_levels;
	delete[] received_num_vertices;
}

void snapshot_merger::handle_snapshot_message(MPI_Status status) {
	int bytes_received;
	MPI_Get_count(&status, MPI_BYTE, &bytes_received);
	DEBUG("Rank " << world_rank << " number of bytes being received: " << bytes_received << " from " << status.MPI_SOURCE);
	char* memblock = new char [bytes_received];
	MPI_Recv(memblock, bytes_received, MPI_BYTE, status.MPI_SOURCE, SNAPSHOT_MESSAGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	DEBUG("Rank " << world_rank << " received file");

	uint32_t file_number = 0;
	file_number += memblock[0] << 24;
	file_number += memblock[1] << 16;
	file_number += memblock[2] << 8;
	file_number += memblock[3];
	DEBUG("Rank " << world_rank << " received file number: " << file_number);
	
	unsigned int temp_num_vertices = (unsigned char)memblock[4];
	unsigned int num_vertices = temp_num_vertices << 24;
	temp_num_vertices = (unsigned char)memblock[5];
	num_vertices += temp_num_vertices << 16;
	temp_num_vertices = (unsigned char)memblock[6];
	num_vertices += temp_num_vertices << 8;
	num_vertices += (unsigned char)memblock[7];
	DEBUG("Rank " << world_rank << " number of vertices from other machine: " << num_vertices);

	//we can get away with this for single level files
	received_snapshot_levels[status.MPI_SOURCE] = file_number;
	received_num_vertices[status.MPI_SOURCE] = num_vertices;

	ostringstream oss;
	oss << database_location << "db" << world_rank << "/rank" << status.MPI_SOURCE;
	//create receipt directory if it doesn't already exist
	mkdir(oss.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IWOTH | S_IXOTH);
	oss << "/csr__out__" << file_number << ".dat";

	string output_file_name = oss.str();

	ofstream file(output_file_name, ios::out | ios::binary | ios::trunc);
	if (file.is_open()) {
		file.write(memblock + 8, bytes_received - 8);
		file.close();
	} else cout << "Rank " << world_rank << " unable to open output file\n";
	delete[] memblock;
}

void snapshot_merger::handle_merge_request(int source) {
	//stop main thread writing snapshots
	sstate->merge_starting_lock.lock();
	bool merge_had_started = sstate->merge_starting;
	sstate->merge_starting = 1;
	sstate->merge_starting_lock.unlock();
	DEBUG("Rank " << world_rank << " received merge request");
	int expected_level;
	if (source != world_rank) {
		MPI_Recv(&expected_level, 1, MPI_INT, source, START_MERGE_REQUEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	} else {
		expected_level = 2;
	}

	//broadcast start merge request if this is the first merge request you have heard
	if (!merge_had_started) {
		sstate->merge_lock.lock();
		for (int i = 0; i < world_size; i++) {
			if (i != world_rank) {
				MPI_Send(&sstate->current_snapshot_level, 1, MPI_INT, i, START_MERGE_REQUEST, MPI_COMM_WORLD);
			}
		}
	}

	//set sender's value in expected snapshot vector to the snapshot number they sent in merge request (the last snapshot they sent)
	expected_snapshot_levels[source] = expected_level - 2;
	//if expected snapshot numbers correspond to currently held latest snapshots (the last snapshots we received from those hosts) then merge, else listen for snapshots until they are equal
	bool received_all_snapshots = 1;
	for (int i = 0; i < world_size; i++) {
		if (received_snapshot_levels[i] != expected_snapshot_levels[i]) {
			DEBUG("received " << received_snapshot_levels[i] << " expected " << expected_snapshot_levels[i]);
			received_all_snapshots = 0;
		}
	}
	if (received_all_snapshots) {
		DEBUG("Rank " << world_rank << " received merge requests from all other hosts");

		received_snapshot_levels[world_rank] = sstate->current_snapshot_level - 2;
		merge_snapshots(received_snapshot_levels);
		
		//tell main thread to stop reading
		sstate->ro_graph_lock.lock();
		
		//reset main thread llama to use new level 0 snapshot, while retaining in memory deltas, then flush deltas to new snapshot later
		sstate->dllama_instance->refresh_ro_graph();
		sstate->ro_graph_lock.unlock();

		//clean up after merge
		//setting expected snapshot level to -1 means we have to hear from that host before merging
		for (int i = 0; i < world_size; i++) {
			received_snapshot_levels[i] = 0;
			expected_snapshot_levels[i] = -1;
			received_num_vertices[i] = 0;
		}
		//don't need to hear from yourself
		expected_snapshot_levels[world_rank] = 0;

		//allow main thread to continue writing snapshots
		sstate->merge_lock.unlock();
		sstate->merge_starting_lock.lock();
		sstate->merge_starting = 0;
		sstate->merge_starting_lock.unlock();
	}
}

void snapshot_merger::begin_merge() {
	listener_lock.lock();
	handle_merge_request(world_rank);
	listener_lock.unlock();
}

void snapshot_merger::handle_new_node_request(MPI_Status status) {
	DEBUG("Rank " << world_rank << " handling new node request");
	int num_new_nodes;
	MPI_Recv(&num_new_nodes, 1, MPI_INT, status.MPI_SOURCE, NEW_NODE_REQUEST, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	DEBUG("Rank " << world_rank << " completed mpi recv");
	if (sstate->num_new_node_requests == 0) {
		sstate->num_new_node_requests_lock.lock();
	}
	sstate->num_new_node_requests++;
	DEBUG("Rank " << world_rank << " handling new node request midpoint");
	sstate->new_node_ack_stack_lock.lock();
	if (sstate->self_adding_node && status.MPI_SOURCE < world_rank) {
		sstate->new_node_ack_stack.push(status.MPI_SOURCE);
	} else {
		int zero = 0;
		MPI_Send(&zero, 1, MPI_INT, status.MPI_SOURCE, NEW_NODE_ACK, MPI_COMM_WORLD);
	}
	sstate->new_node_ack_stack_lock.unlock();
	DEBUG("Rank " << world_rank << " done handling new node request");
}

void snapshot_merger::handle_new_node_command(MPI_Status status) {
	DEBUG("Rank " << world_rank << " commanded to add node");
	int num_new_nodes;
	MPI_Recv(&num_new_nodes, 1, MPI_INT, status.MPI_SOURCE, NEW_NODE_COMMAND, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	//check we are not currently checkpointing
	
	sstate->dllama_instance->force_add_nodes(num_new_nodes);
	
	sstate->num_new_node_requests--;
	if (sstate->num_new_node_requests == 0) {
		sstate->num_new_node_requests_lock.unlock();
	}
}

void snapshot_merger::handle_new_edge(MPI_Status status) {
	int edge[2];
	MPI_Recv(&edge, 2, MPI_INT, status.MPI_SOURCE, NEW_EDGE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	sstate->dllama_instance->force_add_edge(edge[0], edge[1]);
	DEBUG("Rank " << world_rank << " added edge " << edge[0] << " to " << edge[1]);
}

void snapshot_merger::handle_new_node_ack(MPI_Status status) {
	int ack;
	MPI_Recv(&ack, 1, MPI_INT, status.MPI_SOURCE, NEW_NODE_ACK, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	{
		lock_guard<mutex> lk(sstate->num_acks_lock);
		sstate->num_acks++;
	}
	sstate->num_acks_condition.notify_all();
}

void snapshot_merger::start_snapshot_listener() {
	DEBUG("Rank " << world_rank << " mpi_listener running");

	MPI_Status status;
	bool running = true;
	while (running) {
		DEBUG("Rank " << world_rank << " start probe");
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		DEBUG("Rank " << world_rank << " probe successful");
		listener_lock.lock();
		DEBUG("mpi tag " << status.MPI_TAG << " received by machine " << world_rank);
		switch (status.MPI_TAG) {
			case SNAPSHOT_MESSAGE:
				handle_snapshot_message(status);
				break;
			case START_MERGE_REQUEST:
				handle_merge_request(status.MPI_SOURCE);
				break;
			case NEW_NODE_REQUEST:
				handle_new_node_request(status);
				break;
			case NEW_NODE_COMMAND:
				handle_new_node_command(status);
				break;
			case NEW_EDGE:
				handle_new_edge(status);
				break;
			case NEW_NODE_ACK:
				handle_new_node_ack(status);
				break;
			case SHUTDOWN:
				running = false;
				int nop;
				MPI_Recv(&nop, 1, MPI_INT, status.MPI_SOURCE, SHUTDOWN, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				break;
			default:
				cout << "Rank " << world_rank << " received message with unknown tag\n";
		}
		listener_lock.unlock();
	}
}

//for debugging
void snapshot_merger::read_snapshots(string input_file_name) {
	if (world_rank == 0) {
		ifstream file(input_file_name, ios::in | ios::binary | ios::ate);
		if (file.is_open()) {
			int file_size = file.tellg();
			char* memblock = new char [file_size];
			dll_level_meta* meta;

			file.seekg(0, ios::beg);
			file.read(memblock, file_size);
			file.close();

			meta = (dll_level_meta*) memblock;
			cout << "size of level meta " << sizeof(dll_level_meta) << "\n";
			cout << "level " << meta->lm_level << "\n";
			cout << "header offset " << meta->lm_header_offset << "\n";
			cout << "header size " << meta->lm_header_size << "\n";
			cout << "vt offset " << meta->lm_vt_offset << "\n";
			cout << "vt partitions " << meta->lm_vt_partitions << "\n";
			cout << "vt size " << meta->lm_vt_size << "\n\n";
			
			dll_header_t* header = (dll_header_t*) (memblock + meta->lm_header_offset);
			cout << "et size " << header->h_et_size << "\n";
			
			ll_large_persistent_chunk et_chunk = header->h_et_chunk;
			cout << "et level " << et_chunk.pc_level << "\n";
			cout << "et length " << et_chunk.pc_length << "\n";
			cout << "et offset " << et_chunk.pc_offset << "\n\n";
			
			ll_persistent_chunk* indirection_entry = (ll_persistent_chunk*) (memblock + meta->lm_vt_offset /*+ sizeof(ll_persistent_chunk)*/);
			cout << "vertex table chunk level " << indirection_entry->pc_level << "\n";
			cout << "vertex table chunk length " << indirection_entry->pc_length << "\n";
			cout << "vertex table chunk offset " << indirection_entry->pc_offset << "\n\n";
			
			//NB. the edge list offset is based off the start of the edge table, unlike the rest of the offsets
			ll_mlcsr_core__begin_t* vertex_table_entry = (ll_mlcsr_core__begin_t*) (memblock + indirection_entry->pc_offset);
			cout << "edge list level length " << vertex_table_entry->level_length << "\n";
			cout << "edge list start " << vertex_table_entry->adj_list_start << "\n";
			cout << "vertex degree " << vertex_table_entry->degree << "\n\n";
			
			LL_DATA_TYPE* neighbour;
			for (unsigned i = 0; i < vertex_table_entry->level_length; i++) {
				neighbour = (LL_DATA_TYPE*) (memblock + et_chunk.pc_offset + vertex_table_entry->adj_list_start + (i * sizeof(LL_DATA_TYPE)));
				cout << "neighbour " << i << " is " << LL_VALUE_PAYLOAD(*neighbour) << "\n";
			}

			delete[] memblock;
		} else cout << "Rank " << world_rank << " unable to open snapshot file\n";
		
		read_second_snapshot();
	}
}

//for debugging
void snapshot_merger::read_second_snapshot() {
	string input_file_name = "db0/csr__out__1.dat";
	ifstream file(input_file_name, ios::in | ios::binary | ios::ate);
	if (file.is_open()) {
		int file_size = file.tellg();
		char* memblock = new char [file_size];
		dll_level_meta* meta;

		file.seekg(0, ios::beg);
		file.read(memblock, file_size);
		file.close();

		meta = (dll_level_meta*) memblock;
		cout << "\n\n\nsecond snapshot\n";
		cout << "size of level meta " << sizeof(dll_level_meta) << "\n";
		cout << "level " << meta->lm_level << "\n";
		cout << "header offset " << meta->lm_header_offset << "\n";
		cout << "header size " << meta->lm_header_size << "\n";
		cout << "vt offset " << meta->lm_vt_offset << "\n";
		cout << "vt partitions " << meta->lm_vt_partitions << "\n";
		cout << "vt size " << meta->lm_vt_size << "\n\n";

		dll_header_t* header = (dll_header_t*) (memblock + meta->lm_header_offset);
		cout << "et size " << header->h_et_size << "\n";

		ll_large_persistent_chunk et_chunk = header->h_et_chunk;
		cout << "et level " << et_chunk.pc_level << "\n";
		cout << "et length " << et_chunk.pc_length << "\n";
		cout << "et offset " << et_chunk.pc_offset << "\n\n";

		ll_persistent_chunk* indirection_entry = (ll_persistent_chunk*) (memblock + meta->lm_vt_offset /*+ sizeof(ll_persistent_chunk)*/);
		cout << "vertex table chunk level " << indirection_entry->pc_level << "\n";
		cout << "vertex table chunk length " << indirection_entry->pc_length << "\n";
		cout << "vertex table chunk offset " << indirection_entry->pc_offset << "\n\n";

		//NB. the edge list offset is based off the start of the edge table, unlike the rest of the offsets
		ll_mlcsr_core__begin_t* vertex_table_entry = (ll_mlcsr_core__begin_t*) (memblock + indirection_entry->pc_offset + sizeof(ll_mlcsr_core__begin_t));
		cout << "edge list level length " << vertex_table_entry->level_length << "\n";
		cout << "edge list start " << LL_EDGE_INDEX(vertex_table_entry->adj_list_start) << "\n";
		cout << "vertex degree " << vertex_table_entry->degree << "\n\n";

		LL_DATA_TYPE* neighbour;
		for (unsigned i = 0; i < vertex_table_entry->level_length; i++) {
			neighbour = (LL_DATA_TYPE*) (memblock + et_chunk.pc_offset + LL_EDGE_INDEX(vertex_table_entry->adj_list_start) + (i * sizeof(LL_DATA_TYPE)));
			cout << "neighbour " << i << " is " << LL_VALUE_PAYLOAD(*neighbour) << "\n";
		}
		
		cout << "ll_mlcsr_core__begin_t size " << sizeof(ll_mlcsr_core__begin_t) << "\n";
		ll_mlcsr_core__begin_t* continuation = (ll_mlcsr_core__begin_t*) (memblock + et_chunk.pc_offset + LL_EDGE_INDEX(vertex_table_entry->adj_list_start) + (vertex_table_entry->level_length * sizeof(LL_DATA_TYPE)));
		cout << "continuation start " << continuation->adj_list_start << "\n";
		cout << "continuation degree " << continuation->degree << "\n";
		cout << "continuation level length " << continuation->level_length << "\n";
		
		delete[] memblock;
	} else cout << "Rank " << world_rank << " unable to open snapshot file\n";
}

std::ostream& operator<<(std::ostream& out, const ll_mlcsr_core__begin_t& h) {
    return out.write((char*) (&h), sizeof(ll_mlcsr_core__begin_t));
}

std::ostream& operator<<(std::ostream& out, const ll_persistent_chunk& h) {
    return out.write((char*) (&h), sizeof(ll_persistent_chunk));
}

void snapshot_merger::merge_snapshots_helper(int* rank_snapshots, bool local_only) {
	DEBUG("Rank " << world_rank << " starting merge");
	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	
	received_num_vertices[world_rank] = sstate->dllama_number_of_vertices;
	
	DEBUG("Rank " << world_rank << " before max element");
	int number_of_vertices = *max_element(received_num_vertices, received_num_vertices + world_size) + 1;
	DEBUG("Rank " << world_rank << "num vertices for new level 0: " << number_of_vertices);
	
	//metadata
	dll_level_meta new_meta;
	new_meta.lm_level = 0;
	new_meta.lm_sub_level = 0;
	new_meta.lm_header_size = 32;
	new_meta.lm_base_level = 0;
	new_meta.lm_vt_partitions = (number_of_vertices + LL_ENTRIES_PER_PAGE - 1) / LL_ENTRIES_PER_PAGE;
	new_meta.lm_vt_size = number_of_vertices;

	//edge table
	snapshot_manager snapshots(rank_snapshots, database_location);

	vector<LL_DATA_TYPE> edge_table;
	vector<ll_mlcsr_core__begin_t> vertex_table;
	for (int vertex = 0; vertex < number_of_vertices; vertex++) {
		set<LL_DATA_TYPE> neighbours;
		if (local_only) {
			vector<LL_DATA_TYPE> new_neighbours = snapshots.get_neighbours_of_vertex(world_rank, vertex);
			neighbours.insert(new_neighbours.begin(), new_neighbours.end());
		} else {
			for (int r = 0; r < world_size; r++) {
				//add all neighbours in edge table pointed to by chunk
				if (vertex <= received_num_vertices[r]) {
					vector<LL_DATA_TYPE> new_neighbours = snapshots.get_neighbours_of_vertex(r, vertex);
					neighbours.insert(new_neighbours.begin(), new_neighbours.end());
				}
			}
		}
		//add edges from level 0 if the vertex existed in level 0
		vector<LL_DATA_TYPE> new_neighbours = snapshots.get_level_0_neighbours_of_vertex(vertex);
		neighbours.insert(new_neighbours.begin(), new_neighbours.end());

		ll_mlcsr_core__begin_t vertex_table_entry;
		vertex_table_entry.adj_list_start = edge_table.size();
		
		edge_table.insert(edge_table.end(), neighbours.begin(), neighbours.end()); //could just write this directly to file
		vertex_table_entry.degree = neighbours.size();
		vertex_table_entry.level_length = vertex_table_entry.degree; // level is 0 anyway
		vertex_table.push_back(vertex_table_entry);
	}
	
	int num_edge_table_chunks = ((edge_table.size() * sizeof(LL_DATA_TYPE)) + LL_BLOCK_SIZE - 1) / LL_BLOCK_SIZE;
	int num_vertex_chunks = (new_meta.lm_vt_size * sizeof(ll_mlcsr_core__begin_t) + LL_BLOCK_SIZE - 1) / LL_BLOCK_SIZE;
	int num_indirection_entries = (new_meta.lm_vt_size + LL_ENTRIES_PER_PAGE - 1) / LL_ENTRIES_PER_PAGE;
	int num_indirection_table_chunks = ((num_indirection_entries * sizeof(ll_persistent_chunk)) + sizeof(dll_header_t) + LL_BLOCK_SIZE - 1) / LL_BLOCK_SIZE;
	int file_size = LL_BLOCK_SIZE + (num_edge_table_chunks + num_indirection_table_chunks + num_vertex_chunks) * LL_BLOCK_SIZE;
	
	DEBUG("num_edge_table_chunks " << num_edge_table_chunks);
	DEBUG("num_vertex_chunks " << num_vertex_chunks);
	DEBUG("num_indirection_entries " << num_indirection_entries);
	DEBUG("num_indirection_table_chunks " << num_indirection_table_chunks);
	DEBUG("new output file size should be: " << file_size);

	//write to file	
	write_merged_snapshot(edge_table, vertex_table, new_meta);

	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	auto duration = duration_cast<microseconds>(t2 - t1).count();
	if (world_rank == 0 && BENCHMARKING) {
		cout << duration << " ";
	}
}

void snapshot_merger::write_merged_snapshot(vector<LL_DATA_TYPE> edge_table, vector<ll_mlcsr_core__begin_t> vertex_table, dll_level_meta new_meta) {
	ostringstream oss;
	oss << database_location << "db" << world_rank << "/new_level0.dat";
	string output_file_name = oss.str();
	ofstream file(output_file_name, ios::out | ios::binary | ios::ate);
	if (file.is_open()) {
		
		//edge table
		file.seekp(LL_BLOCK_SIZE);
		for (vector<LL_DATA_TYPE>::iterator edges = edge_table.begin(); edges != edge_table.end(); ++edges) {
			file.write((char*) (&(*edges)), sizeof(LL_DATA_TYPE));
		}
		
		//vertex chunks
		int position = file.tellp();
		DEBUG("edge table finished at: " << position);
		if (position % LL_BLOCK_SIZE != 0) {
			position = ((position / LL_BLOCK_SIZE) + 1) * LL_BLOCK_SIZE;
			file.seekp(position);
		}
		DEBUG("writing the vertex chunks at position " << position);
		int vertex_chunks_position = position;
		std::copy(vertex_table.begin(), vertex_table.end(), std::ostream_iterator<ll_mlcsr_core__begin_t>(file));
		
		//header
		dll_header_t header;
		header.h_et_size = edge_table.size();
		ll_large_persistent_chunk et_chunk;
		et_chunk.pc_level = 0;
		et_chunk.pc_length = header.h_et_size * sizeof(LL_DATA_TYPE);
		et_chunk.pc_offset = LL_BLOCK_SIZE;
		header.h_et_chunk = et_chunk;
		position = file.tellp();
		if (position % LL_BLOCK_SIZE != 0) {
			position = ((position / LL_BLOCK_SIZE) + 1) * LL_BLOCK_SIZE;
			file.seekp(position);
		}
		new_meta.lm_header_offset = position;
		DEBUG("writing the header at position " << position);
		file.write((char*)(&header), sizeof(dll_header_t));
		
		//indirection table
		new_meta.lm_vt_offset = file.tellp();
		vector<ll_persistent_chunk> indirection_table;
		for (unsigned i = 0; i < new_meta.lm_vt_partitions; ++i) {
			ll_persistent_chunk vertex_table_chunk;
			vertex_table_chunk.pc_level = 0;
			vertex_table_chunk.pc_length = LL_ENTRIES_PER_PAGE * sizeof(ll_mlcsr_core__begin_t);
			vertex_table_chunk.pc_offset = vertex_chunks_position + i * vertex_table_chunk.pc_length;
			indirection_table.push_back(vertex_table_chunk);
		}
		DEBUG("writing the indirection table at position " << new_meta.lm_vt_offset);
		std::copy(indirection_table.begin(), indirection_table.end(), std::ostream_iterator<ll_persistent_chunk>(file));
		
		position = file.tellp();
		DEBUG("finished at position " << position);
		//metadata
		file.seekp(0);
		file.write((char*)(&new_meta), sizeof(dll_level_meta));
		
		DEBUG("written file size is " << file.tellp());
		
		file.close();
	} else cout << "Rank " << world_rank << " unable to open output file\n";
}

void snapshot_merger::merge_snapshots(int* rank_snapshots) {
	merge_snapshots_helper(rank_snapshots, false);
}

void snapshot_merger::merge_local_llama() {
	int* rank_snapshots = new int[world_size]();
	rank_snapshots[world_rank] = sstate->current_snapshot_level - 2;
	merge_snapshots_helper(rank_snapshots, true);
	delete[] rank_snapshots;
}