#include <cstdlib>
//#include <stdio.h>

struct LList {
    int value;
    struct LList* next;
};

typedef struct LList LList;

LList* cons(int value, LList* next) {
    LList* l = new LList;//(LList*) malloc(sizeof(LList));
    l->value = value;
    l->next = next;
    return l;
}

/*void unalloc(LList* todel) {
    while (todel) {
        LList* tmp = todel;
        todel = todel->next;
        delete tmp;
    }
}*/

/*int length1(LList* l) {
    int len = 0;
    while (l) {
        l = l->next;
        ++len;
    }
    return len;
}*/

int sum(LList* l, int total) {
    // Upwards: l, total
    // Downwards: x, total
    int x;
    //int total = 0;
    while (l) {
        x = 5 + total;
        total += l->value;
        l = l->next;
    }
    return x + total;// total;
}

/*int length2(LList* l) {
    int len = 0;
    while (l) {
        ++len;
        l = l->next;
    }
    return len;
}*/


/*void append(LList* a, LList* b) {
    while (a->next) {
        a = a->next;
    }
    a->next = b;
}

LList* enumerate(int max) {
    LList* l;
    while (max > 0) {
        --max;
        l = cons(max, l);
    }
    return l;
}*/


int main() {
    //unalloc(enumerate(30));

    //int j = 0;
    //for (int i = 0; i < 10; ++i) {
    //    j += 2*i + 3;
    //}
    return 0;
}
