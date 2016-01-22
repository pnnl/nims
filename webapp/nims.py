#!/usr/bin/python

import tornado.httpserver
import tornado.ioloop
import tornado.options
import tornado.web
import tornado.websocket
from tornado.options import define, options

import os
import socket


import frame_thread
from echowebsocket import EchoWebSocket
from configwebsocket import ConfigWebSocket

import logging
logging.basicConfig(level=logging.DEBUG,
                    format='("webserver") %(message)s',
                    )

globalYAML = None
DEBUG = True
DIRNAME = os.path.dirname(__file__)
STATIC_PATH = os.path.join(DIRNAME, 'static')
TEMPLATE_PATH = os.path.join(DIRNAME, 'templates')

echo_clients = set()
config_clients = set()



#Handler for index.html
class EchoHandler(tornado.web.RequestHandler):
    def get(self, *args, **kwargs):
        cwd = os.getcwd()
        self.render("index.html", host=self.request.host)

#Handler for config.html
class ConfigHandler(tornado.web.RequestHandler):
    def get(self, *args, **kwargs):    
        cwd = os.getcwd()
        hostname = socket.gethostname()
        self.render("config.html",  host=self.request.host)


# Webconnections from index.html callback here
def registerEchoClient(echo_client, torf):
    if torf == True:
        echo_clients.add(echo_client)
        logging.info("New connection to EchoWebSocket.")
    else:
        echo_clients.discard(echo_client)
        logging.info("Lost connection to EchoWebSocket.")

    logging.info(str(len(config_clients) + len(echo_clients)) + " concurrent users.")

# Webconnections from config.html callback here
def registerConfigClient(config_client, torf):
    if torf == True:
        config_clients.add(config_client)
        print("New connection to ClientWebSocket", config_client)
    else:
        config_clients.discard(config_client)
        logging.info("Lost connection to ClientWebSocket")

    logging.info(str(len(config_clients) + len(echo_clients)) + " concurrent users.")

def editGlobalConfig(yaml=None, who=None):
    print yaml
    global globalYAML
    if yaml == None:
        return globalYAML
    else:
        globalYAML = yaml
        for client in config_clients:
            if client != who:
                client.send_data(globalYAML)

        writeYAMLConfig()
        # tell nims/ingester/tracker to reload config
        os.system("/usr/bin/killall -HUP nims")

def writeYAMLConfig():
    print "in write YAML ..."
    globalYAML
    import yaml
    yaml_path = os.path.join(os.getenv("NIMS_HOME"), "config.yaml")
    yaml_file = open(yaml_path, 'w')
    yaml.dump(globalYAML, yaml_file, encoding=('utf-8'))
    yaml_file.close()



def readYAMLConfig():
    """
    Reads the configuration file (formatted as YAML) and assigns it to variable globalYAML
    :return:
    """
    global globalYAML
    import yaml
    yaml_path = os.path.join(os.getenv("NIMS_HOME"), "config.yaml")
    with open(yaml_path, 'r') as stream:
       globalYAML = yaml.load(stream)

    
def main():
    global globalYAML
    readYAMLConfig()
    port = 80 #this is the default port
    if globalYAML.has_key('WEB_SERVER_PORT'):
        port = globalYAML['WEB_SERVER_PORT']
    define("port", default=port, help="run on the given port", type=int)


    logging.info("Starting nims webserver with the following configuration:")
    from pprint import pprint as pp
    pp(globalYAML)

    print "Running server on:", socket.gethostname(),"(",socket.gethostbyname(socket.gethostname()), '):', port
    tornado.options.parse_command_line()

    settings = {
        "static_path": os.path.join(os.getcwd(), "static"),
    }


    handlers = [
        (r"/", EchoHandler),
        (r"/index.html", EchoHandler),
        (r"/config.html", ConfigHandler),
        (r"/config", ConfigHandler),
        (r"/echosocket", EchoWebSocket, dict(register_socket=registerEchoClient)),
        (r"/pagesocket", ConfigWebSocket, dict(register_socket=registerConfigClient, global_config = editGlobalConfig))
    ]
    
    settings = {
        "template_path": TEMPLATE_PATH,
        "static_path": STATIC_PATH,
        "debug": DEBUG
    }

  
    t = frame_thread.frameThread(echo_clients, target="daemon")
    t.daemon = True
    t.start()

    application = tornado.web.Application(handlers, **settings)

    http_server = tornado.httpserver.HTTPServer(application)
    http_server.listen(options.port)
    tornado.ioloop.IOLoop.current().start()


if __name__ == "__main__":
    main()
