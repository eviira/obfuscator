// this file exists to compile payload without invoking main and injector

#include "./entry.cpp"

int main(int argc, const char *argv[]) {
	return start_payload(argc, argv);
}