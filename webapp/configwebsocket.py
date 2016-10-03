import sys

import tornado.websocket
import json


class ConfigWebSocket(tornado.websocket.WebSocketHandler):
    def initialize(self, register_socket, global_config):
        """
        Proxy for __init__.  The baseclass tornado.websocket.WebSocketHandler sometimes has funky signature problems.
        Args:
            register_socket: A function to register the client when the socket opens.
            global_config: The dictionary representing the NIMS config file.
        """
        print "ConfigWebSocket::__init__()"
        self.register_socket = register_socket
        self.config = global_config()
        self.global_config = global_config;

    def open(self):
        """
        When the websocket opens, this function registers the client to have data written to it.
        """
        print "ConfigWebSocket::Open()"
        self.register_socket(self, True)
        try:
            self.send_data(self.config)
        except:
            print "Unexpected error:", sys.exc_info()[0]

    def on_message(self, message):
        """
        When a message is recieved from the webclient, it will be an update to the config file.  We convert to JSON and
        forward the new configuration on to other clients as well as rewriting the config file.
        Args:
            message: unicode string representing the k,v pairs.
        """
        print "ConfigWebSocket::OnMessage()"
        config = json.loads(message)
        if config.has_key('type'):
            config.pop('type')
        self.global_config(config, self);

    def send_data(self, config):
        """
        Forward the new JSON config to the webclient.
        Args:
            config:
        """
        print "ConfigWebSocket::send_data()"
        self.write_message(config)

    def on_close(self):
        """
        Close out the socket when it's dropped.
        """
        self.register_socket(self, False)
