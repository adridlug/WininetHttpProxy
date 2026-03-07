# WinInet proxy

## Motivation

While reading [*Advanced Penetration Testing*] by Will Allsopp I came across a browser-pivoting technique that immediately caught my interest. A youtube video with the title [*DerbyCon 3 0 3205 Browser Pivoting Fu2fa Raphael Mudge*] on the same topic reinforced that curiosity. The technique touches on several different areas — network proxying, code injection, and Windows internals — so I started this repository as a hands-on project to explore and implement those concepts.

## Components

This repository contains small, focused Windows tooling written in C. The projects are related in that they are useful building blocks for *pivoting* and *traffic forwarding* scenarios (as well as for learning Windows internals).

| Directory | Description |
|-----------|-------------|
| `http_proxy/` | Local TCP listener that accepts a single HTTP/1.1 request from a client and re-issues it upstream over **HTTPS (443)** using **WinINet** (default) or **WinHTTP** (compile-time switch). Intended as a minimal "HTTP-in → HTTPS-out" forwarder rather than a full RFC-compliant proxy. |
| `code_injector/` | Process/code injection utilities intended to execute payloads (or run tooling) inside another process context. Useful as a supporting primitive when experimenting with in-memory execution and related Windows internals. |
| `https_request_forwarder/` | Companion HTTPS forwarding component. This is meant to be paired with `http_proxy/` in multi-hop / relay setups, where traffic is moved between hosts or network segments using HTTPS as the transport. |

---

## http_proxy

`http_proxy` is a lightweight **TCP-to-HTTPS request forwarder** for Windows.

It listens for inbound TCP connections on a local port, reads an HTTP/1.1 request, extracts the destination from the `Host:` header, and then performs an **outbound HTTPS request to port 443** using either:

- **WinINet** (default), or
- **WinHTTP** (when compiling with `USE_WINHTTP`)

The response from the upstream server is then relayed back to the original client.

> This is intentionally minimal and is **not** a full-featured HTTP proxy (no CONNECT tunneling, authentication, connection pooling, etc.). It’s designed as a small, understandable building block.

### Listening address / port

By default the proxy listens on:

- `0.0.0.0:3476` (all interfaces)
- Port: **3476**

### Data flow (conceptual)

1. Client connects to `http_proxy` over TCP
2. Client sends an HTTP/1.1 request (e.g., `GET /... HTTP/1.1` + `Host: example.com`)
3. `http_proxy` parses:
   - request line (method/path)
   - `Host:` header (destination)
4. `http_proxy` creates an outbound HTTPS connection to `Host` on **port 443**
5. Upstream response is read and written back to the client
6. Connection closes (no keep-alive)

### Configuration

| Parameter | Value | Where to change |
|-----------|-------|-----------------|
| Listening port | `3476` | `main()` in `http_porxy.c`, `htonsWrapper(3476, ...)` |
| Worker threads | `50` | `MAX_THREADS` macro in `http_porxy.c` |
| Request buffer | `4096` bytes | `REQUEST_SIZE` macro in `http_porxy.c` |
| HTTP backend | WinINet (default) / WinHTTP | Define `USE_WINHTTP` at compile time |

## Usage

The following steps describe how to compile `http_proxy` to a position-independent assembly stub and then reassemble it into a standalone executable. All commands are run from a **x64 Native Tools Command Prompt** (MSVC) inside the `http_proxy/` directory.

### Step 1 — Compile to assembly

```bat
cd http_proxy
.\compile_to_asm.bat
```

This invokes `cl.exe` with `/FA` (and related flags) to produce `http_proxy.asm` from the C source.

### Step 2 — Remove `.pdata` and `.xdata` segments from `http_proxy.asm`

Open `http_proxy.asm` in a text editor and **delete every section that begins with** either of the following segment declarations, including all lines up to and including the corresponding `ENDS` line:

```asm
; segment .pdata
_PDATA  SEGMENT
...
_PDATA  ENDS

; segment .xdata
_XDATA  SEGMENT
...
_XDATA  ENDS
```

These exception-handling metadata segments are not needed for a shellcode-style payload and will cause linker errors if left in place.

### Step 3 — Insert the `AlignRSP` stack-alignment stub

Immediately after the `_TEXT SEGMENT` line in `http_proxy.asm`, paste the contents of [`http_proxy/alignstack.txt`](http_proxy/alignstack.txt):

