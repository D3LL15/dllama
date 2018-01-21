#ifndef SIMPLE_DLLAMA_H
#define SIMPLE_DLLAMA_H

#include <thread>
#include <vector>

#include "llama.h"

namespace dllama_ns {

    void start_mpi_listener();

    class simple_dllama {
    public:
        simple_dllama(bool initialise_mpi = true);
        virtual ~simple_dllama();
        void load_net_graph(std::string net_graph);
        edge_t add_edge(node_t src, node_t tgt);
        void delete_edge(node_t src, edge_t edge);
        node_t add_node();
        void add_node(node_t id);
        size_t out_degree(node_t node);
        void out_iter_begin(ll_edge_iterator& iter, node_t node);
        ITERATOR_DECL bool out_iter_has_next(ll_edge_iterator& iter);
        ITERATOR_DECL edge_t out_iter_next(ll_edge_iterator& iter);
        std::vector<node_t> get_neighbours_of_vertex(node_t vertex);
        void add_random_edge();
        void request_checkpoint();
        void auto_checkpoint();
    private:
        std::thread* mpi_listener;
    protected:
        void checkpoint();
        ll_writable_graph* graph;
        ll_database* database;
        bool handling_mpi;
    };
}


#endif /* SIMPLE_DLLAMA_H */
