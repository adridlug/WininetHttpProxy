#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "procs.h"
#include "queue.h"
#include <Windows.h>
#include<winternl.h>
#define SPACE_CHAR  0x20
#define BUFFER_SIZE 1024
#define HOST_SIZE 255
#define REQUEST_SIZE 4096
#define PORT_SIZE 5
#define METHOD_SIZE 10
#define MAX_THREADS 50
#define LISTEN_PORT 3476

struct thread_param {
	Queue *queue;
	int thread_nr;
	HANDLE queueMutex;
	HMODULE ws2_32;
	HMODULE http_mod;
	_proc* procList;
};

/* Singly-linked list node holding one HTTP header line (full "Name: value\r\n" string). */
struct http_header_line {
	char* value;
	struct http_header_line *next;
};

/* Allocate and zero-initialise a new http_header_line node on the process heap. */
void alloc_header_struct(struct http_header_line **headers, HANDLE heap, _proc* procList) {
	
	*headers = (struct http_header_line*)HeapAllocCustomWrapper(heap, HEAP_ZERO_MEMORY, sizeof(struct http_header_line), procList);
	(**headers).value = (char*)HeapAllocCustomWrapper(heap, HEAP_ZERO_MEMORY, 1024, procList);

	(**headers).next = 0;
}

/* Walk the header list and free every node and its buffers. */
void free_headers(struct http_header_line *headers, HANDLE heap, _proc* procList) {
	while (headers != NULL) {
		HeapFreeWrapper(heap, HEAP_ZERO_MEMORY, headers->value, procList);
		struct http_header_line *oldHeader = headers;
		headers = headers->next;
		HeapFreeWrapper(heap, HEAP_ZERO_MEMORY, oldHeader, procList);
	}
}


/*
 * ParseRequest - Extract method, path, Host, and headers from a raw HTTP/1.1 request.
 *
 * Outputs are written into caller-supplied buffers (method, host, port, path).
 * A singly-linked list of http_header_line nodes is allocated on the process heap
 * and returned via *headers; the caller is responsible for freeing it with
 * free_headers().
 *
 * Defaults applied when not found in the request:
 *   port -> "80"
 *   path -> "/"
 */
void ParseRequest(char* request, char* method, char* host, char* port, char* path, struct http_header_line **headers, HANDLE heap, _proc* procList) {
	int method_length = 0;
	int host_length = 0;
	int port_length = 0;
	int path_length = 0;
	int headers_length = 0;
	int header_name_length = 0;
	int header_value_length = 0;
	int default_http_port = 0x00003038;
	int base_path = 0x0000002f;
	struct http_header_line *tmp_header;

	alloc_header_struct(&tmp_header, heap, procList);
	*headers = tmp_header;


    /* Parse method: read until first space. */
    while (*request != SPACE_CHAR) {
        ++method_length;
        ++request;
    }

    CustomMemcpy(method, request - method_length, method_length);

    ++request; /* skip space */

    /* Parse path: read until next space or end of string. */
    while (*request != SPACE_CHAR && *request != '\0') {
        ++path_length;
        ++request;
    }
    
    CustomMemcpy(path, request - path_length, path_length);
    /* Parse method: read until first space (bounded, null-terminated). */
    while (*request != SPACE_CHAR && *request != '\0' &&
           method_length < (METHOD_SIZE - 1)) {
        ++method_length;
        ++request;
    }

    CustomMemcpy(method, request - method_length, method_length);

    if (*request == SPACE_CHAR) {
        ++request; /* skip space */
    }

    /* Parse path: read until next space or end of string (bounded, null-terminated). */
    while (*request != SPACE_CHAR && *request != '\0' &&
           path_length < (BUFFER_SIZE - 1)) {
        ++path_length;
        ++request;
    }

    CustomMemcpy(path, request - path_length, path_length);

	/* Scan forward for the "Host:" header line. */
	while (*request != '\0') {
		if (*request == 0x0a && *(request + 1) == 0x48 && *(request + 2) == 0x6f && *(request + 3) == 0x73 && *(request + 4) == 0x74 && *(request + 5) == 0x3a && *(request + 6) == SPACE_CHAR) {
			request += 7;

			while (*request != 0x0d) {
				++request;
				++host_length;
			}

			CustomMemcpy(host, request - host_length, host_length);
			request += 2; /* skip \r\n */
			break;
		}
		++request;
	}
		
	/* Collect remaining header lines until the blank line (\r\n\r\n). */
	while (1) {
		header_value_length= 0;

		while (*request != 0x0d && *(request + 1) != 0x0a) {
			++header_value_length;
			++request;
		}

		CustomMemcpy(tmp_header->value, request - header_value_length, header_value_length +2);
		request += 2; /* skip \r\n */


		if (*request == 0x0d && *(request + 1) == 0x0a) {
			break;
		}
		

		alloc_header_struct(&(tmp_header->next), heap, procList);
		tmp_header = tmp_header->next;
	}

	/* Apply defaults for port and path when not provided. */
	if (CustomStrlen(port) == 0)
		CustomMemcpy(port, &default_http_port, 2);

	if (CustomStrlen(path) == 0)
		CustomMemcpy(path, &base_path, 1);
}
//
inline int receive_until_end_of_http_request(int socket, char* buff, int length, HANDLE ws2_32_handle, _proc* procList) {
    int read_length = 0;
    do {
        char tmp_buf[200];
		CustomMemset(&tmp_buf, 0, 200);
        int tmp_len = recvWrapper(socket, tmp_buf, 200-1, 0, ws2_32_handle, procList);

        if (tmp_len <= 0) {
            break;
        }

		CustomMemcpy(buff + read_length, tmp_buf, tmp_len);
        read_length += tmp_len;

		if (*(buff + read_length - 4) == 0x0d && *(buff + read_length - 3) == 0x0a && *(buff + read_length - 2) == 0x0d && *(buff + read_length - 1) == 0x0a) {
			break;
		}

    } while (read_length < length);

    return read_length;
}

