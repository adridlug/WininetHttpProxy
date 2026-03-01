#pragma once
#include <Windows.h>
#include "http_backend.h"
#include "getprocaddr_utilities.h"

typedef struct _procName{
	ULONG64 t0, t1, t2;
} _procName;

typedef struct _longstring {
	ULONG64 t0, t1, t2, t3, t4;
} _longstring;

typedef struct _proc{
	_procName procName;
	FARPROC procAddr;
	struct _proc *next;
} _proc;

inline BOOL cmpProcNames(_procName p1, _procName p2) {
	return (p1.t0 == p2.t0 && p1.t1 == p2.t1 && p1.t2 == p2.t2);
}

inline FARPROC getProcAddr(_procName procName, _proc* procList) {
	_proc* proc = procList;

	while (proc != NULL) {
		if (cmpProcNames(proc->procName, procName)) {
			return proc->procAddr;
		}

		proc = proc->next;
	}

	return NULL;
}

inline unsigned CustomStrlen(const char* str)
{
	int cnt = 0;
	if (!str)
		return 0;
	for (; *str != 0x00; ++str)
		++cnt;
	return cnt;
}

inline int CustomStprintf_s(LPTSTR szDest, unsigned int len, LPCTSTR szFmt, ...)
{
	va_list marker;
	int out_len;

	va_start(marker, szFmt);
	out_len = CustomStprintf_s(szDest, len, szFmt, marker);
	va_end(marker);
	return out_len;
}

char* Customitoa(long n, char* str, long radix)
{
	//char digit[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	_longstring digitstr = { 0 };
	digitstr.t0 = 0x3736353433323130;
	digitstr.t1 = 0x4645444342413938;
	digitstr.t2 = 0x4e4d4c4b4a494847;
	digitstr.t3 = 0x565554535251504f;
	digitstr.t4 = 0x000000005a595857;

	char *digit = (char*)&digitstr;

	char* p = str;
	char* head = str;

	if (!p || radix < 2 || radix>36)
		return p;
	if (radix != 10 && n < 0)
		return p;
	if (n == 0) {
		*p++ = '0';
		*p = 0;
		return p;
	}
	if (radix == 10 && n < 0)
	{
		*p++ = '-';
		head += 1;
		n = -n;
	}
	while (n) {
		*p++ = digit[n % radix];
		n /= radix;
	}
	*p = 0;

	for (--p; head < p; ++head, --p)
	{
		char temp = *head;
		*head = *p;
		*p = temp;
	}
	return str;

}

inline void* CustomMemcpy(void* dest, void* src, unsigned int len)
{
	unsigned int i;
	char* char_src = (char*)src;
	char* char_dest = (char*)dest;
	for (i = 0; i < len; i++) {
		char_dest[i] = char_src[i];
	}
	return dest;
}

inline void CustomMemset(void* dest, char c, unsigned int len)
{
	unsigned int i;
	unsigned int fill;
	unsigned int chunks = len / sizeof(fill);
	char* char_dest = (char*)dest;
	unsigned int* uint_dest = (unsigned int*)dest;

	//
	//  Note we go from the back to the front.  This is to 
	//  prevent newer compilers from noticing what we're doing
	//  and trying to invoke the built-in memset instead of us.
	//

	fill = (c << 24) + (c << 16) + (c << 8) + c;

	for (i = len; i > chunks * sizeof(fill); i--) {
		char_dest[i - 1] = c;
	}

	for (i = chunks; i > 0; i--) {
		uint_dest[i - 1] = fill;
	}

	return dest;
}

inline CustomStrstr( char* str, char* search)
{
    char* ptr = str;
    int i;
    while (*ptr != 0x00) {
        for (i=0;ptr[i]==search[i]&&search[i]!=0x00&&ptr[i]!=0x00;i++);
        if (search[i]==0x00) return (char*)ptr;
        ptr++;
    }
    return NULL;
}

inline HANDLE GetProcessHeapCustomWrapper(_proc* procList) {
	typedef HANDLE(*_GetProcessHeap)();

	_procName procName = { 0 };
	procName.t0 = 0x65636f7250746547;
	procName.t1 = 0x0000706165487373;

	FARPROC procAddr = getProcAddr(procName, procList);

	_GetProcessHeap GetProcessHeap = (_GetProcessHeap)procAddr;

	return GetProcessHeap();
}

