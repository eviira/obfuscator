#pragma once

// windows
// so winsock2 and 1.1 dont collide
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// lib for winsock2
#pragma comment(lib, "Ws2_32.lib")

// common stdlib
#include <stdexcept>
#include <iostream>

// payload entry
int startPayload(int argc, const char *argv[]);
