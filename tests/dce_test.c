int main(int argc, char **argv) {
    int x = 1;
    int a = x + 5;
    int b = a + 6;
    int c = b;
    int d;

    if (a > 5) {
        c = a * 50;
        d = a + 6;
    } else {
        c = a + 50;
        d = a + 66;
    }

    int e = a - 40;
    int f = e + c;
    return 0;
}
