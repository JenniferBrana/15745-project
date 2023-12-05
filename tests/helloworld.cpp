#include <pthread.h>
#include <stdio.h>

void* thread_func(void* arg) {
    printf("Hello world, from child thread\n");
    return NULL;
}

int main() {
    pthread_t thread;
    pthread_create(&thread, NULL, thread_func, NULL);
    printf("Hello world, from main thread\n");
    pthread_join(thread, NULL);
}
