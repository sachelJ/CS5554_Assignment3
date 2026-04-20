int test(int x) {
    int y = 0;
    for (int i = 0; i < 100; i++) {
        int t = x * 2;
        y += t;
    }
    return y;
}

int main() {
    return test(3);
}
