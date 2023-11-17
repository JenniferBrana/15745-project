#include <string.h>

int strsum(char* s) {
    unsigned int total = 0;
    unsigned int l = strlen(s);
    for (unsigned int i = 0; i < l; ++i) {
        total += (unsigned int) s[i];
    }
    return total;
}

int main(int argc, char** argv) {
    unsigned int total = 0;
    for (int i = 0; i < argc; i++) {
        total += (unsigned int) strsum(argv[i]);
    }
    return total;
}
