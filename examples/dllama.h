#ifndef DLLAMA_H
#define DLLAMA_H

#include <pthread.h>

#include "llama.h"

class dllama {
public:
    dllama();
    virtual ~dllama();
    void load_SNAP_graph();
    edge_t add_edge(node_t src, node_t tgt);
    node_t add_node();
    size_t out_degree(node_t node);
    void out_iter_begin(ll_edge_iterator& iter, node_t node);
    ITERATOR_DECL bool out_iter_has_next(ll_edge_iterator& iter);
    ITERATOR_DECL edge_t out_iter_next(ll_edge_iterator& iter);
    //void add_property();
    //std::string get_property();
private:
    void auto_checkpoint();
protected:
    ll_writable_graph* graph;
    ll_database* database;
};

#endif /* DLLAMA_H */

