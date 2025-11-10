#include <iostream>

using namespace std;

int main() {
	unsigned char A = 0xf0;
	unsigned char B = 0xfa;
	printf("%hhx %hhx %hhx %hhx\n", A, B, A+B, A-B);
	return 0;
}