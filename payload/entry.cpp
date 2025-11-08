#include "payload.h"
#include "stdlib.h"
#include "stdio.h"

#pragma section(".payload", read, write, execute)
#pragma comment(linker, "/SECTION:.payload,ERW")
#pragma code_seg(".payload")

void print_whatever() {
	printf("mraow\n");
}

int start_payload(int argc, char const *argv[]) {
	print_whatever();
	return 0;
}
