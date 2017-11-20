#include "dllama_test.h"

int main(int argc, char** argv) {
    dllama_test x = dllama_test();
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
        }
    }
    //x.test_llama_interact();
    return 0;
}

