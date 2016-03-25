#!/usr/bin/env python 

""" 
A multithreaded socket server, which has a main thread that listens for
connections, and passes them off to a queue reader thread that reads data
from a message queue. Data from the message queue is then sent to worker
threads which write it to client sockets.

Messages are sent as JSON, and separated by a NUL byte. A single
message looks like this (tracks[] is a list):

{
    "frame_num": 1735,
    "num_tracks": 50,
    "ping_num_sonar": 29653,
    "tracks": [
        {
            "first_detect": 1458329600.0,
            "height": 0.0,
            "id": 20508,
            "last_pos_bearing": 1236.126708984375,
            "last_pos_elevation": 0.0,
            "last_pos_range": 5.508647918701172,
            "last_vel_bearing": Infinity,
            "last_vel_elevation": NaN,
            "last_vel_range": -Infinity,
            "length": 0.1428571492433548,
            "max_bearing_deg": 1338.540771484375,
            "max_elevation_deg": 0.0,
            "max_range_m": 5.538654327392578,
            "min_bearing_deg": 1000.0,
            "min_elevation_deg": 0.0,
            "min_range_m": 2.09869122505188,
            "pings_visible": 10,
            "size_sq_m": 0.8089292645454407,
            "speed_mps": 0.0,
            "target_strength": 0.0,
            "width": 4.687503814697266
        },
    ]
}

In order to parse multiple messages, a client needs to tokenise the
input stream with \0 as the message delimiter. The raw JSON probably
doesn't have newlines in it, but I didn't want to use that as a
delimiter. Messages are encoded as UTF-8.

The server won't start sending messages until it receives a message
from the client. Nominally this should be a JSON dictionary with
options; planned keys are "frequency" and "host" though neither are
implemented.

{ "frequency" : 10, "host" : _HOST, "heartbeat" : 60.0 }

Frequency is a rate limit in Hz, and host is an IP or FQDN of the
server. I'm not sure what my plan was for sending the server
address. Heartbeat requests a null byte every N seconds as proof-of-life.
Failure to receive a heartbeat would indicate a network issue or other
problem on the server side, so the client could try to reconnect.

Other possible options include gzip compression if bandwidth
turns out to be a problem.
 
""" 

import socket 
from time import sleep
import threading
from threading import Thread, current_thread
import signal
import os, sys
import json
from datetime import datetime

def log_error(s):
    """mimic format of boost log"""
    sys.stderr.write("%s [error] tracks_server.py (%d)\t%s\n" % (datetime.now(), os.getpid(), s))
    sys.stderr.flush()
    
if os.getenv("NIMS_HOME"):
    sys.path.append(os.path.join(os.getenv("NIMS_HOME"), "lib", "python"))

# should be in NIMS_HOME/lib/python, but may also be in working
# directory if we're debugging a standalone copy
try:
    import nims_py
except Exception, e:
    log_error("*** %s" % (e))
    log_error("*** Make sure NIMS_HOME is set in your environment")
    exit(1)
    
import ruamel.yaml

_mq_thread = None
_server_socket = None
_run = True

# list of all connected clients
_servers = []

# condition lock to protect _run and _servers, and signal changes in _run
_run_cond = threading.Condition()

def _tracker_queue_name():
    """Read from config file or use default /nims_tracker_socket"""
    nims_home = os.getenv("NIMS_HOME")
    if nims_home:
        yaml_path = os.path.join(nims_home, "config.yaml")
        with open(yaml_path, "r") as inf:
            conf = ruamel.yaml.load(inf, ruamel.yaml.RoundTripLoader)
            return os.path.join("/", conf["TRACKER_SOCKET_NAME"])
    
    queue_name = "/nims_tracker_socket"
    log_error("WARNING: NIMS_HOME not set in environment; trying queue %s" % (queue_name))
    return queue_name

def _server_port():
    """Read from config file or return default 8001"""
    nims_home = os.getenv("NIMS_HOME")
    if nims_home:
        yaml_path = os.path.join(nims_home, "config.yaml")
        with open(yaml_path, "r") as inf:
            conf = ruamel.yaml.load(inf, ruamel.yaml.RoundTripLoader)
            return conf["TRACKSERVER_SOCKET_PORT"]
            
    return 8001
    
