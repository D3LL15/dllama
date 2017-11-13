/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   dllama.h
 * Author: dan
 *
 * Created on 07 November 2017, 16:54
 */

#ifndef DLLAMA_H
#define DLLAMA_H

#include "llama.h"

class dllama {
public:
    dllama();
    virtual ~dllama();
    void test_llama_init();
    void test_llama_interact();
    void full_test();
private:
protected:
    ll_writable_graph* graph;
    ll_database database;
};

#endif /* DLLAMA_H */

