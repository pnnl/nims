#!/usr/bin/env python 

""" 
A simple echo server 
""" 

import socket 
from time import sleep

#host = '127.0.0.1' 
host = 'we18320' 
port = 20001 
backlog = 5 
size = 4096

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
s.connect((host, port))

def hex_to_string(value):
    return value
    return "".join([chr(x) for x in value])

data = ""

while 1: 
    data += s.recv(5)
    #print hex_to_string(data)
    print data
