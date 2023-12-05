#include <pthread.h>
#include <unistd.h>

#include <atomic>
//#include <cstdarg>
//#include <cstdio>
//#include <cstdlib>
//#include <iostream>
//#include <mutex>
#include "uli.h"

typedef void (*FunctionPtr)(int* val);

//volatile int handlerFuncIncomplete = 1;
//std::atomic<int> threadUninitialized(1); // a global barrier

extern "C" {

    volatile int handlerFuncIncomplete = 1;
    volatile int threadUninitialized = 1;

    void handler_trampoline(); // call into assembly trampoline

    void handler_func() {
        //printf("Core %ld: I received an ULI!\n", core_id());
        uint64_t raw_fx_addr = read_fx_addr();
        //printf("Address received: %p\n", raw_fx_addr);

        FunctionPtr funcPtr = (FunctionPtr)(uintptr_t)raw_fx_addr;
        int* ptr = reinterpret_cast<int*>(read_addr());

        funcPtr(ptr); //call the passed function

        handlerFuncIncomplete--;
        uli_send_resp(2);
    }

    void* thread_func(void*)
    {
        uli_init();
        uil_set_handler((void*)&handler_trampoline);
        uli_enable();

        //printf("child thread setup\n");

        threadUninitialized--; // notify core 0 to interrupt me

        while (handlerFuncIncomplete); // wait until the handler to unset x

        uli_disable();

        return NULL;
    }
    
    void main_start() {
        pthread_t thread;
        pthread_create(&thread, NULL, thread_func, NULL);
        while (threadUninitialized); // wait thread 1 to finish setup
        //return &thread;
    }
}


/*void main_end(pthread_t thread) {
    pthread_join(*((pthread_t*) thread), NULL);
}*/
