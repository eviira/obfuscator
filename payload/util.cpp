#pragma once
#include "payload.h"

void close_handles(std::initializer_list<HANDLE*> handle_list) {
	for (auto handle : handle_list) {
		if (*handle == INVALID_HANDLE_VALUE)
			continue;
		
		CloseHandle(*handle);
		*handle = INVALID_HANDLE_VALUE;
	}
}

void close_sockets(std::initializer_list<SOCKET*> socket_list) {
	for (auto socket : socket_list) {
		if (*socket == INVALID_SOCKET)
			continue;
		
		closesocket(*socket);
		*socket = INVALID_SOCKET;
	}
}