#ifndef DLLAMA_TEST_H
#define DLLAMA_TEST_H

#include "llama.h"

class dllama_test {
public:
    dllama_test();
    virtual ~dllama_test();
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

#endif /* DLLAMA_TEST_H */