inline LPVOID HeapAllocCustomWrapper(HANDLE heap, DWORD flag, size_t size, _proc* procList) {
	typedef LPVOID(*_HeapAlloc)(HANDLE, DWORD, size_t);

	_procName procName = { 0 };
	procName.t0 = 0x6f6c6c4170616548;
	procName.t1 = 0x0000000000000063;

	FARPROC procAddr = getProcAddr(procName, procList);

	_HeapAlloc HeapAlloc = (_HeapAlloc)procAddr;
	return HeapAlloc(heap, flag, size);
}

inline void addProc(_procName procName, FARPROC procAddr, _proc* procList) {
	_proc* proc = procList;

	while (proc->next != NULL) {
		proc = proc->next;
	}

	proc->next = (_proc*)HeapAllocCustomWrapper(GetProcessHeapCustomWrapper(procList), HEAP_ZERO_MEMORY, sizeof(_proc), procList);
	((_proc*)(proc->next))->procAddr = procAddr;
	((_proc*)(proc->next))->procName = procName;
	((_proc*)(proc->next))->next = NULL;
}


inline FARPROC GetProcAddressWrapper(HMODULE handle, LPCSTR name, _proc* procList) {
	typedef FARPROC(*_GetProcAddress)(HMODULE, LPCSTR);

	_procName procName = { 0 };
	procName.t0 = 0x41636F7250746547;

	FARPROC procAddr = getProcAddr(procName, procList);

	_GetProcAddress GetProcAddress = (_GetProcAddress)procAddr;
	return GetProcAddress(handle, name);
}

inline FARPROC GetOrAddProcAddr(_procName procName, HMODULE mod, _proc* procList) {
	FARPROC procAddr = getProcAddr(procName, procList);

	if (procAddr == NULL) {
		procAddr = GetProcAddressWrapper(mod, (LPCSTR)&procName, procList);
		addProc(procName, procAddr, procList);
	}

	return procAddr;
}

BOOL HeapFreeWrapper(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, _proc* procList) {
	typedef BOOL(*_HeapFree)(HANDLE, DWORD, LPVOID);

	_procName procName = { 0 };
	procName.t0 = 0x6565724670616548;

	FARPROC procAddr = GetOrAddProcAddr(procName, (HMODULE)GetKernel32DosHeader(), procList);

	_HeapFree HeapFree = (_HeapFree)procAddr;
	return HeapFree(hHeap, dwFlags, lpMem);
}

inline void crateProcList(_proc** procList) {
	typedef FARPROC(*_GetProcAddress)(HMODULE, LPCSTR);
	typedef HANDLE(*_GetProcessHeap)();
	typedef LPVOID(*_HeapAlloc)(HANDLE, DWORD, size_t);

	_GetProcAddress GetProcAddress = (_GetProcAddress)GetProcAddressFunctionAddress();

	_procName procName1 = { 0 };
	procName1.t0 = 0x65636f7250746547;
	procName1.t1 = 0x0000706165487373;

	_GetProcessHeap GetProcessHeapp = (_GetProcessHeap)GetProcAddress((HMODULE)GetKernel32DosHeader(), (LPCSTR)&procName1);

	_procName procName2 = { 0 };
	procName2.t0 = 0x6f6c6c4170616548;
	procName2.t1 = 0x0000000000000063;

	_HeapAlloc HeapAllocc = (_HeapAlloc)GetProcAddress((HMODULE)GetKernel32DosHeader(), (LPCSTR)&procName2);

	*procList = (_proc*)HeapAllocc(GetProcessHeapp(), 0, sizeof(_proc));
	(*procList)->procName = procName2;
	(*procList)->procAddr = (FARPROC)HeapAllocc;
    (*procList)->next =(_proc*)HeapAllocc(GetProcessHeapp(), 0, sizeof(_proc));
	
	((_proc*)((*procList)->next))->procName = procName1;
	((_proc*)((*procList)->next))->procAddr = (FARPROC)GetProcessHeapp;

	_procName procName3 = { 0 };
	procName3.t0 = 0x41636F7250746547;

	((_proc*)((*procList)->next))->next = (_proc*)HeapAllocc(GetProcessHeapp(), 0, sizeof(_proc));
	((_proc*)((_proc*)((*procList)->next))->next)->procAddr = (FARPROC)GetProcAddress;
	((_proc*)((_proc*)((*procList)->next))->next)->procName = procName3;
	((_proc*)((_proc*)((*procList)->next))->next)->next = NULL;
}

