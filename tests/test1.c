int main() {
    int acc = 0;
    for (int i = 0; i < 100; i ++) {
        acc += i * 7 - 3;
    }

    for (int i = 0; i < 100; i += 2) {
        acc += i * 7 - 3;
    }

    int j = 0;
    while (j < 50) {
        j += 2;
        acc += j * 3;
    }
    

    return acc + j;
}
