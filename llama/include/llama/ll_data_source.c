#include"llama/ll_data_source.h"

virtual bool ll_simple_data_source::pull(ll_writable_graph* graph, size_t max_edges) {

    size_t num_stripes = omp_get_max_threads();
    ll_la_request_queue* request_queues[num_stripes];

    for (size_t i = 0; i < num_stripes; i++) {
        request_queues[i] = new ll_la_request_queue();
    }

    bool loaded = false;
    size_t chunk = num_stripes <= 1
        ? std::min<size_t>(10000ul, max_edges)
        : max_edges;

    while (true) {

        graph->tx_begin();
        bool has_data = false;

        for (size_t i = 0; i < num_stripes; i++)
            request_queues[i]->shutdown_when_empty(false);

#pragma omp parallel
        {
            if (omp_get_thread_num() == 0) {

                has_data = this->pull(request_queues, num_stripes, chunk);

                for (size_t i = 0; i < num_stripes; i++)
                    request_queues[i]->shutdown_when_empty();
                for (size_t i = 0; i < num_stripes; i++)
                    request_queues[i]->run(*graph);
            }
            else {
                int t = omp_get_thread_num();
                for (size_t i = 0; i < num_stripes; i++, t++)
                    request_queues[t % num_stripes]->worker(*graph);
            }
        }

        graph->tx_commit();

        if (has_data)
            loaded = true;
        else
            break;
    }

    for (size_t i = 0; i < num_stripes; i++) delete request_queues[i];

    return loaded;
}
