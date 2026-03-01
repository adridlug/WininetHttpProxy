#pragma once
/*
 * http_backend.h - Build-time HTTP backend selection
 *
 * Define USE_WINHTTP before including this header (or pass /DUSE_WINHTTP to
 * the compiler) to use the WinHTTP backend (winhttp.dll, WinHttp* APIs).
 * Without that define the default WinINet backend is used (wininet.dll,
 * Internet* or Http* APIs).
 *
 * Build examples:
 *   WinINet (default): cl /GS- http_porxy.c
 *   WinHTTP:           cl /GS- /DUSE_WINHTTP http_porxy.c
 */

#if defined(USE_WINHTTP)

#include <winhttp.h>

/* --- backend flags ---------------------------------------------------- */
#define HTTP_OPEN_TYPE_DIRECT    WINHTTP_ACCESS_TYPE_NO_PROXY
#define HTTP_FLAG_SECURE_REQUEST (WINHTTP_FLAG_BYPASS_PROXY_CACHE | WINHTTP_FLAG_SECURE)
#define HTTP_ADDREQ_FLAGS        (WINHTTP_ADDREQ_FLAG_REPLACE | WINHTTP_ADDREQ_FLAG_ADD)

/* --- neutral operation macros ----------------------------------------- */
#define HTTP_Open(agent, accessType, proxy, bypass, flags, mod, procList) \
    WinHttpOpenWrapper((LPCSTR)(agent), (accessType), (proxy), (bypass), (flags), (mod), (procList))

#define HTTP_Connect(hSession, host, port, mod, procList) \
    WinHttpConnectWrapper((hSession), (host), (port), 0, (mod), (procList))

#define HTTP_OpenRequest(hConnect, verb, path, version, referrer, accept, flags, mod, procList) \
    WinHttpOpenRequestWrapper((hConnect), (verb), (path), (version), (referrer), (accept), (flags), (mod), (procList))

#define HTTP_AddRequestHeaders(hRequest, headers, len, flags, mod, procList) \
    WinHttpAddRequestHeadersWrapper((hRequest), (headers), (len), (flags), (mod), (procList))

#define HTTP_SendRequest(hRequest, headers, headersLen, optional, optionalLen, mod, procList) \
    WinHttpSendRequestWrapper((hRequest), (headers), (headersLen), (optional), (optionalLen), 0, 0, (mod), (procList))

/* WinHTTP requires WinHttpReceiveResponse after WinHttpSendRequest */
#define HTTP_ReceiveResponse(hRequest, mod, procList) \
    WinHttpReceiveResponseWrapper((hRequest), (mod), (procList))

#define HTTP_QueryHeaders(hRequest, infoLevel, buffer, bufLen, index, mod, procList) \
    WinHttpQueryHeadersWrapper((hRequest), (infoLevel), NULL, (buffer), (bufLen), (index), (mod), (procList))

#define HTTP_ReadData(hRequest, buffer, toRead, read, mod, procList) \
    WinHttpReadDataWrapper((hRequest), (buffer), (toRead), (read), (mod), (procList))

#define HTTP_CloseHandle(h, mod, procList) \
    WinHttpCloseHandleWrapper((h), (mod), (procList))

#else /* WinINet (default) */

#include <wininet.h>

/* --- backend flags ---------------------------------------------------- */
#define HTTP_OPEN_TYPE_DIRECT    INTERNET_OPEN_TYPE_DIRECT
#define HTTP_FLAG_SECURE_REQUEST (INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | \
                                  INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_AUTO_REDIRECT | \
                                  INTERNET_FLAG_RAW_DATA)
#define HTTP_ADDREQ_FLAGS        (HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD)

/* --- neutral operation macros ----------------------------------------- */
#define HTTP_Open(agent, accessType, proxy, bypass, flags, mod, procList) \
    InternetOpenWWrapper((LPCSTR)(agent), (accessType), (proxy), (bypass), (flags), (mod), (procList))

#define HTTP_Connect(hSession, host, port, mod, procList) \
    InternetConnectWWrapper((hSession), (host), (port), NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0, (mod), (procList))

#define HTTP_OpenRequest(hConnect, verb, path, version, referrer, accept, flags, mod, procList) \
    HttpOpenRequestAWrapper((hConnect), (verb), (path), (version), (referrer), (accept), (flags), 0, (mod), (procList))

#define HTTP_AddRequestHeaders(hRequest, headers, len, flags, mod, procList) \
    HttpAddRequestHeadersAWrapper((hRequest), (headers), (len), (flags), (mod), (procList))

#define HTTP_SendRequest(hRequest, headers, headersLen, optional, optionalLen, mod, procList) \
    HttpSendRequestAWrapper((hRequest), (headers), (headersLen), (optional), (optionalLen), (mod), (procList))

/* WinINet does not need a separate receive-response step */
#define HTTP_ReceiveResponse(hRequest, mod, procList) (1)

#define HTTP_QueryHeaders(hRequest, infoLevel, buffer, bufLen, index, mod, procList) \
    HttpQueryInfoAWrapper((hRequest), (infoLevel), (buffer), (bufLen), (index), (mod), (procList))

#define HTTP_ReadData(hRequest, buffer, toRead, read, mod, procList) \
    InternetReadFileWrapper((hRequest), (buffer), (toRead), (read), (mod), (procList))

#define HTTP_CloseHandle(h, mod, procList) \
    InternetCloseHandleWrapper((h), (mod), (procList))

#endif /* USE_WINHTTP */
