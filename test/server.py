#!/usr/bin/env python 

""" 
A simple echo server 
""" 

import socket 
from time import sleep

host = '127.0.0.1' 
port = 20001 
backlog = 5 
size = 1024
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
s.bind((host,port)) 
s.listen(backlog) 
count = 0

def hex_string(value):
    return value
    return "".join(["\\x%02X" % ord(x) for x in value])

while 1: 
    print "waiting for a connection"
    client, address = s.accept() 
    try:
        while 1:
            data = "test %d" % (count) #client.recv(size) 
            count += 1
            client.send(hex_string(data).encode("utf-8")) 
            sleep(2)
    except Exception, e:
        print "failed to send data", e
