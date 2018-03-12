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

	class IntegrationTest : public ::testing::Test {
	protected:
		// You can remove any or all of the following functions if its body
		// is empty.

		IntegrationTest() {
			// You can do set-up work for each test here.
		}

		virtual ~IntegrationTest() {
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

	TEST_F(IntegrationTest, GoogleTestTest) {
		EXPECT_EQ(0, 3 - 3);
		EXPECT_NE(0, 2);
		EXPECT_TRUE(1==1);
	}
	
	void krongraph_test(graph_database* my_dllama_instance, bool llama) {
		my_dllama_instance->load_net_graph("empty_graph.net");
		int num_nodes = 1024;
		int num_edges = 2655;
		string line;
		int from;
		int to;
		MPI_Barrier(MPI_COMM_WORLD);
		if (world_rank == 0) {
			ifstream file("kronecker_graph.net");
			if (!file.is_open()) {
				cout << "cannot open graph net file\n";
				return;
			}
			my_dllama_instance->add_nodes(num_nodes);
			
			for (int i = 0; i < num_edges/2; i++) {
				getline(file, line);
				sscanf(line.c_str(), "%d	%d", &from, &to);
				my_dllama_instance->add_edge(from, to);
			}
			my_dllama_instance->request_checkpoint();
			for (int i = num_edges/2; i < num_edges; i++) {
				getline(file, line);
				sscanf(line.c_str(), "%d	%d", &from, &to);
				my_dllama_instance->add_edge(from, to);
			}
			file.close();
			my_dllama_instance->request_checkpoint();
			my_dllama_instance->start_merge();
			sleep(3);
		}
		MPI_Barrier(MPI_COMM_WORLD);
		if ((world_rank == 0) || (!llama)) {
			ifstream file2("kronecker_graph.net");
			if (!file2.is_open()) {
				cout << "cannot open graph net file\n";
				return;
			}
			while (getline(file2, line)) {
				sscanf(line.c_str(), "%d	%d", &from, &to);
				vector<node_t> neighbours = my_dllama_instance->get_neighbours_of_vertex(from);
				if (std::find(neighbours.begin(), neighbours.end(), to) == neighbours.end()) {
					cout << "rank " << world_rank << ": no edge from " << from << " to " << to << "\n";
					EXPECT_EQ(true, false);
				}
			}
			file2.close();
		}
		
		MPI_Barrier(MPI_COMM_WORLD);
		my_dllama_instance->delete_db();
		my_dllama_instance->shutdown();
		delete my_dllama_instance;
		sleep(1);
	}
	
	TEST_F(IntegrationTest, DLLAMAKronGraphTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		dllama* my_dllama_instance = new dllama("database/", false);
		krongraph_test(my_dllama_instance, false);
	}
	
	TEST_F(IntegrationTest, SimpleDLLAMAKronGraphTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		simple_dllama* my_dllama_instance = new simple_dllama("database/", false);
		krongraph_test(my_dllama_instance, false);
	}
	
	TEST_F(IntegrationTest, LLAMAKronGraphTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		llama_for_benchmark* my_dllama_instance = new llama_for_benchmark("database/", false);
		krongraph_test(my_dllama_instance, true);
	}
	
	void full_test(graph_database* dllama_instance, bool llama) {
		dllama_instance->load_net_graph("simple_graph.net");

		MPI_Barrier(MPI_COMM_WORLD);
		if (world_rank == 0) {
			dllama_instance->add_edge(1, 0);
			dllama_instance->add_edge(2, 1);
			dllama_instance->request_checkpoint();
			dllama_instance->add_edge(2, 0);
			dllama_instance->request_checkpoint();
			node_t new_node = dllama_instance->add_node();
			dllama_instance->add_edge(new_node, 0);
			dllama_instance->request_checkpoint();
			sleep(1);

			DEBUG("Rank " << world_rank << " trying to manually start merge");
			dllama_instance->start_merge();

			sleep(3);
		}

		MPI_Barrier(MPI_COMM_WORLD);
		
		node_t expected_neighbours1[3] = {1, 2, 3};
		node_t expected_neighbours2[3] = {0, 2, 3};
		node_t expected_neighbours3[3] = {0, 1, 3};
		node_t expected_neighbours4[3] = {};
		node_t expected_neighbours5[3] = {0};
		node_t* expected_neighbours[5] = {expected_neighbours1, expected_neighbours2, expected_neighbours3, expected_neighbours4, expected_neighbours5};
		size_t expected_degree[5] = {3, 3, 3, 0, 1};
		
		if ((world_rank == 0) || (!llama)) {
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

		dllama_instance->delete_db();
		dllama_instance->shutdown();
		delete dllama_instance;
		sleep(1);
	}
	
	TEST_F(IntegrationTest, DLLAMAFullTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		dllama* dllama_instance = new dllama("database/", false);
		full_test(dllama_instance, false);
	}
	
	TEST_F(IntegrationTest, SimpleDLLAMAFullTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		simple_dllama* dllama_instance = new simple_dllama("database/", false);
		full_test(dllama_instance, false);
	}
	
	TEST_F(IntegrationTest, LLAMAFullTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		llama_for_benchmark* dllama_instance = new llama_for_benchmark("database/", false);
		full_test(dllama_instance, true);
	}
	
	

} // namespace

int main(int argc, char **argv) {
	int p = 0;
	int *provided;
	provided = &p;
	MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, provided);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	
	::testing::InitGoogleTest(&argc, argv);
	
	::testing::TestEventListeners& listeners =
		::testing::UnitTest::GetInstance()->listeners();
	if (world_rank != 0) {
		delete listeners.Release(listeners.default_result_printer());
	}
	int result = RUN_ALL_TESTS();
	MPI_Finalize();
	return result;
}
