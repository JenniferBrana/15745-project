#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <cstdlib>

#include <atomic>
//#include <cstdarg>
//#include <cstdio>
//#include <cstdlib>
//#include <iostream>
//#include <mutex>
#include "uli.h"

typedef void (*FunctionPtr)(int* val);

volatile int handlerFuncIncomplete = 1;
//volatile bool firstReq = true;
volatile int threadUninitialized = 1; // a global barrier



extern "C" {

    void handler_trampoline(); // call into assembly trampoline

    uint64_t send_request_uli(uint64_t target, void* addr, void* dataAddr) {
        //printf("send_request_uli(%llu, %lu, %lu)\n", (unsigned long long) target, (unsigned long) addr, (unsigned long) dataAddr);
        // if (firstReq) {
        //     firstReq = false;
        // } else {
        //     handlerFuncIncomplete += 1;
        // }
        return uli_send_req_fx_addr_data(target, addr, dataAddr);
    }

    void handler_func() {
        //printf("handler_func called!\n");
        //printf("Core %ld: I received an ULI!\n", core_id());
        uint64_t raw_fx_addr = read_fx_addr();
        //printf("raw_fx_addr = %llu\n", (unsigned long long) raw_fx_addr);
        //printf("Address received: %p\n", raw_fx_addr);

        FunctionPtr funcPtr = (FunctionPtr)(uintptr_t)raw_fx_addr;
        int* ptr = reinterpret_cast<int*>(read_addr());

        //printf("about to call %lu with %lu\n", (unsigned long) &funcPtr, (unsigned long) ptr);
        funcPtr(ptr); //call the passed function

        // handlerFuncIncomplete--;
        uli_send_resp(2);
    }

    void* thread_func(void*) {
        //printf("thread_func called!\n");
        uli_init();
        //printf("uli_init()\n");
        uli_set_handler((void*)&handler_trampoline);
        //printf("uli_set_handler()\n");
        uli_enable();
        //printf("uli_enable()\n");

        //printf("child thread setup\n");

        // notify core 0 to interrupt me
        threadUninitialized--;

        // wait until the handler to unset x
        while (handlerFuncIncomplete) {
            //printf("waiting for handler function to complete\n");
        }
        //printf("handler function complete\n");

        uli_disable();
        //printf("uli_disable()\n");

        return NULL;
    }
    
    unsigned long main_start() {

        //printf("main_start called!\n");
        pthread_t thread;
        //printf("declared thread\n");
        int failed = pthread_create(&thread, NULL, thread_func, NULL);
        if (failed) {
            exit(failed);
        }
        //printf("created pthread\n");
        // wait thread 1 to finish setup
        while (threadUninitialized) {
            //printf("waiting for thread function to initialize\n");
        }
        //printf("thread initialized!\n");
        //printf("Thread id = %lu\n", (unsigned long) thread);
        return thread;
    }

    void main_end(unsigned long thread) {
        handlerFuncIncomplete--;
        pthread_join(thread, NULL);
    }
}


/*void main_end(pthread_t thread) {
    pthread_join(*((pthread_t*) thread), NULL);
}*/
