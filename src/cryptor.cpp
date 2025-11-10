#include <stdlib.h>

// encrypting and decrypting are the exact same operation
void crypt_array(char *data, size_t length, unsigned rand_seed, unsigned *next_rand) {
	srand(rand_seed);
	for (int i=0; i<length; i++) {
		char random_value = rand();
		data[i] = data[i] ^ random_value;
	}

	if (next_rand != nullptr) {
		*next_rand = rand();
	}
}