/*
 * http_send_request - Connect to the upstream host and send the HTTP request.
 *
 * Opens a session, connects to the host on HTTPS port 443, opens a request,
 * attaches all headers from the linked list, sends the request, and (for the
 * WinHTTP backend) waits for the response.
 *
 * Returns 1 on success, 0 on any failure.
 */
int http_send_request(
	HINTERNET hInternet, HINTERNET *hConnect, HINTERNET *hRequest, char* method, char* host, char* port, char* path, struct http_header_line *headers, HMODULE mod, _proc* procList) {

	*hConnect = HTTP_Connect(hInternet, host, INTERNET_DEFAULT_HTTPS_PORT, mod, procList);
	
	if (*hConnect == NULL) {
		return 0;
	}

	int get = 0x00544547;
	*hRequest = HTTP_OpenRequest(
		*hConnect, 
		&get, 
		path, 
		NULL, 
		NULL, 
		NULL, 
		HTTP_FLAG_SECURE_REQUEST, 
		mod, 
		procList);
	
	if (!*hRequest) {
		return 0;
	}

	while (headers != NULL) {
		if (!HTTP_AddRequestHeaders(*hRequest, headers->value, -1, HTTP_ADDREQ_FLAGS, mod, procList)) {
			return 0;
		}

		headers = headers->next;
	}

	if (!HTTP_SendRequest(*hRequest, NULL, 0, 0, 0, mod, procList)) {
		return 0;
	}

	if (!HTTP_ReceiveResponse(*hRequest, mod, procList)) {
		return 0;
	}
	
	return 1;
}

int http_get_response_headers(HANDLE hRequest, char** http_response_header_buffer, int* http_response_header_buffer_length, HMODULE mod, _proc* procList, HANDLE heap) {
	while (HTTP_QueryHeaders(
		hRequest, HTTP_QUERY_RAW_HEADERS_CRLF, *http_response_header_buffer, http_response_header_buffer_length, NULL, mod, procList) == FALSE) {

		if (GetLastErrorWrapper(procList) == ERROR_INSUFFICIENT_BUFFER) {

			*http_response_header_buffer = (char*)HeapAllocCustomWrapper(heap, 0, *http_response_header_buffer_length, procList);

			if (http_response_header_buffer == NULL) {
				return 0;
			}
		}
		else {
			return 0;
		}
	}

	return 1;
}

