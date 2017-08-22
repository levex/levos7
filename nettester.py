import socket
import sys
from thread import *
 
HOST = ''
PORT = 7548
 
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
print 'Socket created'
 
try:
    s.bind((HOST, PORT))
except socket.error as msg:
    print 'Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
    sys.exit()
     
print 'Socket bind complete'
 
s.listen(10)
print 'Socket now listening'
 
def clientthread(s, conn):
    conn.send('This is the Mac')
    conn.send('... and this is another message')
    data = conn.recv(1024)
    print("REPLY: " + str(data))
    conn.close()
    s.close()
    sys.exit()
     
    while True:
         
        data = conn.recv(1024)
        print("REPLY: " +str(data))
        if not data: 
            print("Disconnected!")
            break
    conn.close()
 
while 1:
    conn, addr = s.accept()
    print 'Connected with ' + addr[0] + ':' + str(addr[1])
     
    clientthread(s, conn)
    #start_new_thread(clientthread ,(s, conn,))
 
s.close()
