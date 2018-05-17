#ifndef LLAMA_TEST_H
#define LLAMA_TEST_H

#include "llama.h"

namespace dllama_ns {
    class dllama_test {
    public:
        llama_test();
        virtual ~llama_test();
        void test_llama_init();
        void test_llama_add_edges();
        void test_llama_print_neighbours();
        void full_test();
        void read_db();
    private:
    protected:
        ll_writable_graph* graph;
        ll_database* database;
    };
}


#endif /* LLAMA_TEST_H */

