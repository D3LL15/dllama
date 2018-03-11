#ifndef LLAMA_FOR_BENCHMARK_H
#define LLAMA_FOR_BENCHMARK_H

#include <thread>
#include <vector>
#include <string>

#include "llama.h"
#include "graph_database.h"

namespace dllama_ns {

    class llama_for_benchmark : public graph_database {
    public:
        llama_for_benchmark(std::string database_location, bool initialise_mpi = true);
        virtual ~llama_for_benchmark();
        void load_net_graph(std::string net_graph);
        edge_t add_edge(node_t src, node_t tgt);
        edge_t force_add_edge(node_t src, node_t tgt);
        void delete_edge(node_t src, edge_t edge);
        node_t add_nodes(int num_new_nodes);
        node_t add_node();
        node_t force_add_nodes(int num_nodes);
        size_t out_degree(node_t node);
        void out_iter_begin(ll_edge_iterator& iter, node_t node);
        ITERATOR_DECL bool out_iter_has_next(ll_edge_iterator& iter);
        ITERATOR_DECL edge_t out_iter_next(ll_edge_iterator& iter);
        std::vector<node_t> get_neighbours_of_vertex(node_t vertex);
        void add_random_edge();
        void request_checkpoint();
        void auto_checkpoint();
        void refresh_ro_graph();
        void start_merge();
        void delete_db();
        node_t max_nodes();
        void shutdown();
    protected:
        void checkpoint();
    };
}


#endif /* LLAMA_FOR_BENCHMARK_H */

