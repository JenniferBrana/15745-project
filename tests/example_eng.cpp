//#include <malloc>
//#include <cstdio>
//#include <cstdlib>
//#include <stdlib.h>
#include <cstdlib>
#include <cstdio>
//#include <stdlib.h>

struct LList {
    int value;
    struct LList* next;
};

typedef struct LList LList;

LList* cons(int value, LList* next) {
    LList* l = (LList*) malloc(sizeof(LList));
    l->value = value;
    l->next = next;
    return l;
}

LList* enumerate(int max) {
    LList* l = NULL;
    while (max > 0) {
        --max;
        l = cons(max, l);
    }
    return l;
}

/* ================================================== */

struct Data {
    int k;
    int total;
    LList* list;
};

void eng_func(void* d) {
    struct Data* data = (struct Data*) d;
    int k = data->k;
    int total = data->total;
    LList* list = data->list;
    while (list != NULL) {
        total += list->value * k;
        list = list->next;
    }
    data->k = k;
    data->total = total;
    data->list = list;
}

extern void uli_send_req_fx_addr(int, void*, void*);

int foo(LList* list, int k) {
    int total = 0;
    // ...
    struct Data data;
    data.k = k;
    data.total = total;
    data.list = list;
    uli_send_req_fx_addr_data(1, (void*) &eng_func, (void*) &data);
    k = data.k;
    total = data.total;
    list = data.list;
    return total;
}

/* ================================================== */

int main() {
    printf("%d\n", foo(enumerate(10), 3));
}