inline HMODULE LoadLibraryAWrapper(LPCSTR lpLibFileName, _proc* procList) {
	typedef HMODULE(*_LoadLibraryA)(LPCSTR);

	_procName procName = { 0 };
	procName.t0 = 0x7262694C64616F4C;
	procName.t1 = 0x0000000041797261;

	FARPROC procAddr = GetOrAddProcAddr(procName, (HANDLE)GetKernel32DosHeader(), procList);

	_LoadLibraryA LoadLibraryA = (_LoadLibraryA)procAddr;

	return LoadLibraryA(lpLibFileName);
}

inline int WSAStartupWrapper(WORD wVersionRequired, LPWSADATA lpWSAData, HMODULE mod, _proc* procList) {
	typedef int (*_WSAStartup)(WORD, LPWSADATA);

	_procName procName = { 0 };
	procName.t0 = 0x7472617453415357;
	procName.t1 = 0x0000000000007075;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WSAStartup WSAStartup = (_WSAStartup)procAddr;

	return WSAStartup(wVersionRequired, lpWSAData);
}

inline SOCKET socketWrapper(int af, int type, int protocol, HMODULE mod, _proc* procList) {
	typedef SOCKET(*_socket)(int, int, int);

	_procName procName = { 0 };
	procName.t0 = 0x000074656b636f73;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_socket socket = (_socket)procAddr;

	return socket(af, type, protocol);
}

inline int bindWrapper(SOCKET s, struct sockaddr_in* addr, int namelen, HMODULE mod, _proc* procList) {
	typedef int (*_bind)(SOCKET, struct sockaddr_in*, int);

	_procName procName = { 0 };
	procName.t0 = 0x646e6962;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_bind bind = (_bind)procAddr;

	return bind(s, addr, namelen);
}

inline u_short htonsWrapper(u_short hostshort, HMODULE mod, _proc* procList) {
	typedef u_short(*_htons)(u_short);

	_procName procName = { 0 };
	procName.t0 = 0x736e6f7468;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_htons htons = (_htons)procAddr;

	return htons(hostshort);
}

inline int listenWrapper(SOCKET s, int backlog, HMODULE mod, _proc* procList) {
	typedef int (*_listen)(SOCKET, int);

	_procName procName = { 0 };
	procName.t0 = 0x6e657473696c;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_listen listen = (_listen)procAddr;

	return listen(s, backlog);
}

inline SOCKET acceptWrapper(SOCKET s, struct sockaddr_in* addr, int* addrlen, HMODULE mod, _proc* procList) {
	typedef SOCKET(*_accept)(SOCKET, struct sockaddr_in*, int*);

	_procName procName = { 0 };
	procName.t0 = 0x0000747065636361;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_accept accept = (_accept)procAddr;

	return accept(s, addr, addrlen);
}

inline int WSACleanupWrapper(HMODULE mod, _proc* procList) {
	typedef int(*_WSACleanup)();

	_procName procName = { 0 };
	procName.t0 = 0x6e61656c43415357;
	procName.t1 = 0x0000000000007075;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WSACleanup WSACleanup = (_WSACleanup)procAddr;

	return WSACleanup();
}

inline int closesocketWrapper(SOCKET s, HMODULE mod, _proc* procList) {
	typedef int (*_closesocket)(SOCKET);

	_procName procName = { 0 };
	procName.t0 = 0x636f7365736f6c63;
	procName.t1 = 0x000000000074656b;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_closesocket closesocket = (_closesocket)procAddr;
	return closesocket(s);
}

inline HANDLE CreateThreadWrapper(
	LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE  lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId, _proc* procList) {
	typedef HANDLE(*_CreateThread)(LPSECURITY_ATTRIBUTES, size_t, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);

	_procName procName = { 0 };
	procName.t0 = 0x6854657461657243;
	procName.t1 = 0x0000000064616572;

	FARPROC procAddr = GetOrAddProcAddr(procName, (HMODULE)GetKernel32DosHeader(), procList);

	_CreateThread CreateThread = (_CreateThread)procAddr;

	return CreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
}

