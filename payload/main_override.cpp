// this file exists so you can compile payload without invoking main and injector

#include "./payload.cpp"

int main(int argc, const char *argv[]) {
	return startPayload(argc, argv);
}
