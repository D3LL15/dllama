#include "gtest/gtest.h"
#include "dllama.h"
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

	class DllamaTest : public ::testing::Test {
	protected:
		// You can remove any or all of the following functions if its body
		// is empty.

		DllamaTest() {
			// You can do set-up work for each test here.
		}

		virtual ~DllamaTest() {
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

	// Tests that the Foo::Bar() method does Abc.

	TEST_F(DllamaTest, MethodBarDoesAbc) {
		EXPECT_EQ(0, 3 - 3);
	}

	// Tests that Foo does Xyz.

	TEST_F(DllamaTest, DoesXyz) {
		// Exercises the Xyz feature of Foo.
	}
	
	TEST_F(DllamaTest, KronGraphTest) {
		MPI_Barrier(MPI_COMM_WORLD);

		int num_nodes = 1024;
		int num_edges = 2655;
		dllama* my_dllama_instance = new dllama("database/", false);
		my_dllama_instance->load_net_graph("empty_graph.net");
		MPI_Barrier(MPI_COMM_WORLD);
		if (world_rank == 0) {
			ifstream file("kronecker_graph.net");
			if (!file.is_open()) {
				cout << "cannot open graph net file\n";
				return;
			}
			my_dllama_instance->add_nodes(num_nodes);
			string line;
			int from;
			int to;
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
			sleep(8);

			ifstream file2("kronecker_graph.net");
			if (!file2.is_open()) {
				cout << "cannot open graph net file\n";
				return;
			}
			while (getline(file2, line)) {
				sscanf(line.c_str(), "%d	%d", &from, &to);
				vector<node_t> neighbours = my_dllama_instance->get_neighbours_of_vertex(from);
				if (std::find(neighbours.begin(), neighbours.end(), to) == neighbours.end()) {
					cout << "no edge from " << from << " to " << to << "\n";
					EXPECT_EQ(true, false);
				}
			}
			file2.close();
		}
		MPI_Barrier(MPI_COMM_WORLD);
		my_dllama_instance->delete_db();
		my_dllama_instance->shutdown();
		delete my_dllama_instance;
		sleep(2);
	}
	
	TEST_F(DllamaTest, FullTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		dllama* dllama_instance = new dllama("database/", false);
	
		dllama_instance->load_net_graph("simple_graph.net");

		sleep(1);

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

			sleep(5);
		}

		MPI_Barrier(MPI_COMM_WORLD);
		
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

		dllama_instance->delete_db();
		sleep(5);
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
