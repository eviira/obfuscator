#include <libssh/libssh.h>
#include <libssh/server.h>
#include <libssh/callbacks.h>

#include <stdexcept>
#include <cassert>
#include <iostream>

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
	// 	std::cerr << "code: " << code << "\n";
	// 	throw std::runtime_error("generating key");		
	// }
	if (ssh_pki_generate(SSH_KEYTYPE_ECDSA, 256, &key)) {
		throw std::exception("generting key");
	}
	std::cout << "key sucessfully imported" << "\n";

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
	std::cerr << "!! ssh internal error messages:\n";
	if (bind != nullptr) {
		std::cout << "- bind: " << ssh_get_error(bind) << "\n";
	}
	if (session != nullptr) {
		std::cout << "- session: " << ssh_get_error(session) << "\n";
	}
	if (channel != nullptr) {
		std::cout << "- channel: " << ssh_get_error(channel) << "\n";
	}
	if (event != nullptr) {
		std::cout << "- event: " << ssh_get_error(event) << "\n";
	}
}

static int copy_fd_to_chan(socket_t fd, int revents, void *userdata) {
    ssh_channel chan = (ssh_channel)userdata;
    char buf[2048];

    if(!chan) {
		std::cerr << "$ fd closing\n";
        // close(fd);
        return -1;
    }
    if(revents & POLLIN) {
		std::cout << "$ reading fd\n";
        // sz = read(fd, buf, 2048);
        // if(sz > 0) {
        //     ssh_channel_write(chan, buf, sz);
        // }
    }
    if(revents & POLLHUP) {
		std::cout << "$ fd->chan closing 2\n";
        ssh_channel_close(chan);
        return -1;
    }
	std::cout << "$ fd->chan leaving\n";
    return 0;
}

static int copy_chan_to_fd(ssh_session session,
	ssh_channel channel,
	void *data,
	uint32_t len,
	int is_stderr,
	void *userdata) {
	int fd = *(int*)userdata;
	int sz;
	(void)session;
	(void)channel;
	(void)is_stderr;

	std::cout << "$ chan->fd: " << data << "\n";
	// sz = write(fd, data, len);
	return len;
}

static void chan_close(ssh_session session, ssh_channel channel, void *userdata) {
    int fd = *(int*)userdata;
    (void)session;
    (void)channel;

	std::cout << "$ chan closing\n";
    // close(fd);
}

void SSHServer::handleShell() {
	std::cout << "handling shell!\n";

	std::cout << "creating event..\n";
	event = ssh_event_new();
	if (event == NULL) {
		printInternalErrors();
		throw std::runtime_error("creating event");
	}

	socket_t fd = 100;

	std::cout << "setting channel callbacks..\n";
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

	std::cout << "adding fd to event..\n";
	auto events = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;
	if (ssh_event_add_fd(event, fd, events, copy_fd_to_chan, channel) != SSH_OK) {
		// printInternalErrors();
		throw std::runtime_error("adding fd event");
	}

	std::cout << "adding event to session..\n";
	if (ssh_event_add_session(event, session)) {
		printInternalErrors();
		throw std::runtime_error("adding session to event");
	}

	std::cout << "sitting in event dopoll loop..\n";
	while (!ssh_channel_is_closed(channel)) {
		auto code = ssh_event_dopoll(event, -1);
		if (code) {
			printInternalErrors();
			std::cout << "code: " << code << "\n";
			throw std::runtime_error("event poll");
		}
	}
	std::cout << "exited shell..\n";

	ssh_event_remove_session(event, session);
	ssh_event_remove_fd(event, fd);
	ssh_remove_channel_callbacks(channel, &cb);
}

void SSHServer::start() {
	std::cout << "listening.." << "\n";
	if (ssh_bind_listen(bind)) {
		printInternalErrors();
		throw std::runtime_error("start listening");
	}

	std::cout << "waiting connection.." << "\n";
	if (ssh_bind_accept(bind, session)) {
		printInternalErrors();
		throw std::runtime_error("accepting connection");
	}

	std::cout << "key exchange.." << "\n";
	if (ssh_handle_key_exchange(session)) {
		printInternalErrors();
		throw std::runtime_error("key exchange");
	}

	while ( true ) {
		std::cout << "waiting next message.." << "\n";
		ssh_message msg = ssh_message_get(session);
		if (msg == NULL) {
			std::cerr << ssh_get_error(session) << "\n";
			throw std::runtime_error("getting message");
		}

		auto type = ssh_message_type(msg);
		auto subType = ssh_message_subtype(msg);
		std::cout << "- message type: " << type << "\n";
		std::cout << "- message sub-type: " << subType << "\n";

		switch (type) {
			case SSH_REQUEST_SERVICE: {
				auto serviceName = ssh_message_service_service(msg);
				std::cout << "- service name: " << serviceName << "\n";

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
				}
				break;
			}
		}

		ssh_message_free(msg);
	}
}
