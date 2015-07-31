#!/usr/bin/python

import tornado.httpserver
import tornado.ioloop
import tornado.options
import tornado.web
import tornado.websocket
from tornado.options import define, options
from threading import Timer
import os
import math
import random
import time
import frames
import frame_thread
from struct import *
import mmap
import logging



logging.basicConfig(level=logging.DEBUG,
                    format='("webserver") %(message)s',
                    )

DEBUG = True
DIRNAME = os.path.dirname(__file__)
STATIC_PATH = os.path.join(DIRNAME, 'static')
TEMPLATE_PATH = os.path.join(DIRNAME, 'templates')

clients = set()
define("port", default=8888, help="run on the given port", type=int)
class MainHandler(tornado.web.RequestHandler):
    #def get_template_path(self):
    #    return "templates/"

    def get(self):
        cwd = os.getcwd()
        self.render("index.html", curdir=cwd)

class EchoWebSocket(tornado.websocket.WebSocketHandler):
    def open(self):
        clients.add(self)
        #self.timer = Timer(.1, self.on_message, args=["timer went off"])
        #self.timer.start()

    def send_image(self, image, f):
        logging.info("Sending image to client.")
        a={}
        
        a["device"] = "Unknown"
        a["version"] = f.version[0]
        a["pingid"] = f.ping_num[0]
        a["ping_sec"] = f.ping_sec[0]
        a["ping_ms"] = f.ping_millisec[0]
        a["soundspeed"] = f.soundspeed_mps[0]
        a["num_samples"] = f.num_samples[0]
        a["range_min"] = f.range_min_m[0]
        a["range_max"] = f.range_max_m[0]
        a["num_beams"] = f.num_beams[0]
        a["freq"] = f.freq_hz[0]
        a["pulse_len"] = f.pulselen_microsec
        a["pulserep"] = f.pulserep_hz

        a["intensity"] = image
        a["com"] = random.randint(0, 100)
        a["sv"] = random.randint(0, 100)
        a["sa"] = random.randint(0, 100)               
        
        self.write_message(a)

    def on_message(self, message):

        a={}
        image = self.generate_test_image()

        a["intensity"] = image
        a["com"] = random.randint(0, 100)
        a["sv"] = random.randint(0, 100)
        a["sa"] = random.randint(0, 100)
        self.write_message(a)
        self.timer = Timer(.1, self.on_message, args=["timer went off"])
        self.timer.start()

    def on_close(self):
        print("WebSocket closed")
        clients.discard(self)

    def generate_test_image(self):
        rows = []
        for i in range(0,255):
            row = []
            for j in range(0,255):
                b = random.randint(0, 255)
                row.append((0, 0, b))
            rows.append(row)
        return rows


def main():
    tornado.options.parse_command_line()

    settings = {
        "static_path": os.path.join(os.getcwd(), "static"),
    }

    print "static_path:", os.path.join(os.getcwd(), "static")
    #application = tornado.web.Application([
    #    (r"/", MainHandler),
    #])

    handlers = [
        (r"/", MainHandler),
        (r"/websocket", EchoWebSocket)
    ]
    
    settings = {
        "template_path": TEMPLATE_PATH,
        "static_path": STATIC_PATH,
        "debug": DEBUG
    }

    # first connect to the message q



    t = frame_thread.frameThread(clients, target="daemon")
    t.start()

    application = tornado.web.Application(handlers, **settings)

    http_server = tornado.httpserver.HTTPServer(application)
    http_server.listen(options.port)
    tornado.ioloop.IOLoop.current().start()


if __name__ == "__main__":
    main()
