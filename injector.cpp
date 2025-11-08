#include <windows.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include "cryptor\cryptor.h"

using namespace std;

#define read_to(stream_from, var) stream_from.read(reinterpret_cast<char*>(&var), sizeof(var))

void fatal(const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	vfprintf(stderr, format, argptr);
	va_end(argptr);
	exit(1);
}

int main(int argc, char const *argv[]) {
	fstream mainFileStream(".\\output\\main.exe", ios::binary | ios::in | ios::out);
	if (!mainFileStream.is_open()) {
		fatal("failed to open file");
	}


	// DOS HEADER
	// check DOS signature
	UINT16 DosSignature;
	read_to(mainFileStream, DosSignature);
	if (DosSignature != 23117) {
		fatal("expected dos signature (23117), got: %hu", DosSignature);
	}
	printf("DOS signature: %hu\n", DosSignature);

	// read where the PE header starts
	UINT32 PEStart;
	mainFileStream.seekg(0x3c, ios::beg);
	read_to(mainFileStream, PEStart);
	printf("PE start: %u\n", PEStart);
	


	// COFF HEADER
	// check PE signature
	UINT32 PESignature;
	mainFileStream.seekg(PEStart, ios::beg);
	read_to(mainFileStream, PESignature);
	if (PESignature != 17744) {
		fatal("expected PE signature (17744), got: %u", PESignature);
	}
	printf("PE Signature: %u\n", PESignature);

	// read the amount of sections
	UINT16 NumberOfSections;
	mainFileStream.seekg(PEStart + 4 + 2, ios::beg);
	read_to(mainFileStream, NumberOfSections);
	printf("number of sections: %hu\n", NumberOfSections);

	// read size of optional header
	UINT16 SizeOfOptionalHeader;
	mainFileStream.seekg(PEStart + 4 + 16, ios::beg);
	read_to(mainFileStream, SizeOfOptionalHeader);
	printf("size of optional header: %hu\n", SizeOfOptionalHeader);



	// SECTION TABLE
	// calculate the start of the section table
	unsigned int StartOfSectionsTable = PEStart + 24 + SizeOfOptionalHeader;
	printf("start of sections table: %u\n", StartOfSectionsTable);

	// read section table, take everything you need
	UINT32 ReplaceFilePosition;
	UINT32 ReplaceVirtualAddress;

	UINT32 PayloadFilePosition;
	UINT32 PayloadVirtualAddress;
	UINT32 PayloadVirtualSize;

	bool replaceFound = false;
	bool payloadFound = false;
	for (int i=0; i<NumberOfSections; i++) {
		// get the file position of the section in the section table
		unsigned int sectionHeaderFilePosition = StartOfSectionsTable + i*40;

		// read section name
		char sectionName[9];
		mainFileStream.seekg(sectionHeaderFilePosition, ios::beg);
		mainFileStream.read(sectionName, 8);
		sectionName[8] = '\0';

		UINT32 sectionVirtualSize;
		mainFileStream.seekg(sectionHeaderFilePosition + 8, ios::beg);
		read_to(mainFileStream, sectionVirtualSize);

		UINT32 sectionVirtualAddress;
		mainFileStream.seekg(sectionHeaderFilePosition + 12, ios::beg);
		read_to(mainFileStream, sectionVirtualAddress);

		// read the file position of the section itself
		UINT32 sectionFilePosition;
		mainFileStream.seekg(sectionHeaderFilePosition + 20, ios::beg);
		read_to(mainFileStream, sectionFilePosition);

		// printf("section: %s\n", sectionName);
		// printf(" - virtual size: %u\n", sectionVirtualSize);
		// printf(" - virtual address: %u\n", sectionVirtualAddress);
		// printf(" - file position: %u\n", sectionFilePosition);

		const char compare[9] = ".replace";
		if ( !strcmp(sectionName, ".replace") ) {
			replaceFound = true;
			ReplaceFilePosition = sectionFilePosition;
			ReplaceVirtualAddress = sectionVirtualAddress;
		} else if ( !strcmp(sectionName, ".payload") ) {
			payloadFound = true;
			PayloadFilePosition = sectionFilePosition;
			PayloadVirtualAddress = sectionVirtualAddress;
			PayloadVirtualSize = sectionVirtualSize;
		}
	}

	// check if we found all the sections we need
	if (!replaceFound)
		fatal("section with name .replace not found");
	if (!payloadFound)
		fatal("section with name .payload not found");


	// WRITE TO .replace
	// overwrite the values in the .replace section
	unsigned PayloadCryptKey = time(0);
	INT32 VirtualOffset = PayloadVirtualAddress - ReplaceVirtualAddress;

	mainFileStream.seekp(ReplaceFilePosition, ios::beg);
	mainFileStream.write(reinterpret_cast<char*>(&VirtualOffset), sizeof VirtualOffset);
	mainFileStream.write(reinterpret_cast<char*>(&PayloadVirtualSize), sizeof PayloadVirtualSize);
	mainFileStream.write(reinterpret_cast<char*>(&PayloadCryptKey), sizeof PayloadCryptKey);
	mainFileStream.write(reinterpret_cast<char*>(&ReplaceFilePosition), sizeof ReplaceFilePosition);
	mainFileStream.write(reinterpret_cast<char*>(&PayloadFilePosition), sizeof PayloadFilePosition);
	
	printf("offset: %u\n", VirtualOffset);
	printf("payload size: %u\n", PayloadVirtualSize);
	printf("payload key: %u\n", PayloadCryptKey);
	printf(".replace file position: %u\n", ReplaceFilePosition);
	printf(".payload file position: %u\n", PayloadFilePosition);

	// ENCRYPT PAYLOAD SECTION
	// read payload
	auto payloadData = (char*) malloc(PayloadVirtualSize);
	mainFileStream.seekg(PayloadFilePosition, ios::beg);
	mainFileStream.read(payloadData, PayloadVirtualSize);

	printf("payload data:\n");
	for (int i=0; i<PayloadVirtualSize; i++) {
		printf( "%hhx ", *(payloadData+i) );
	}
	cout << "\n";

	// encrypt payload
	crypt_array(payloadData, PayloadVirtualSize, PayloadCryptKey, nullptr);

	printf("encrypted data:\n");
	for (int i=0; i<PayloadVirtualSize; i++) {
		printf( "%hhx ", *(payloadData+i) );
	}
	cout << "\n";

	// write encrypted payload back to file
	mainFileStream.seekp(PayloadFilePosition, ios::beg);
	mainFileStream.write(payloadData, PayloadVirtualSize);
	free(payloadData);

	mainFileStream.close();

	return 0;
}
