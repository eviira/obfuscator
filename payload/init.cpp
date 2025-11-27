#include "payload.h"

// winsock needs to be explicitly initilized before the first call
// and deinitilized after the last call

void payloadInit() {
	// start winsock
	WSADATA WSAData;
    auto errorCode = WSAStartup(MAKEWORD(2,2), &WSAData);
    if (errorCode) {
        std::cerr << "WSAStartup err code: " << errorCode << std::endl;
        throw std::runtime_error("starting winsock2.2");
    }

	// Redirect CRT error reports to STDERR  
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);  
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
}

void payloadClean() {
	// stop winsock
	if (WSACleanup()) {
        std::cerr << "WSACleanup: " << WSAGetLastError() << std::endl;
    }
}