class DetectionServer(object):
    """DetectionServer maintains state for each connection to the server."""
    def __init__(self, client_socket, address):
        super(DetectionServer, self).__init__()
        self._client_socket = client_socket
        self._client_address = address     
        self._messages = []
        self._condition = threading.Condition()
        self._run = True
        self._thread = Thread(target=self.run_server)
        self._options = {}
        self._thread.start()
    
    def enqueue_message(self, new_message):
        """API: call this when you get a new message."""
        self._condition.acquire()
        self._messages.append(new_message)
        self._condition.notify()
        self._condition.release()
        
    def stop_server(self):
        """API: call this to stop the server."""
        self._condition.acquire()
        self._run = False
        self._condition.notify()
        self._condition.release()
        self._thread.join()
        log_error("stopped server %s" % (self))
        
    def get_option(self, key):
        """value of key passed in from client or None"""
        if key in self._options:
            return self._options[key]
        return None
            
    def run_server(self):
        """SPI: separate thread to send messages to a connected client."""
        data = ""
        try:
            # !!! ok not to use condition here?
            while self._run:
                data = self._client_socket.recv(64)
                opts = json.loads(data, encoding="utf-8")
                if len(opts):
                    self._options = opts
                log_error("server received checkin data \"%s\" from %s" % (opts, self._client_socket.getpeername()))
                break
        except Exception, e:
            log_error("Failed to receive checkin from %s. Exception %s" % (self._client_socket.getpeername(), e))
        
        try:
            self._condition.acquire()
            while self._run:
                # block until signaled and lock acquired
                timeout = self.get_option("heartbeat")
                self._condition.wait(timeout)
                messages_to_send = list(self._messages)
                self._messages = list()
                self._condition.release()
                for msg in messages_to_send:
                    jv = json.dumps(msg.dict_value()).encode("utf-8")
                    #log_error("writing to socket: %s" % (jv))
                    self._client_socket.send(jv)
                    self._client_socket.send("\0")

                # heartbeat so the client knows we're still alive and doesn't
                # try to reconnect (which creates another thread)
                if len(messages_to_send) == 0:
                    log_error("sending heartbeat to %s" % (self.get_option("host")))
                    self._client_socket.send("\0")
                    
                # lock before entering top of loop and checking _run
                self._condition.acquire()                

            self._condition.release()
        except Exception, e:
            log_error("Failed to send data to server %s; assuming it disconnected. Exception %s" % (self._client_socket.getpeername(), e))
            # this server object is dead, so my as well remove it
            _run_cond.acquire()
            _servers.remove(self)
            _run_cond.release()     
            self._client_socket.shutdown(2)
            self._client_socket.close()
        
def run_data_server():
    """
    Read nims data from message queue or database store, and provide to
    all clients. We can't have want multiple threads reading from a 
    POSIX message queue, so use a producer-consumer architecture, with
    a single thread reading data from the tracker queue, and pushing
    it out to multiple server threads (which maintain their own
    copy of the data and sent/unsent state).
    
    """
    
    mq = nims_py.open_tracks_message_queue_py(_tracker_queue_name())
    global _run
    _run_cond.acquire()
    while _run:
        _run_cond.release()
        msg = nims_py.get_next_tracks_message_timed_py(mq, 0.1)
        if len(msg):
            for s in _servers:
                s.enqueue_message(msg)
        _run_cond.acquire()
    
    log_error("message queue thread exiting")
    _run_cond.release()     
        
def _sigterm_handler(signum, frame):
    """
    Propagate the SIGTERM to all worker threads, so our sockets get
    shut down properly and things are cleaned up. We have to stop
    each thread before the interpreter will exit.
    
    """
    
    log_error("caught signal to quit")
    _server_socket.close()
    
    # !!! need to stop _mq_thread
    global _run
    _run_cond.acquire()
    _run = False
    _run_cond.notify()
    _run_cond.release()
    
    for s in _servers:
        s.stop_server()    
    
    exit(0)

if __name__ == '__main__':
    
    # Handle signals manually; python handles all signals on the main
    # thread, and will not interrupt other threads (so signals as a
    # form of IPC are right out).
    signal.signal(signal.SIGTERM, _sigterm_handler)
    signal.signal(signal.SIGINT, _sigterm_handler)

    # empty string will listen on all available interfaces, so we
    # don't have to muck about with finding the right IP (which is
    # surprisingly hard) or install netifaces.
    host = ""
    
    port = _server_port() 
    backlog = 5 
    _server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    
    # tell the kernel to reuse a local socket in TIME_WAIT state, in
    # case we try to relaunch too soon (avoid "Address already in use").
    _server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    _server_socket.bind((host, port)) 
    _server_socket.listen(backlog) 
    
    _mq_thread = Thread(target=run_data_server)
    _mq_thread.start()
    
    # need to checkin with nims if we're launched from the config file
    nims_py.nims_checkin_py()

    while 1: 
        log_error("waiting for a connection")
        client_socket, address = _server_socket.accept() 
        _run_cond.acquire()
        _servers.append(DetectionServer(client_socket, address))
        _run_cond.release()