```asm
_TEXT	SEGMENT

AlignRSP PROC
    push rsi ; Preserve RSI since we're stomping on it
    mov rsi, rsp ; Save the value of RSP so it can be restored
    and rsp, 0FFFFFFFFFFFFFFF0h ; Align RSP to 16 bytes
    sub rsp, 020h ; Allocate homing space for ExecutePayload
    call main ; Call the entry point of the payload
    mov rsp, rsi ; Restore the original value of RSP
    pop rsi ; Restore RSI
    ret ; Return to caller
AlignRSP ENDP
```

### Step 4 — Remove the `FLAT:` prefix

In `http_proxy.asm`, find every occurrence of `FLAT:` (used by MASM for flat memory model addressing) and remove the prefix, leaving only the bare symbol name.

**Example:**

```asm
; Before
call    QWORD PTR [FLAT:__imp_WSAStartup]

; After
call    QWORD PTR [__imp_WSAStartup]
```

Use a global find-and-replace (`Ctrl+H` in most editors) to replace all instances of `FLAT:` with an empty string.

### Step 5 — Fix the Thread Environment Block (TEB) access

Find the line that reads:

```asm
mov rax, QWORD PTR gs:96
```

Replace it with:

```asm
mov rax, QWORD PTR gs:[96]
```

The bracket syntax is required by MASM for segment-relative memory operands.

### Step 6 — Assemble and link to an executable

```bat
.\compile_to_exe.bat
```

This invokes `ml64.exe` (the MASM assembler) and `link.exe` on the modified `http_proxy.asm` to produce the final `http_proxy.exe`.

### Quick test

A Python test script is included to test the http proxy component:
Just replace [INSERT HOST HERE] with a valid host name and call:
```bat
python http_proxy\test_con.py
```

The script connects to `localhost:3476` and sends a sample HTTP/1.1 GET request.

### End-to-end (inject + forwarder + browser)

The `http_proxy` component can also be run *in-memory* (injected into another process) and paired with the Python forwarder to proxy a real browser.

Overall flow:

1. Browser → `https_request_forwarder` on `localhost:8080`
2. `https_request_forwarder` → injected `http_proxy` on `localhost:3476`
3. injected `http_proxy` → upstream destination over **HTTPS (443)**

#### Step 7 — Extract the `.text` section as a C array and embed it into `code_injector`

1. Compile/link `http_proxy.exe` (Steps 1–6 above).
2. Extract the **`.text`** section bytes from `http_proxy.exe` (for example using **CFF Explorer**).
3. Convert the extracted `.text` section bytes into a **C array** (e.g., `unsigned char payload[] = {...};`).
4. Add that C array to the injector project and rebuild so `code_injector.exe` injects/executes this payload.

#### Step 8 — Inject the `http_proxy` payload into a process

Run `code_injector.exe [pid]` and inject the embedded `http_proxy` payload into a target process.

After successful injection, the injected `http_proxy` should be listening on:

- `0.0.0.0:3476` (default)

#### Step 9 — Start the request forwarder (`localhost:8080` → `localhost:3476`)

Start the forwarder:

```bat
python https_request_forwarder.py
```

This listens locally on `localhost:8080` and forwards requests to the injected proxy on `localhost:3476`.

#### Step 10 — Start Edge with proxy set to `localhost:8080`

Start the browser via with a configured http proxy with the address localhost:8080

### Notes and Limitations

- All upstream connections are made over **HTTPS** (port 443) regardless of the original request scheme.
- Only HTTP/1.1 requests with a `Host:` header are supported. The Host: header parser looks for the literal byte sequence \nHost:  — it will miss host: (lowercase) or headers that begin with Host: on the very first line.
- No authentication or CONNECT tunnel support.
- No persistent connections – each request opens a new upstream session.
- Designed for Windows only; uses Windows Sockets (ws2_32.dll) and WinINet/WinHTTP.
- Port is extracted from the Host: header value only via the default path; there is no explicit host:port split, so non-default ports in the Host: value are forwarded verbatim but the upstream TCP connection always uses port 443.
- The path parsing loop stops at the first space but does not guard against a missing second space (e.g., a malformed request line).
- No body (POST/PUT) forwarding – only the headers are forwarded upstream.
---

### Disclaimer
The information in this repository is for research and educational purposes and not meant to be used in production environments and/or as part of commercial products.
