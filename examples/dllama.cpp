#include <cstdlib>
#include <iostream>

#include "dllama.h"

using namespace std;

dllama::dllama() {
    //initialise llama
    char* database_directory = (char*) alloca(16);
    strcpy(database_directory, "db");
    
    database = new ll_database(database_directory);
    graph = database->graph();
}

dllama::~dllama() {
}

void dllama::load_SNAP_graph() {
    ll_file_loaders loaders;
    char graph_location[] = "/home/dan/NetBeansProjects/Part2Project/graph.net";
    ll_file_loader* loader = loaders.loader_for(graph_location);
    if (loader == NULL) {
        fprintf(stderr, "Error: Unsupported input file type\n");
        return;
    }
    
    ll_loader_config loader_config;
    loader->load_direct(graph, graph_location, &loader_config);
    
    cout << "num levels " << graph->num_levels() << "\n";
}

edge_t dllama::add_edge(node_t src, node_t tgt) {
    return graph->add_edge(src,tgt);
}

node_t dllama::add_node() {
    //get vertex number from vertex allocator service
    return graph->add_node();
}

size_t dllama::out_degree(node_t node) {
    return graph->out_degree(node);
}

//call after a certain amount of updates
void dllama::auto_checkpoint() {
    graph->checkpoint();
}

void dllama::out_iter_begin(ll_edge_iterator& iter, node_t node) {
    graph->out_iter_begin(iter, node);
}

ITERATOR_DECL bool dllama::out_iter_has_next(ll_edge_iterator& iter) {
    return graph->out_iter_has_next(iter);
}

ITERATOR_DECL edge_t dllama::out_iter_next(ll_edge_iterator& iter) {
    return graph->out_iter_next(iter);
}