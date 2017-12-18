#ifndef DLLAMA_H
#define DLLAMA_H

#include <pthread.h>
#include <vector>

#include "llama.h"

#define SNAPSHOT_MESSAGE 0

using namespace std;

class dllama {
public:
    dllama();
    virtual ~dllama();
    void load_net_graph(string net_graph);
    edge_t add_edge(node_t src, node_t tgt);
    node_t add_node();
    size_t out_degree(node_t node);
    void out_iter_begin(ll_edge_iterator& iter, node_t node);
    ITERATOR_DECL bool out_iter_has_next(ll_edge_iterator& iter);
    ITERATOR_DECL edge_t out_iter_next(ll_edge_iterator& iter);
    vector<node_t> get_neighbours_of_vertex(node_t vertex);
    void add_random_edge();
    void auto_checkpoint();
    void refresh_ro_graph();
    //void add_property();
    //std::string get_property();
private:

protected:
    ll_writable_graph* graph;
    ll_database* database;
};

#endif /* DLLAMA_H */

