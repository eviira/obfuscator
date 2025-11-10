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

// seemingly doesnt automatically get set back to .text
// without this line, a bunch of builtin functions get encrypted and prevent the program from launching properly
#pragma code_seg(".text")