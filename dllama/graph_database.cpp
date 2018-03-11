#include "graph_database.h"

using namespace dllama_ns;

graph_database::~graph_database() {
	delete database;
}

