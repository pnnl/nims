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
from struct import *
import mmap

# 3rd party modules
import posix_ipc


DEBUG = True
DIRNAME = os.path.dirname(__file__)
STATIC_PATH = os.path.join(DIRNAME, 'static')
TEMPLATE_PATH = os.path.join(DIRNAME, 'templates')
TIMER_DELAY=1

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
        print("WebSocket opened")
        clients.add(self)
        self.timer = Timer(TIMER_DELAY, self.on_message, args=["timer went off"])
        self.timer.start()

    def on_message(self, message):

        a={}
        image = self.generate_test_image()

        a["intensity"] = image
        a["com"] = random.randint(0, 100)
        a["sv"] = random.randint(0, 100)
        a["sa"] = random.randint(0, 100)
        self.write_message(a)
        self.timer = Timer(TIMER_DELAY, self.on_message, args=["timer went off"])
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
    mqr_name = "/nim_display_mq"
    mqr = posix_ipc.MessageQueue(mqr_name, posix_ipc.O_CREAT)
    mqCheckin = posix_ipc.MessageQueue("/nims_framebuffer",
        posix_ipc.O_RDONLY)
    mqCheckin.send(mqr_name)
    print "Connected to nims message queue."

    time.sleep(1)

    while True:
        print "waiting on recv..."
        mqr_recv = mqr.receive()
        print "mqr recv'd - attempting to parse ..."
        mqr_recv = mqr_recv[0]
        print "recv'd"
        buff = frames.frame_message(mqr_recv)
        if buff.valid is False:
            continue
        buff.print_message()

        print "attempting to connection shm ..."
        try:
            memory = posix_ipc.SharedMemory(buff.shm_location, posix_ipc.O_RDONLY,
                size=buff.frame_length)
        except:
            print "Error connecting to shared memory."
            continue

        mapfile = mmap.mmap(memory.fd, memory.size)
        memory.close_fd()
        frame_header = mapfile.read(buff.frame_length)
        mapfile.close()

        framebuf = frames.frame_buffer(frame_header)
        if framebuf.valid is False:
            continue
        framebuf.print_header()
        
        break

    print "building application"
    application = tornado.web.Application(handlers, **settings)

    http_server = tornado.httpserver.HTTPServer(application)
    http_server.listen(options.port)
    tornado.ioloop.IOLoop.current().start()


if __name__ == "__main__":
    main()
