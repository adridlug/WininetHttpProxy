import socket

tunn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    tunn.connect(("localhost", 3476))
except:
    print("error")

while True:
    request = (
    "GET /WP/ HTTP/1.1\r\n"
    "Host: [INSERT HOST HERE]\r\n"
    "Connection: close\r\n\r\n"
    )
    
    print(b"sending: "+ request.encode())

    tunn.sendall(request.encode())
    while True:
        data = tunn.recv(40960)
        
        if not data:
            break

        print(b'RECEiVED FROM INJECTED PAYLOAD:\r'+data)
    
    

