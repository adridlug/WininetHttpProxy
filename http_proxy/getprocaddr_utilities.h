#pragma once
#include<winternl.h>

inline PIMAGE_DOS_HEADER GetKernel32DosHeader() {
	__readgsqword(0x60);
	PPEB peb = (PPEB)__readgsqword(0x60);
	PLDR_DATA_TABLE_ENTRY kernel32Entry = CONTAINING_RECORD(peb->Ldr->InMemoryOrderModuleList.Flink->Flink->Flink, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
	return (PIMAGE_DOS_HEADER)kernel32Entry->DllBase;
}

inline PIMAGE_NT_HEADERS64 GetKernel32NtHeader(PIMAGE_DOS_HEADER kernel32DosHeader) {
	return (PIMAGE_NT_HEADERS64)((byte*)kernel32DosHeader + kernel32DosHeader->e_lfanew);
}

inline FARPROC GetProcAddressFunctionAddress() {

	PIMAGE_DOS_HEADER kernel32DosHeader = GetKernel32DosHeader();
	PIMAGE_NT_HEADERS64 kernel32NtHeader = GetKernel32NtHeader(kernel32DosHeader);

	PIMAGE_EXPORT_DIRECTORY kernel32ExportsTable = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)kernel32DosHeader + kernel32NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	DWORD* kernel32addressOfFunctions = (DWORD*)((BYTE*)kernel32DosHeader + kernel32ExportsTable->AddressOfFunctions);
	DWORD* kernel32addressOfNames = (DWORD*)((BYTE*)kernel32DosHeader + kernel32ExportsTable->AddressOfNames);
	WORD* kernel32addressOfNameOrdinals = (WORD*)((BYTE*)kernel32DosHeader + kernel32ExportsTable->AddressOfNameOrdinals);

	for (DWORD i = 0; i <= kernel32ExportsTable->NumberOfFunctions; i++) {

		ULONG64 GetProcA = 0x41636F7250746547;

		if (*(ULONG64*)((byte*)kernel32DosHeader + kernel32addressOfNames[i]) == GetProcA) {
			return (FARPROC)((size_t)kernel32DosHeader + kernel32addressOfFunctions[kernel32addressOfNameOrdinals[i]]);
		}
	}
}