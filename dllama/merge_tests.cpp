#include "gtest/gtest.h"
#include "graph_database.h"
#include "dllama.h"
#include "simple_dllama.h"
#include "llama_for_benchmark.h"
#include "shared_thread_state.h"
#include <iostream>
#include <mpi.h>
#include <algorithm>
#include <fstream>
#include <sstream>

using namespace dllama_ns;
using namespace std;

namespace {

	// The fixture for testing class Foo.

	class MergeTest : public ::testing::Test {
	protected:
		// You can remove any or all of the following functions if its body
		// is empty.

		MergeTest() {
			// You can do set-up work for each test here.
		}

		virtual ~MergeTest() {
			// You can do clean-up work that doesn't throw exceptions here.
		}

		// If the constructor and destructor are not enough for setting up
		// and cleaning up each test, you can define the following methods:

		virtual void SetUp() {
			// Code here will be called immediately after the constructor (right
			// before each test).
		}

		virtual void TearDown() {
			// Code here will be called immediately after each test (right
			// before the destructor).
		}

		// Objects declared here can be used by all tests in the test case for Foo.
	};

	TEST_F(MergeTest, GoogleTestTest) {
		EXPECT_EQ(0, 3 - 3);
		EXPECT_NE(0, 2);
		EXPECT_TRUE(1==1);
	}
	
	void check_new_snapshot(graph_database* dllama_instance) {
		node_t expected_neighbours1[3] = {1, 2, 3};
		node_t expected_neighbours2[3] = {0, 2, 3};
		node_t expected_neighbours3[3] = {0, 1, 3};
		node_t expected_neighbours4[3] = {};
		node_t expected_neighbours5[3] = {0};
		node_t* expected_neighbours[5] = {expected_neighbours1, expected_neighbours2, expected_neighbours3, expected_neighbours4, expected_neighbours5};
		size_t expected_degree[5] = {3, 3, 3, 0, 1};

		ll_edge_iterator neighbours;
		for (int i = 0; i < 5; i++) {
			EXPECT_EQ(dllama_instance->out_degree(i), expected_degree[i]);
			dllama_instance->out_iter_begin(neighbours, i);
			if (debug_enabled) {
				cout << "rank "<< world_rank << " neighbours of vertex " << i <<": ";
			}
			int j = 0;
			while (dllama_instance->out_iter_has_next(neighbours)) {
				dllama_instance->out_iter_next(neighbours);
				EXPECT_EQ(neighbours.last_node, expected_neighbours[i][j]);
				if (debug_enabled) {
					cout << neighbours.last_node;
				}
				j++;
			}
			if (debug_enabled) {
				cout << "\n";
			}
		}
	}
	
	TEST_F(MergeTest, MultiMachineMergeTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		dllama* dllama_instance = new dllama("database/", false);
		dllama_instance->load_net_graph("simple_graph.net");

		MPI_Barrier(MPI_COMM_WORLD);
		
		if (world_rank == 0) {
			dllama_instance->add_edge(1, 0);
			dllama_instance->add_edge(2, 1);
			dllama_instance->request_checkpoint();
			dllama_instance->add_edge(2, 0);
			dllama_instance->request_checkpoint();
			sleep(1);
		}
		
		MPI_Barrier(MPI_COMM_WORLD);
		if (world_rank == 1) {
			node_t new_node = dllama_instance->add_node();
			dllama_instance->add_edge(new_node, 0);
			dllama_instance->request_checkpoint();
			dllama_instance->start_merge();
			sleep(3);
		}
			
		MPI_Barrier(MPI_COMM_WORLD);
			
		check_new_snapshot(dllama_instance);

		dllama_instance->delete_db();
		dllama_instance->shutdown();
		delete dllama_instance;
	}
	
	TEST_F(MergeTest, SingleMachineMergeTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		simple_dllama* dllama_instance = new simple_dllama("database/", false);
		dllama_instance->load_net_graph("simple_graph.net");
		if (world_rank == 0) {
			dllama_instance->add_edge(1, 0);
			dllama_instance->add_edge(2, 1);
			dllama_instance->request_checkpoint();
			dllama_instance->add_edge(2, 0);
			dllama_instance->request_checkpoint();
			node_t new_node = dllama_instance->add_node();
			dllama_instance->add_edge(new_node, 0);
			dllama_instance->request_checkpoint();
			dllama_instance->start_merge();
		
			check_new_snapshot(dllama_instance);
		}
		MPI_Barrier(MPI_COMM_WORLD);
		dllama_instance->delete_db();
		dllama_instance->shutdown();
		delete dllama_instance;
	}
	
	TEST_F(MergeTest, SendFileTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		dllama* dllama_instance = new dllama("database/", false);
		dllama_instance->load_net_graph("simple_graph.net");
			
		if (world_rank == 0) {
			dllama_instance->add_edge(1, 0);
			dllama_instance->add_edge(2, 1);
			dllama_instance->request_checkpoint();
			sleep(2);
		}
		MPI_Barrier(MPI_COMM_WORLD);
		if (world_rank != 0) {
			ostringstream oss;
			oss << "database/db" << world_rank << "/rank0/csr__out__1.dat";
			if (FILE *file = fopen(oss.str().c_str(), "r")) {
				EXPECT_TRUE(file);
				fclose(file);
			} else {
				EXPECT_FALSE(file);
			}
		}
		MPI_Barrier(MPI_COMM_WORLD);
		dllama_instance->delete_db();
		dllama_instance->shutdown();
		delete dllama_instance;
	}
	
	
}