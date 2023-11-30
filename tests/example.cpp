#include <malloc.h>
#include <stdio.h>

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

int foo(LList* list, int k) {
    int total = 0;
    // ...
    while (list != NULL) {
        total += list->value * k;
        list = list->next;
    }
    return total;
}

/* ================================================== */

int main() {
    return foo(enumerate(10), 3);
}
