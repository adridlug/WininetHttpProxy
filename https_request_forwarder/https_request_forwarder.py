#!/usr/bin/python           # This is server.py file                                                                                                                                                                           
import socket
import ssl               # Import socket module
import threading
import hexdump
import logger
def on_new_client(clientsocket,addr, logger):
    
    request = clientsocket.recv(40960)

    if (b"studon".lower() not in request.lower()):
        clientsocket.close()
        return
    
    logger.log(b'RECEiVED FROM BROWSER: '+request)
    
    clientsocket.sendall(b'HTTP/1.1 200 Connection Established\r\n\r\n')
    
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(certfile="[CertFile]", keyfile="[PrivKeyFle]")

    s_clientsocket = context.wrap_socket(clientsocket, server_side = True, do_handshake_on_connect = False)
    try:
        s_clientsocket.do_handshake()
    except :
        logger.log(b"ERROR:\rhandshake with brower failed\r\n\r\n")
        clientsocket.close()
        s_clientsocket.close()
        return

    request_data = s_clientsocket.recv(40960)
   
    logger.log(b'RECEiVED FROM BROWSER:\r'+request_data)

    tunn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        tunn.connect(("localhost", 3476))
    except:
        logger.log(b"ERROR:\rconnection to proxy failed\r\n\r\n")
        return
    
    tunn.sendall(request_data)

    logger.log(b"SENT TO PROXY:\r"+request_data)

    while True:
        data = tunn.recv(40960)
        logger.log(b'RECEiVED FROM PROXY:\r'+data)
        if data == b'':
            break
        
        #s_clientsocket.sendall(data)
        try:
            s_clientsocket.sendall(data)
            logger.log(b'SENT TO BROWSER:\r'+data)
        except Exception as e:
            logger.log(b"ERROR:\r Exception Type: {type(e).__name__}, Error Message: {str(e)}\r\n\r\n")
            hexdump.hexdump(request_data)

    clientsocket.close()

logger = logger.logger()
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.bind(('127.0.0.1', 8080))
    sock.listen(5)
    try:
        while True:
            conn, addr = sock.accept()
            #on_new_client(conn, addr)
            thread = threading.Thread(target=on_new_client, args=(conn, addr, logger))   
            thread.start()
    except KeyboardInterrupt:
        sock.close()
        logger.stop()