inline HANDLE CreateMutexWWrapper(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCWSTR lpName, _proc* procList) {
	typedef HANDLE(*_CreateMutexW)(LPSECURITY_ATTRIBUTES, BOOL, LPCWSTR);

	_procName procName = { 0 };
	procName.t0 = 0x754d657461657243;
	procName.t1 = 0x0000000057786574;

	FARPROC procAddr = GetOrAddProcAddr(procName, (HMODULE)GetKernel32DosHeader(), procList);

	_CreateMutexW CreateMutexW = (_CreateMutexW)procAddr;

	return CreateMutexW(lpMutexAttributes, bInitialOwner, lpName);
}

inline BOOL ReleaseMutexWrapper(HANDLE hMutex, _proc* procList) {
	typedef BOOL(*_ReleaseMutex)(HANDLE);

	_procName procName = { 0 };
	procName.t0 = 0x4d657361656c6552;
	procName.t1 = 0x0000000078657475;

	FARPROC procAddr = GetOrAddProcAddr(procName, (HMODULE)GetKernel32DosHeader(), procList);

	_ReleaseMutex ReleaseMutex = (_ReleaseMutex)procAddr;

	return ReleaseMutex(hMutex);
}

inline DWORD WaitForSingleObjectWrapper(HANDLE hHandle, DWORD  dwMilliseconds, _proc* procList) {
	typedef DWORD(*_WaitForSingleObject)(HANDLE, DWORD);

	_procName procName = { 0 };
	procName.t0 = 0x53726f4674696157;
	procName.t1 = 0x6a624f656c676e69;
	procName.t2 = 0x0000000000746365;

	FARPROC procAddr = GetOrAddProcAddr(procName, (HMODULE)GetKernel32DosHeader(), procList);

	_WaitForSingleObject WaitForSingleObject = (_WaitForSingleObject)procAddr;

	return WaitForSingleObject(hHandle, dwMilliseconds);
}

inline int sendWrapper(SOCKET s, char* buf, int len, int flags, HMODULE mod, _proc* procList) {
	typedef int (*_send)(SOCKET, char*, int, int);
	
	_procName procName = { 0 };
	procName.t0 = 0x00000000646E6573;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_send send = (_send)procAddr;

	return send(s, buf, len, flags);
}

inline int recvWrapper(SOCKET s, char* buf, int len, int flags, HMODULE mod, _proc* procList) {
	typedef int (*_recv)(SOCKET, char*, int, int);
	
	_procName procName = { 0 };
	procName.t0 = 0x00000000076636572;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_recv recv = (_recv)procAddr;

	return recv(s, buf, len, flags);
}

inline void RtlFillMemoryWrapper(void* destination, size_t length, int fill, _proc* procList) {
	typedef void(*_RtlFillMemory)(void* destination, size_t length, int fill);

	_procName procName = { 0 };

	procName.t0 = 0x4d6c6c69466c7452;
	procName.t1 = 0x00000079726f6d65;

	FARPROC procAddr = GetOrAddProcAddr(procName, (HMODULE)GetKernel32DosHeader(), procList);

	_RtlFillMemory RtlFillMemoryy = (_RtlFillMemory)procAddr;

	RtlFillMemoryy(destination, length, fill);
}

inline DWORD GetLastErrorWrapper(_proc* procList) {
	typedef DWORD(*_GetLastError)();

	_procName procName = { 0 };

	procName.t0 = 0x457473614c746547;
	procName.t1 = 0x00000000726f7272;

	FARPROC procAddr = GetOrAddProcAddr(procName, (HMODULE)GetKernel32DosHeader(), procList);

	_GetLastError GetLastError = (_GetLastError)procAddr;

	return GetLastError();
}

#if !defined(USE_WINHTTP)

