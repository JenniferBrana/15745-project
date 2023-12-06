
int a[15] = {1,2,3,4,5, 6, 7, 8, 9, 10, 11, 12, 13,14};

int main() {
    int total = 0;
    for (int i = 0; i < 15; i++) {
        total += a[3 / i];
    }
    return total;
}
