int test(int x) {
    int sum = 0;
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            int t = x * 2;
            sum += t + i;
        }
    }
    return sum;
}
int main() { return test(5); }
