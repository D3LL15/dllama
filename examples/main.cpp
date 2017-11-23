#include "dllama_test.h"
#include "dllama.h"
#include "snapshot_merger.h"

int main(int argc, char** argv) {
    dllama_test x = dllama_test();
    //dllama y = dllama();
    //y.load_SNAP_graph();
    if (argc == 2) {
        switch (*argv[1]) {
            case '1':
                x.full_test();
                break;
            case '2':
                x.test_llama_init();
                break;
            case '3':
                x.test_llama_print_neighbours();
                break;
            case '4':
                dllama y = dllama();
                y.load_SNAP_graph();
                break;
        }
    }
    //x.test_llama_interact();
    return 0;
}

