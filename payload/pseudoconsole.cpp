#pragma once

#include "payload.h"
#include "./util.cpp"
#include <thread>
#include <list>

HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEXA* psi)
{
    // Prepare Startup Information structure
    STARTUPINFOEXA si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEXA);

    // Discover the size required for the list
    SIZE_T bytesRequired;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

    // Allocate memory to represent the list
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
    if (!si.lpAttributeList)
    {
        return E_OUTOFMEMORY;
    }

    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired))
    {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Set the pseudoconsole information into the list
    auto result = UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), NULL, NULL);
    if (!result) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *psi = si;

    return S_OK;
}

class PTY {
    HPCON pseudoConsoleHandle = INVALID_HANDLE_VALUE;

    PROCESS_INFORMATION processInformation;

    void serverSocketThread();

public:
    SOCKET localServerSocket = INVALID_SOCKET;
    SOCKET localClientSocket = INVALID_SOCKET;

    // communication channels
    HANDLE outputReadSide = INVALID_HANDLE_VALUE;
    HANDLE inputWriteSide = INVALID_HANDLE_VALUE;

    ~PTY();
    
    void close();
    bool isClosed();

    HRESULT createPseudoConsole();
    // to use the PTY in the ssh server, the handle needs to be of type SOCKET
    // easiest way to do that is to just create a local server-client socket pair that we feed our communications through
    // as far as I can tell this is just because libssh was mainly made for linux, and that has identical I/O interfaces for network connections and files
    void createLocalSockets();
};

PTY::~PTY() {
    close();
}

void PTY::close() {
    // runtime called abort when I accidentally called ClosePseudoConsole multiple times

    // close pseudoconsole
    if (pseudoConsoleHandle != INVALID_HANDLE_VALUE) {
        ClosePseudoConsole(pseudoConsoleHandle);
        pseudoConsoleHandle = INVALID_HANDLE_VALUE;
    }
    
    // close handles
    close_handles({&outputReadSide, &inputWriteSide});
    
    // close sockets
    close_sockets({&localServerSocket, &localClientSocket});
}

bool PTY::isClosed() {
    return pseudoConsoleHandle == INVALID_HANDLE_VALUE;
}

