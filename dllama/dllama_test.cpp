#include <cstdlib>
#include <iostream>

#include "dllama_test.h"

using namespace std;

dllama_test::dllama_test() {
}

dllama_test::~dllama_test() {
}

void dllama_test::full_test() {
    cout << "starting test\n";
    
    //open database
    
    char* database_directory = (char*) alloca(16);
    strcpy(database_directory, "db");
    
    ll_database database(database_directory);
    graph = database.graph();
    
    // Load the graph

    ll_file_loaders loaders;
    char graph_location[] = "/home/dan/NetBeansProjects/Part2Project/graph.net";
    ll_file_loader* loader = loaders.loader_for(graph_location);
    if (loader == NULL) {
            fprintf(stderr, "Error: Unsupported input file type\n");
            return;
    }
    
    ll_loader_config loader_config;
    loader->load_direct(graph, graph_location, &loader_config);
    
    node_t src = graph->pick_random_node();
    node_t tgt = graph->pick_random_node();
    node_t temp = graph->pick_random_node();

    graph->add_edge(src,tgt);
    graph->add_edge(tgt,temp);
    graph->add_edge(src,temp);
    
    for (std::unordered_map<std::string, LL_CSR*>::const_iterator csrs_iterator = graph->ro_graph().csrs().begin(); 
            csrs_iterator != graph->ro_graph().csrs().end(); 
            csrs_iterator ++)
    {
        cout << csrs_iterator->first << "\n";
        cout << csrs_iterator->second << "\n";
    }
    
    cout << "num levels " << graph->num_levels() << "\n";

}

void dllama_test::test_llama_init() {
    cout << "starting test\n";
    
    //open database
    
    char* database_directory = (char*) alloca(16);
    strcpy(database_directory, "db");
    
    database = new ll_database(database_directory);
    graph = database->graph();
    
    // Load the graph

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

void dllama_test::test_llama_add_edges() {
    char* database_directory = (char*) alloca(16);
    strcpy(database_directory, "db");
    
    database = new ll_database(database_directory);
    graph = database->graph();
    
    node_t src = graph->pick_random_node();
    node_t tgt = graph->pick_random_node();
    node_t temp = graph->pick_random_node();
    
    graph->add_edge(src,tgt);
    graph->add_edge(tgt,temp);
    graph->add_edge(src,temp);
    
    for (std::unordered_map<std::string, LL_CSR*>::const_iterator csrs_iterator = graph->ro_graph().csrs().begin(); 
            csrs_iterator != graph->ro_graph().csrs().end(); 
            csrs_iterator ++)
    {
        cout << csrs_iterator->first << "\n";
        cout << csrs_iterator->second << "\n";
    }
    
    cout << "num levels " << graph->num_levels() << "\n";
}

void dllama_test::test_llama_print_neighbours() {
    char* database_directory = (char*) alloca(16);
    strcpy(database_directory, "db");
    
    database = new ll_database(database_directory);
    graph = database->graph();
    
    node_t src = graph->pick_random_node();
    node_t tgt = graph->pick_random_node();
    
    cout << "node: " << src << " out degree: " << graph->out_degree(src) << "\n";
    
    graph->add_edge(src,tgt);
    
    cout << "node: " << src << " out degree: " << graph->out_degree(src) << "\n";
    
    graph->checkpoint();
}

void dllama_test::read_db() {
    
}

