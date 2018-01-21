#include "gtest/gtest.h"
#include "dllama.h"
#include "shared_thread_state.h"
#include <iostream>
#include <mpi.h>

using namespace dllama_ns;
using namespace std;

namespace {

	// The fixture for testing class Foo.

	class FooTest : public ::testing::Test {
	protected:
		// You can remove any or all of the following functions if its body
		// is empty.

		FooTest() {
			// You can do set-up work for each test here.
		}

		virtual ~FooTest() {
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

	TEST_F(FooTest, MethodBarDoesAbc) {
		EXPECT_EQ(0, 3 - 3);
	}

	// Tests that Foo does Xyz.

	TEST_F(FooTest, DoesXyz) {
		// Exercises the Xyz feature of Foo.
	}
	
	TEST_F(FooTest, FullTest) {
		MPI_Barrier(MPI_COMM_WORLD);
		dllama_instance = new dllama(false);
	
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

			cout << "Rank " << world_rank << " trying to manually start merge\n";
			dllama_instance->start_merge();

			sleep(1);
		} else {
			sleep(2);
		}

		sleep(3);

		ll_edge_iterator neighbours;
		for (int i = 0; i < 5; i++) {
			dllama_instance->out_iter_begin(neighbours, i);
			cout << "rank "<< world_rank << " neighbours of vertex " << i <<": ";
			while (dllama_instance->out_iter_has_next(neighbours)) {
				dllama_instance->out_iter_next(neighbours);
				cout << neighbours.last_node;
			}
			cout << "\n";
		}

		dllama_instance->delete_db();
		sleep(5);
	}
	
	

} // namespace

int main(int argc, char **argv) {
	MPI_Init(NULL, NULL);
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
