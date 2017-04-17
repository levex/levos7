import socket
import sys
from thread import *
 
HOST = ''
PORT = 7548
 
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print 'Socket created'
 
try:
    s.bind((HOST, PORT))
except socket.error as msg:
    print 'Bind failed. Error Code : ' + str(msg[0]) + ' Message ' + msg[1]
    sys.exit()
     
print 'Socket bind complete'
 
s.listen(10)
print 'Socket now listening'
 
def clientthread(conn):
    conn.send('This is the Mac')
     
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
     
    start_new_thread(clientthread ,(conn,))
 
s.close()
