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
import datetime
import socket
from random import randint

logging.basicConfig(level=logging.DEBUG,
                    format='("webserver") %(message)s',
                    )

DEBUG = True
DIRNAME = os.path.dirname(__file__)
STATIC_PATH = os.path.join(DIRNAME, 'static')
TEMPLATE_PATH = os.path.join(DIRNAME, 'templates')

clients = set()
define("port", default=80, help="run on the given port", type=int)
class MainHandler(tornado.web.RequestHandler):
    #def get_template_path(self):
    #    return "templates/"

    def get(self, *args, **kwargs):
        print "MainHandler"
        cwd = os.getcwd()
        hostname = socket.gethostname()
        #print "hostname = ", hostname
        self.render("index.html", curdir=cwd, myhostname=hostname)
        
        
class ConfigHandler(tornado.web.RequestHandler):
    #def get_template_path(self):
    #    return "templates/"

    def get(self, *args, **kwargs):
        print "ConfigHandler"
        cwd = os.getcwd()
        hostname = socket.gethostname()
        #print "hostname = ", hostname
        self.render("config.html")
        
        
class EchoWebSocket(tornado.websocket.WebSocketHandler):       
    def open(self):
        clients.add(self)
        logging.info("Added client")
        print len(clients), "currently connected"
        #self.timer = Timer(.1, self.on_message, args=["timer went off"])
        #self.timer.start()
        self.com = 50
        self.sv = 50
        self.sa = 50

    def send_data(self, f, metrics, tracks):
        #logging.info("Sending image to client.")
        a={}
        
        # sonar meta data
        a["device"] = f.device #"Unknown"
        a["version"] = f.version[0]
        a["pingid"] = f.ping_num[0]
        a["ping_sec"] = f.ping_sec[0]
        a["ping_ms"] = f.ping_millisec[0]
        a["soundspeed"] = f.soundspeed_mps[0]
        a["num_samples"] = f.num_samples[0]
        a["range_min"] = "%.1f" %f.range_min_m[0]
        a["range_max"] = "%.lf" %f.range_max_m[0]
        a["num_beams"] = f.num_beams[0]
        a["freq"] = f.freq_hz[0]
        a["pulse_len"] = f.pulselen_microsec[0]
        a["pulse_rep"] = f.pulserep_hz[0]

     	a["min_angle"] = f.beam_angles_deg[0]
    	a["max_angle"] = f.beam_angles_deg[f.num_beams[0] -1]
     	a["sector_size"] = math.fabs(a["max_angle"] - a["min_angle"])

        # echo metrics 
        a["intensity"] = f.image
        a["center_of_mass"] = metrics['center_of_mass']
        a["sv_area"] = metrics['avg_sv']
        a["sv_volume"] = metrics['depth_integral']
        a['inertia'] = metrics['inertia']
        a['proportion_occupied'] = metrics['proportion_occupied']
        a['aggregation_index'] = metrics['aggregation_index']
        a['equivalent_area'] = metrics['equivalent_area']       
                

        #if tracks.num_detections[0] > 0:
        #    print "Sending track data"
        #    a['tracks'] = tracks.
        self.init_track = True
        if self.init_track:
            self.center_range = 300
            self.center_beam = 40
            self.is_new_track = True
            self.tracks = []
            track = (25, self.center_range, self.center_beam, self.is_new_track)
            self.tracks.append(track)
            self.init_track = False

        for i in range(len(self.tracks)):
            self.center_range += randint(-2, 2)
            self.center_beam += randint(-1, 1)
            self.is_new_track = False
            self.tracks[i] = (25, self.center_range, self.center_beam, self.is_new_track)

        a['tracks'] = self.tracks
        ts = datetime.datetime.fromtimestamp(f.ping_sec[0]).strftime("%H:%M:%S")
        ts = ts + ".%d" % f.ping_millisec[0]
        a["ts"] = ts

        

       # print "writing message..."
       # print a
      #  print "Ping:", a['pingid']    
        self.write_message(a)
    def on_message(self, message):

        a={}
        image = self.generate_test_image()

        #a["intensity"] = image
        a["com"] = self.com
        a["sv"] = self.sv
        a["sa"] = self.sa
        self.write_message(a)
        self.timer = Timer(.1, self.on_message, args=["timer went off"])
        self.timer.start()

    def on_close(self):
        print("WebSocket closed")
        print len(clients), "currently connected"
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



def readYAMLConfig():
    import yaml
    from pprint import pprint as pp
    with open("/home/rhytnen/Desktop/trunk/build/config.yaml", 'r') as stream:
       pp(yaml.load(stream))
        
    
def main():

    readYAMLConfig()
    tornado.options.parse_command_line()

    settings = {
        "static_path": os.path.join(os.getcwd(), "static"),
    }

    print "static_path:", os.path.join(os.getcwd(), "static")
    #application = tornado.web.Application([
    #    (r"/", MainHandler),
    #])

    handlers = [
        (r"/config", ConfigHandler),
        (r"/config.html", ConfigHandler),
        (r"/", MainHandler),
        (r"/index.html", MainHandler),
        (r"/websocket", EchoWebSocket)
    ]
    
    settings = {
        "template_path": TEMPLATE_PATH,
        "static_path": STATIC_PATH,
        "debug": DEBUG
    }

    # first connect to the message q



    t = frame_thread.frameThread(clients, target="daemon")
    t.daemon = True
    t.start()

    application = tornado.web.Application(handlers, **settings)

    http_server = tornado.httpserver.HTTPServer(application)
    http_server.listen(options.port)
    tornado.ioloop.IOLoop.current().start()


if __name__ == "__main__":
    main()