inline HINTERNET InternetOpenWWrapper(LPCSTR lpszAgent, DWORD  dwAccessType, LPCSTR lpszProxy, LPCSTR lpszProxyBypass, DWORD  dwFlags, HMODULE mod, _proc* procList) {
	typedef HINTERNET (__stdcall *_InternetOpenA)(LPCSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);

	_procName procName = { 0 };

	procName.t0 = 0x74656e7265746e49; 
	procName.t1 = 0x000000416e65704f; 

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_InternetOpenA InternetOpenA = (_InternetOpenA)procAddr;

	return InternetOpenA(lpszAgent, dwAccessType, lpszProxy, lpszProxyBypass, dwFlags);
}

inline HINTERNET InternetConnectWWrapper(
	HINTERNET hInternet, LPCSTR lpszServerName,
	INTERNET_PORT nServerPort, LPCSTR lpszUserName, LPCSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext, HMODULE mod, _proc* procList) {
	typedef void* (*_InternetConnectA)(HINTERNET, LPCSTR, INTERNET_PORT, LPCSTR, LPCSTR, DWORD, DWORD, DWORD_PTR);

	_procName procName = { 0 };

	procName.t0 = 0x74656e7265746e49;
	procName.t1 = 0x417463656e6e6f43;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);
	
	_InternetConnectA InternetConnectA = (_InternetConnectA)procAddr;

	return InternetConnectA(hInternet, lpszServerName, nServerPort, lpszUserName, lpszPassword, dwService, dwFlags, dwContext);
}

inline HINTERNET InternetOpenUrlAWrapper( HINTERNET hInternet, LPCSTR lpszUrl, LPCSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwFlags, DWORD_PTR dwContext,
	HMODULE mod, _proc* procList) {
	typedef HINTERNET(*_InternetOpenUrlA)(HINTERNET hInternet, LPCSTR lpszUrl, LPCSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwFlags, DWORD_PTR dwContext);

	_procName procName = { 0 };

	procName.t0 = 0x74656e7265746e49;
	procName.t1 = 0x416c72556e65704f;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_InternetOpenUrlA InternetOpenUrlA = (_InternetOpenUrlA)procAddr;

	return InternetOpenUrlA(hInternet, lpszUrl, lpszHeaders, dwHeadersLength, dwFlags, dwContext);
}

inline HINTERNET HttpOpenRequestAWrapper(
	HINTERNET hConnect, 
	LPCSTR lpszVerb, 
	LPCSTR lpszObjectName, 
	LPCSTR lpszVersion, 
	LPCSTR lpszReferrer, 
	LPCSTR* lplpszAcceptTypes, 
	DWORD dwFlags, 
	DWORD_PTR dwContext,
	HMODULE mod, _proc *procList) {
	typedef HINTERNET(*_HttpOpenRequestA)(HINTERNET, LPCSTR, LPCSTR, LPCSTR, LPCSTR, LPCSTR*, DWORD, DWORD_PTR);

	_procName procName = { 0 };

	procName.t0 = 0x6e65704f70747448;
	procName.t1 = 0x4174736575716552;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_HttpOpenRequestA HttpOpenRequestA = (_HttpOpenRequestA)procAddr;

	return HttpOpenRequestA(hConnect, lpszVerb, lpszObjectName, lpszVersion, lpszReferrer, lplpszAcceptTypes, dwFlags, dwContext);
}

BOOL HttpAddRequestHeadersAWrapper(HINTERNET hRequest,LPCSTR lpszHeaders,DWORD dwHeadersLength, DWORD dwModifiers, HMODULE mod, _proc *procList) {
	typedef BOOL(*_HttpAddRequestHeadersA)(HINTERNET, LPCSTR, DWORD, DWORD);

	_procName procName = { 0 };

	procName.t0 = 0x5264644170747448;
	procName.t1 = 0x6548747365757165;
	procName.t2 = 0x0000417372656461;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_HttpAddRequestHeadersA HttpAddRequestHeadersA = (_HttpAddRequestHeadersA)procAddr;

	return HttpAddRequestHeadersA(hRequest, lpszHeaders, dwHeadersLength, dwModifiers);
}

inline BOOL HttpSendRequestAWrapper(HINTERNET hRequest, LPCSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength, HMODULE mod, _proc *procList) {
	typedef BOOL(*_HttpSendRequestA)(HINTERNET, LPCSTR, DWORD, LPVOID, DWORD);
	
	_procName procName = { 0 };

	procName.t0 = 0x646e655370747448;
	procName.t1 = 0x4174736575716552;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_HttpSendRequestA HttpSendRequestA = (_HttpSendRequestA)procAddr;

	return HttpSendRequestA(hRequest, lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength);

}