void free_handles(HINTERNET hInternet, HINTERNET hConnect, HINTERNET hRequest, HMODULE mod, _proc* procList) {

	if (hRequest != 0) {
		HTTP_CloseHandle(hRequest, mod, procList);
	}

	if (hConnect != 0) {
		HTTP_CloseHandle(hConnect, mod, procList);
	}

	if (hInternet != 0) {
		HTTP_CloseHandle(hInternet, mod, procList);
	}
}

inline DWORD WINAPI HandleRequestThread(LPVOID params) {
	char host[HOST_SIZE];
	char port[PORT_SIZE];
	char method[METHOD_SIZE];

	struct http_header_line *headers;

	struct thread_param* th_params = (struct thread_param*)params;
	Queue* queue = th_params->queue;
	int thread_nr = th_params->thread_nr;
	HMODULE queueMutex = th_params->queueMutex;
	_proc* procList = th_params->procList;
	HMODULE ws2_32 = th_params->ws2_32;
	HMODULE http_mod = th_params->http_mod;

	HANDLE heap = GetProcessHeapCustomWrapper(procList);

	char* request = (char*)HeapAllocCustomWrapper(heap, HEAP_ZERO_MEMORY, REQUEST_SIZE, procList);
	char* path = (char*)HeapAllocCustomWrapper(heap, HEAP_ZERO_MEMORY, BUFFER_SIZE, procList);

	_procName procName = { 0 };

	while (1) {

		int client_sock = dequeue(queue, queueMutex, procList);

		if (client_sock == -1) {
			continue;
		}

		RtlFillMemoryWrapper(request, REQUEST_SIZE, 0, procList);

		int read_length = receive_until_end_of_http_request(client_sock, request, REQUEST_SIZE, ws2_32, procList);

		if (read_length <= 0) {
			continue;
		}

		RtlFillMemoryWrapper(&method, METHOD_SIZE, 0, procList);
		RtlFillMemoryWrapper(&host, HOST_SIZE, 0, procList);
		RtlFillMemoryWrapper(&port, PORT_SIZE, 0, procList);
		RtlFillMemoryWrapper(path, BUFFER_SIZE, 0, procList);

		ParseRequest(request, &method, &host, &port, path, &headers, heap, procList);

		_longstring httpAgent = { 0 };
		httpAgent.t0 = 0x2f616c6c697a6f4d;
		httpAgent.t1 = 0x0000000000302e35;


		HINTERNET hInternet = HTTP_Open(&httpAgent, HTTP_OPEN_TYPE_DIRECT, NULL, NULL, 0, http_mod, procList);
		
		if (!hInternet) {
			goto DONE;
		}

		HINTERNET hConnect = 0;
		HINTERNET hRequest = 0;

		if (http_send_request(hInternet, &hConnect, &hRequest, &method, &host, &port, path, headers, http_mod, procList) == 0) {
			goto DONELOOP;
		}

		int http_response_header_buffer_length = 0;
		char* http_response_header_buffer = NULL;
		if (http_get_response_headers(hRequest, &http_response_header_buffer, &http_response_header_buffer_length, http_mod, procList, heap) == 0) {
			goto DONELOOP;
		}
		
		int send_length = 0;
		while ((send_length += sendWrapper(
			client_sock, 
			http_response_header_buffer + send_length, 
			http_response_header_buffer_length - send_length, 
			0,
			ws2_32,
			procList)) <= http_response_header_buffer_length - send_length) {

		}
		
		_longstring str = { 0 };
		str.t0 = 0x726566736e617254;
		str.t1 = 0x6e69646f636e452d;
		str.t2 = 0x6b6e756863203a67;
		str.t3 = 0x000000000a0d6465;

		int encoding_chunked = 0;

		if (CustomStrstr(http_response_header_buffer, &str) != NULL) {
			encoding_chunked = 1;
		}

		char response_buffer[BUFFER_SIZE];
		read_length = 0;

		int crlf = 0x00000a0d;
		ULONG64 crlfcrlf = 0x00000a0d0a0d30;
		int hexformater = 0x007825;

		while (1) {
			RtlFillMemoryWrapper(&response_buffer, BUFFER_SIZE, 0, procList);

			if (!HTTP_ReadData(hRequest, &response_buffer, BUFFER_SIZE, &read_length, http_mod, procList) || read_length == 0)
				break;

			if (encoding_chunked) {
				char read_length_hex_str[4] = { 0 };
				Customitoa(read_length, &read_length_hex_str, 16);
				
				sendWrapper(client_sock, &read_length_hex_str, CustomStrlen(read_length_hex_str), 0, ws2_32, procList);
				sendWrapper(client_sock, &crlf, 2, 0, ws2_32, procList);
			}

			send_length = 0;
			while ((send_length += sendWrapper(client_sock, &response_buffer + send_length, read_length - send_length, 0, ws2_32, procList)) <= read_length - send_length) {
			}

			if (encoding_chunked == 1) {
				sendWrapper(client_sock, &crlf, 2, 0, ws2_32, procList);
			}

			send_length = 0;
		}

		if (encoding_chunked) {
			sendWrapper(client_sock, &crlfcrlf, 5, 0,ws2_32, procList);
		}

	DONELOOP:
		if (http_response_header_buffer != NULL)
			HeapFreeWrapper(heap, HEAP_ZERO_MEMORY, http_response_header_buffer, procList);
		free_headers(headers, heap, procList);
		closesocketWrapper(client_sock, ws2_32, procList);
		free_handles(hInternet, hConnect, hRequest, http_mod, procList);
	}
DONE:
	HeapFreeWrapper(heap, HEAP_ZERO_MEMORY, path, procList);
	HeapFreeWrapper(heap, HEAP_ZERO_MEMORY, request, procList);
}

