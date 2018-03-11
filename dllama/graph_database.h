#ifndef GRAPH_DATABASE_H
#define GRAPH_DATABASE_H


#include <thread>
#include <vector>
#include <string>

#include "llama.h"

namespace dllama_ns {

    class graph_database {
    public:
        virtual ~graph_database();
        virtual void load_net_graph(std::string net_graph) = 0;
        virtual edge_t add_edge(node_t src, node_t tgt) = 0;
        virtual edge_t force_add_edge(node_t src, node_t tgt) = 0;
        virtual void delete_edge(node_t src, edge_t edge) = 0;
        virtual node_t add_nodes(int num_new_nodes) = 0;
        virtual node_t add_node() = 0;
        virtual node_t force_add_nodes(int num_nodes) = 0;
        virtual size_t out_degree(node_t node) = 0;
        virtual void out_iter_begin(ll_edge_iterator& iter, node_t node) = 0;
        virtual ITERATOR_DECL bool out_iter_has_next(ll_edge_iterator& iter) = 0;
        virtual ITERATOR_DECL edge_t out_iter_next(ll_edge_iterator& iter) = 0;
        virtual std::vector<node_t> get_neighbours_of_vertex(node_t vertex) = 0;
        virtual void add_random_edge() = 0;
        virtual void request_checkpoint() = 0;
        virtual void auto_checkpoint() = 0;
        virtual void refresh_ro_graph() = 0;
        virtual void start_merge() = 0;
        virtual void delete_db() = 0;
        virtual node_t max_nodes() = 0;
        virtual void shutdown() = 0;
    protected:
        virtual void checkpoint() = 0;
        
        ll_writable_graph* graph;
        ll_database* database;
        bool handling_mpi;
        std::string database_location;
        std::thread* mpi_listener;
    };
}


#endif /* GRAPH_DATABASE_H */

