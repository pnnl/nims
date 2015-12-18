import tornado.websocket
import sys

class ConfigWebSocket(tornado.websocket.WebSocketHandler):       

    def open(self):
        print "ConfigWebSocket::open()"
        
    def initialize(self, register_socket, global_config):
        print "ConfigWebSocket::__init__()"     
        self.register_socket = register_socket
        self.config = global_config()
        self.global_config = global_config;

    def open(self):
        print "ConfigWebSocket::Open()"
        self.register_socket(self, True)
        try:
            self.send_data(self.config)
        except:
            print "Unexpected error:", sys.exc_info()[0]

    def on_message(self, message):
        print "ConfigWebSocket::OnMessage()"
        self.global_config(message, self);

    def send_data(self, config):
        print "ConfigWebSocket::send_data()"
        self.write_message(config)

    def on_close(self):
        self.register_socket(self, False)