inline BOOL HttpQueryInfoAWrapper(HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex, HMODULE mod, _proc* procList) {
	typedef BOOL(*_HttpQueryInfoA)(HINTERNET, DWORD, LPVOID, LPWORD, LPDWORD);

	_procName procName = { 0 };

	procName.t0 = 0x7265755170747448;
	procName.t1 = 0x0000416f666e4979;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_HttpQueryInfoA HttpQueryInfoA = (_HttpQueryInfoA)procAddr;

	return HttpQueryInfoA(hRequest, dwInfoLevel, lpBuffer, lpdwBufferLength, lpdwIndex);
}

inline  BOOL InternetReadFileWrapper(HINTERNET hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead, HMODULE mod, _proc* procList) {
	typedef BOOL(*_InternetReadFile)(HINTERNET, LPVOID, DWORD, LPDWORD);

	_procName procName = { 0 };

	procName.t0 = 0x74656e7265746e49;
	procName.t1 = 0x656c694664616552;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_InternetReadFile InternetReadFile = (_InternetReadFile)procAddr;

	return InternetReadFile(hFile, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
}

inline BOOL InternetCloseHandleWrapper(HINTERNET hInternet, HMODULE mod, _proc* procList) {
	typedef BOOL(*_InternetCloseHandle)(HINTERNET);

	_procName procName = { 0 };

	procName.t0 = 0x74656e7265746e49;
	procName.t1 = 0x6e614865736f6c43;
	procName.t2 = 0x0000000000656c64;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_InternetCloseHandle InternetCloseHandle = (_InternetCloseHandle)procAddr;

	return InternetCloseHandle(hInternet);
}

#endif /* !USE_WINHTTP */

#if defined(USE_WINHTTP)

inline HINTERNET WinHttpOpenWrapper(LPCSTR lpszAgent, DWORD dwAccessType, LPCWSTR lpszProxy, LPCWSTR lpszProxyBypass, DWORD dwFlags, HMODULE mod, _proc* procList) {
	typedef HINTERNET(__stdcall *_WinHttpOpen)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);

	_procName procName = { 0 };

	procName.t0 = 0x4f707474486e6957;
	procName.t1 = 0x00000000006e6570;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WinHttpOpen WinHttpOpen = (_WinHttpOpen)procAddr;

	return WinHttpOpen((LPCWSTR)lpszAgent, dwAccessType, lpszProxy, lpszProxyBypass, dwFlags);
}

inline HINTERNET WinHttpConnectWrapper(HINTERNET hSession, LPCSTR lpszServerName, INTERNET_PORT nServerPort, DWORD dwReserved, HMODULE mod, _proc* procList) {
	typedef HINTERNET(*_WinHttpConnect)(HINTERNET, LPCWSTR, INTERNET_PORT, DWORD);

	_procName procName = { 0 };

	procName.t0 = 0x43707474486e6957;
	procName.t1 = 0x00007463656e6e6f;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WinHttpConnect WinHttpConnect = (_WinHttpConnect)procAddr;

	return WinHttpConnect(hSession, (LPCWSTR)lpszServerName, nServerPort, dwReserved);
}

inline HINTERNET WinHttpOpenRequestWrapper(
	HINTERNET hConnect,
	LPCSTR lpszVerb,
	LPCSTR lpszObjectName,
	LPCSTR lpszVersion,
	LPCSTR lpszReferrer,
	LPCWSTR* lplpszAcceptTypes,
	DWORD dwFlags,
	HMODULE mod, _proc* procList) {
	typedef HINTERNET(*_WinHttpOpenRequest)(HINTERNET, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);

	_procName procName = { 0 };

	procName.t0 = 0x4f707474486e6957;
	procName.t1 = 0x65757165526e6570;
	procName.t2 = 0x0000000000007473;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WinHttpOpenRequest WinHttpOpenRequest = (_WinHttpOpenRequest)procAddr;

	return WinHttpOpenRequest(hConnect, (LPCWSTR)lpszVerb, (LPCWSTR)lpszObjectName, (LPCWSTR)lpszVersion, (LPCWSTR)lpszReferrer, lplpszAcceptTypes, dwFlags);
}

