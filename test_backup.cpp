#pragma comment(lib, "User32.lib")

#include <windows.h>
#include <windef.h>
#include <iostream>
#include <processthreadsapi.h>
#include <handleapi.h>
#include <Psapi.h>

auto address = (unsigned char *)0x041c000;

void closeProcess(PROCESS_INFORMATION pi) {
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

int main(int argc, char const *argv[])
{
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if ( !CreateProcessW(L".\\output\\main.exe", NULL, NULL, NULL, true, 0, NULL, NULL, &si, &pi) ) {
		std::cerr << "couldnt create process";
		return 1;
	}

	WCHAR _processName[300];
	DWORD used_size = GetProcessImageFileNameW(pi.hProcess, _processName, std::size(_processName));
	auto processName = new WCHAR[used_size];
	memcpy(processName, _processName, used_size * sizeof(WCHAR));
	
	printf("process id: %i\n", pi.dwProcessId);
	printf("process name: %ls\n", processName);
	printf("name length: %i\n", used_size);
	
	// SuspendThread(pi.hThread);
	Sleep(100);

	HMODULE hMods[1500];
	DWORD needed;
	DWORD size = sizeof(hMods);
	if (!EnumProcessModulesEx(pi.hProcess, hMods, size, &needed, LIST_MODULES_ALL) ) {
		std::cerr << "error getting process modules\n";
		std::cerr << GetLastError();
		closeProcess(pi);
		return 1;
	}

	if ( sizeof(hMods) < needed ) {
		std::cerr << "not enough space for hmods";
		closeProcess(pi);
		return 1;
	}

	for (int i=0; i < needed/sizeof(HMODULE); i++) {
		WCHAR moduleName[MAX_PATH];
		if (!GetModuleFileNameExW(&pi.hProcess, hMods[i], moduleName, MAX_PATH)) {
			printf("couldnt get module name: %i\n", GetLastError());
			printf("name: %ls\n", moduleName);
			continue;
		}
		printf("- %ls\n", moduleName);
	}

	closeProcess(pi);

	return 0;
}
