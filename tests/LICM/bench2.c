int test(int a, int b, int c) {
    int sum = 0;
    for (int i = 0; i < 1000; i++) {
        int x = a + b;
        int y = b * c;
        sum += x + y + i;
    }
    return sum;
}
int main() { return test(2, 3, 4); }