BOOL WinHttpAddRequestHeadersWrapper(HINTERNET hRequest, LPCSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwModifiers, HMODULE mod, _proc* procList) {
	typedef BOOL(*_WinHttpAddRequestHeaders)(HINTERNET, LPCWSTR, DWORD, DWORD);

	_procName procName = { 0 };

	procName.t0 = 0x41707474486e6957;
	procName.t1 = 0x7365757165526464;
	procName.t2 = 0x7372656461654874;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WinHttpAddRequestHeaders WinHttpAddRequestHeaders = (_WinHttpAddRequestHeaders)procAddr;

	return WinHttpAddRequestHeaders(hRequest, (LPCWSTR)lpszHeaders, dwHeadersLength, dwModifiers);
}

inline BOOL WinHttpSendRequestWrapper(HINTERNET hRequest, LPCSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength, DWORD dwTotalLength, DWORD_PTR dwContext, HMODULE mod, _proc* procList) {
	typedef BOOL(*_WinHttpSendRequest)(HINTERNET, LPCWSTR, DWORD, LPVOID, DWORD, DWORD, DWORD_PTR);

	_procName procName = { 0 };

	procName.t0 = 0x53707474486e6957;
	procName.t1 = 0x6575716552646e65;
	procName.t2 = 0x0000000000007473;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WinHttpSendRequest WinHttpSendRequest = (_WinHttpSendRequest)procAddr;

	return WinHttpSendRequest(hRequest, (LPCWSTR)lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength, dwTotalLength, dwContext);
}

inline BOOL WinHttpReceiveResponseWrapper(HINTERNET hRequest, HMODULE mod, _proc* procList) {
	typedef BOOL(*_WinHttpReceiveResponse)(HINTERNET, LPVOID);

	_procName procName = { 0 };

	procName.t0 = 0x52707474486E6957;
	procName.t1 = 0x6552657669656365;
	procName.t2 = 0x000065736E6F7073;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WinHttpReceiveResponse WinHttpReceiveResponse = (_WinHttpReceiveResponse)procAddr;

	return WinHttpReceiveResponse(hRequest, NULL);
}

inline BOOL WinHttpQueryHeadersWrapper(HINTERNET hRequest, DWORD dwInfoLevel, LPCWSTR pwszName, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex, HMODULE mod, _proc* procList) {
	typedef BOOL(*_WinHttpQueryHeaders)(HINTERNET, DWORD, LPCWSTR, LPVOID, LPDWORD, LPDWORD);

	_procName procName = { 0 };

	procName.t0 = 0x51707474486e6957;
	procName.t1 = 0x6461654879726575;
	procName.t2 = 0x0000000000737265;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WinHttpQueryHeaders WinHttpQueryHeaders = (_WinHttpQueryHeaders)procAddr;

	return WinHttpQueryHeaders(hRequest, dwInfoLevel, pwszName, lpBuffer, lpdwBufferLength, lpdwIndex);
}

inline BOOL WinHttpReadDataWrapper(HINTERNET hRequest, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead, HMODULE mod, _proc* procList) {
	typedef BOOL(*_WinHttpReadData)(HINTERNET, LPVOID, DWORD, LPDWORD);

	_procName procName = { 0 };

	procName.t0 = 0x52707474486e6957;
	procName.t1 = 0x0061746144646165;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WinHttpReadData WinHttpReadData = (_WinHttpReadData)procAddr;

	return WinHttpReadData(hRequest, lpBuffer, dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
}

inline BOOL WinHttpCloseHandleWrapper(HINTERNET hInternet, HMODULE mod, _proc* procList) {
	typedef BOOL(*_WinHttpCloseHandle)(HINTERNET);

	_procName procName = { 0 };

	procName.t0 = 0x43707474486e6957;
	procName.t1 = 0x646e614865736f6c;
	procName.t2 = 0x000000000000656c;

	FARPROC procAddr = GetOrAddProcAddr(procName, mod, procList);

	_WinHttpCloseHandle WinHttpCloseHandle = (_WinHttpCloseHandle)procAddr;

	return WinHttpCloseHandle(hInternet);
}

#endif /* USE_WINHTTP */

