#!/usr/bin/python

import ast
import logging
import os
import socket
from types import *

import ruamel.yaml
import tornado.httpserver
import tornado.ioloop
import tornado.options
import tornado.web
import tornado.websocket
from tornado.options import define, options

import frame_thread
from configwebsocket import ConfigWebSocket
from echowebsocket import EchoWebSocket

logging.basicConfig(level=logging.DEBUG,
                    format='("webserver") %(message)s',
                    )
logger = None
globalYAML = None

# The following are values used by Tornado
DEBUG = True
DIRNAME = os.path.dirname(__file__)
STATIC_PATH = os.path.join(DIRNAME, 'static')
TEMPLATE_PATH = os.path.join(DIRNAME, 'templates')

# Connections on various pages - Not sure this is ideal
echo_clients = set()
config_clients = set()


# Handler for index.html
class EchoHandler(tornado.web.RequestHandler):
    def data_received(self, chunk):
        pass

    def get(self, *args, **kwargs):
        self.render("index.html", host=self.request.host)


# Handler for config.html
class ConfigHandler(tornado.web.RequestHandler):
    def data_received(self, chunk):
        pass

    def get(self, *args, **kwargs):
        self.render("config.html", host=self.request.host)


# Web Connections from index.html callback here
def register_echo_client(echo_client, torf):
    if torf:
        echo_clients.add(echo_client)
        logger.info("New connection to EchoWebSocket.")
    else:
        echo_clients.discard(echo_client)
        logger.info("Lost connection to EchoWebSocket.")

    logger.info(str(len(config_clients) + len(echo_clients)) + " concurrent users.")


# Web Connections from config.html callback here
def register_config_client(config_client, torf):
    if torf:
        config_clients.add(config_client)
        print("New connection to ClientWebSocket", config_client)
    else:
        config_clients.discard(config_client)
        logger.info("Lost connection to ClientWebSocket")

    logger.info(str(len(config_clients) + len(echo_clients)) + " concurrent users.")


def edit_global_config(yaml=None, who=None):
    global globalYAML
    if yaml is None:
        return globalYAML
    else:
        for client in config_clients:
            if client != who:
                client.send_data(globalYAML)
        write_yaml_config()
        # tell nims/ingester/tracker to reload config
        os.system("/usr/bin/killall -HUP nims")


def write_yaml_config():
    global globalYAML
    yaml_path = os.path.join(os.getenv("NIMS_HOME", "../build"), "config.yaml")
    open(yaml_path, "w").write(ruamel.yaml.dump(globalYAML, Dumper=ruamel.yaml.RoundTripDumper))


def readYAMLConfig():
    """
    Reads the configuration file (formatted as YAML) and assigns it to variable globalYAML
    :return:
    """
    global globalYAML
    yaml_path = os.path.join(os.getenv("NIMS_HOME", "../build"), "config.yaml")
    try:
        globalYAML = ruamel.yaml.load(open(yaml_path, "r"), ruamel.yaml.RoundTripLoader)
    except:
        import yaml
        stream = file(globalYAML)
        globalYAML = yaml.load(stream)

    if type(globalYAML) == StringType:
        globalYAML = ast.literal_eval(globalYAML)
    logger.info('Configuration Path: ' + yaml_path)


def main():
    global logger
    logger = logging.getLogger('webserver')

    global globalYAML
    readYAMLConfig()
    port = 80  # this is the default port
    if globalYAML.has_key('WEB_SERVER_PORT'):
        port = globalYAML['WEB_SERVER_PORT']
    define("port", default=port, help="run on the given port", type=int)

    logger.info("Starting nims webserver with the following configuration:")
    import json
    logger.info(json.dumps(globalYAML, indent=2))
    logger.info('Running server on: ' + socket.gethostname() + ' (' + socket.gethostbyname(socket.gethostname()) +
                '): ' + str(port))
    tornado.options.parse_command_line()

    handlers = [
        (r"/", EchoHandler),
        (r"/index.html", EchoHandler),
        (r"/config.html", ConfigHandler),
        (r"/config", ConfigHandler),
        (r"/echosocket", EchoWebSocket, dict(register_socket=register_echo_client)),
        (
        r"/pagesocket", ConfigWebSocket, dict(register_socket=register_config_client, global_config=edit_global_config))
    ]

    settings = {
        "template_path": TEMPLATE_PATH,
        "static_path": STATIC_PATH,
        "debug": DEBUG
    }

    t = frame_thread.FrameThread(echo_clients, target="daemon")
    t.daemon = True
    t.start()

    application = tornado.web.Application(handlers, **settings)

    http_server = tornado.httpserver.HTTPServer(application)
    http_server.listen(options.port)
    tornado.ioloop.IOLoop.current().start()


if __name__ == "__main__":
    main()
