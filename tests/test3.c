#include <stdlib.h>
#include <stdio.h>

struct LList {
    int value;
    struct LList* next;
};

typedef struct LList LList;

LList* cons(int value, LList* next) {
    LList* l = (LList*) malloc(sizeof(LList));
    l->value = value;
    return l;
}

void delete(LList* l) {
    if (l) {
        LList* next = l->next;
        free(l);
        delete(next);
    }
}

int length1(LList* l) {
    int len = 0;
    while (l) {
        l = l->next;
        ++len;
    }
    return len;
}

int length2(LList* l) {
    int len = 0;
    while (l) {
        ++len;
        l = l->next;
    }
    return len;
}


void append(LList* a, LList* b) {
    while (a->next) {
        a = a->next;
    }
    a->next = b;
}

LList* enumerate(int max) {
    LList* l = NULL;
    while (max > 0) {
        --max;
        l = cons(max, l);
    }
    return l;
}


int main() {
    delete(enumerate(30));
    return 0;
}