HRESULT PTY::createPseudoConsole() {
	COORD size;
	size.X = 200;
	size.Y = 100;
    
    // these 2 handles are closed after creating the process
    // inputWriteSide and outputReadSide are class members
    HANDLE inputReadSide, outputWriteSide;
    if (!CreatePipe(&inputReadSide, &inputWriteSide, NULL, 0)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    if (!CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    HRESULT result;
    result = CreatePseudoConsole(size, inputReadSide, outputWriteSide, 0, &pseudoConsoleHandle);
    if FAILED(result) {
        return result;
    }

    STARTUPINFOEXA startupInfo;
    result = PrepareStartupInformation(pseudoConsoleHandle, &startupInfo);
    if (FAILED(result)) {
        return result;
    }

    ZeroMemory(&processInformation, sizeof(processInformation));

    // Call CreateProcess
    if (!CreateProcessA("C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
                        NULL,
                        NULL,
                        NULL,
                        FALSE,
                        EXTENDED_STARTUPINFO_PRESENT,
                        NULL,
                        NULL,
                        &startupInfo.StartupInfo,
                        &processInformation))
    {
        HeapFree(GetProcessHeap(), 0, startupInfo.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // free no longer needed memory
    HeapFree(GetProcessHeap(), 0, startupInfo.lpAttributeList);
    // documentation reccomends closing these handles after creating the process
    close_handles({&inputReadSide, &outputWriteSide});

    return S_OK;
}

void PTY::createLocalSockets() {
    // create hints
    ADDRINFOA hints;
    ZeroMemory(&hints, sizeof (hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    // hints.ai_flags = AI_PASSIVE;

    PADDRINFOA addressInfo;
    auto errAddress = getaddrinfo("localhost", "6000", &hints, &addressInfo);
    if (errAddress) {
        std::cerr << "address info: " << errAddress << std::endl;
        throw std::runtime_error("getting address info");
    }

    // create server socket
    localServerSocket = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol);
    if (localServerSocket == INVALID_SOCKET) {
        std::cerr << WSAGetLastError() << std::endl;
        throw std::runtime_error("creating server socket");
    }

    // create client socket
    localClientSocket = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol);
    if (localClientSocket == INVALID_SOCKET) {
        std::cerr << WSAGetLastError() << std::endl;
        throw std::runtime_error("creating client socket");
    }

    // bind server socket
    auto errBind = bind(localServerSocket, addressInfo->ai_addr, addressInfo->ai_addrlen);
    if (errBind) {
        std::cerr << "server bind" << WSAGetLastError() << std::endl;
        throw std::runtime_error("creating server bind");
    }
    // set server socket to listen
    auto errListen = listen(localServerSocket, 1);
    if (errListen) {
        std::cerr << "server listen: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("server listen");
    }

    // run the server in the background
    std::thread serverThread(&PTY::serverSocketThread, this);
    serverThread.detach();

    // connect client socket to the server
    auto errConnect = connect(localClientSocket, addressInfo->ai_addr, addressInfo->ai_addrlen);
    if (errConnect) {
        std::cerr << "client attempt connect: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("connecting with client");
    }
}

void PTY::serverSocketThread() {
    SOCKET connection = accept(localServerSocket, NULL, NULL);
    if (connection == INVALID_SOCKET) {
        std::cerr << "server accept: " << WSAGetLastError() << std::endl;
        throw std::runtime_error("accepting connection");
    }

    // we need to both listen for messages from the client and read from the pseudoconsole output
    // listen to messages from the client
    std::thread networkListen( [this, connection]() {
        const size_t bufferSize = 512;
        char buffer[bufferSize];

        while ( !isClosed() ) {
            auto bytesRead = recv(connection, buffer, bufferSize, 0);
            if (bytesRead == 0 || WSAGetLastError() == 10054) {
                return;
            }
            if (bytesRead <= 0) {
                std::cerr << "server recv" << std::endl;
                std::cerr << "recv return: " << bytesRead << std::endl;
                std::cerr << "recv err: " << WSAGetLastError() << std::endl;
                return;
            }

            DWORD bytesWritten;
            auto writeSuccess = WriteFile(inputWriteSide, buffer, bytesRead, &bytesWritten, NULL);
            if (!writeSuccess) {
                std::cerr << "server write to pty" << std::endl;
                std::cerr << "write to pty: " << GetLastError() << std::endl;
                std::cerr << "code: " << bytesWritten << std::endl;
                return;
            }
        }
    } );

    // listen for writes from the pty
    std::thread ptyRead( [this, connection]() {
        const size_t bufferSize = 512;
        char buffer[bufferSize];

        while ( !isClosed() ) {
            // ReadFile does not seem to emit a EOF when using the exit command
            DWORD bytesRead;
            auto readSuccess = ReadFile(outputReadSide, buffer, bufferSize, &bytesRead, NULL);
            if (!readSuccess) {
                auto err = GetLastError();
                if (err != ERROR_HANDLE_EOF && err != ERROR_INVALID_HANDLE) {
                    std::cerr << "read shell: " << err << std::endl;
                }
                break;
            }

            auto bytesSent = send(connection, buffer, bytesRead, 0);
            if (bytesSent <= 0) {
                std::cerr << "server send to client" << std::endl;
                std::cerr << "send to client: " << GetLastError() << std::endl;
                std::cerr << "code: " << bytesSent << std::endl;
                return;
            }
        }
    } );

    // waits for the shell to exit, and then forces it to close
    auto result = WaitForSingleObject(processInformation.hProcess, INFINITE);
    close();

    networkListen.join();
    ptyRead.join();
}
