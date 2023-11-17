int main() {
    int acc = 0;
    for (int i = 0; i <= 100; i ++) {
        acc += i * 7 - 3;
    }

    for (int i = 0; i < 100; i += 2) {
        acc += i * 7 - 3;
    }

    int j = 10;
    while (j > 0) {
        j -= 3;
        acc += j * 3;
    }
    

    return acc + j;
}
