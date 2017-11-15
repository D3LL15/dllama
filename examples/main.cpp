/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.cpp
 * Author: dan
 *
 * Created on 29 October 2017, 15:42
 */

#include "dllama.h"

/*
 * 
 */
int main(int argc, char** argv) {
    dllama x = dllama();
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