inline HMODULE LoadWs2_32(_proc* procList) {
	_procName procName = { 0 };

	procName.t0 = 0x642E32335F327357;
	procName.t1 = 0x0000000000006C6C;

	return LoadLibraryAWrapper((LPCSTR)&procName, procList);
}

inline HMODULE LoadHttpModule(_proc* procList) {
	_procName procName = { 0 };

#if defined(USE_WINHTTP)
	/* winhttp */
	procName.t0 = 0x00707474686E6977;
#else
	/* wininet */
	procName.t0 = 0x0074656E696E6977;
#endif

	return LoadLibraryAWrapper((LPCSTR)&procName, procList);
}

int main() {
	HMODULE ws2_32;
	HMODULE http_mod;
	LPWSADATA wsaData;
	_proc* procList = NULL;

	crateProcList(&procList);

	ws2_32 = LoadWs2_32(procList);
	http_mod = LoadHttpModule(procList);

	if (WSAStartupWrapper(MAKEWORD(2, 2), &wsaData, ws2_32, procList) != 0) {
		return;
	}

	SOCKET server_sock = socketWrapper(AF_INET, SOCK_STREAM, IPPROTO_TCP, ws2_32, procList);

	if (server_sock == -1) {
		WSACleanupWrapper(ws2_32, procList);
		return;
	}

	struct sockaddr_in serverAddr;

	// Bind the socket to the proxy port
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htonsWrapper(LISTEN_PORT, ws2_32, procList);

	if (bindWrapper(server_sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr), ws2_32, procList) == SOCKET_ERROR) {
		closesocketWrapper(server_sock, ws2_32, procList);
		WSACleanupWrapper(ws2_32, procList);
		return;
	}

	if (listenWrapper(server_sock, SOMAXCONN, ws2_32, procList) == SOCKET_ERROR) {
		closesocketWrapper(server_sock, ws2_32, procList);
		WSACleanupWrapper(ws2_32, procList);
		return;
	}
	Queue *queue = NULL;
	HANDLE queueMutex = initializeQueue(&queue, procList);

	unsigned int threadId[MAX_THREADS];
	struct thread_param th_params[MAX_THREADS];
	HANDLE threads[MAX_THREADS];

	for (int i = 0; i < MAX_THREADS; ++i)
	{
		th_params[i].queue = queue;
		th_params[i].thread_nr = i;
		th_params[i].queueMutex = queueMutex;
		th_params[i].ws2_32 = ws2_32;
		th_params[i].http_mod = http_mod;
		th_params[i].procList = procList;

		threads[i] = (HANDLE)CreateThreadWrapper(NULL, 0, HandleRequestThread, &(th_params[i]), 0, &threadId[i], procList);
	}

	SOCKET client_sock;
	while ((client_sock = acceptWrapper(server_sock, NULL, NULL, ws2_32, procList)) != INVALID_SOCKET) {
				
		enqueue(queue, client_sock, queueMutex, procList);
	}
}
