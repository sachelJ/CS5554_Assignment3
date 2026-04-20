int test(int x, int y) {
    int sum = 0;
    for (int i = 0; i < 1000; i++) {
        int t = x * y;
        sum += t;
    }
    return sum;
}
int main() { return test(3, 4); }
