#!/usr/bin/python

import threading
import logging
# 3rd party modules
import posix_ipc
import time
import frames
from struct import *
import mmap
from PIL import Image
import numpy
logging.basicConfig(level=logging.DEBUG,
                    format='(%(threadName)-10s) %(message)s',
                    )

import math

def generate_heat_map():
    cmap = {}
    for i in range(0, 256):
        color = (i,int(.5*i), 0)
        for j in range(0, 4):
            cmap[(i * 4) + j] = color

    return cmap

    #heat map
    for i in range(255, -1, -1):
        color = (0, 255-i, i)
        v = (256.0 - i) / 512.0
        v = int(math.ceil(v * 1000))
        cmap[v] = color
     #   print "v:", v

    for i in range(255, -1, -1):
        color = (255-i, i, 0)
        v = (512.0 - i) / 512.0
        v = int(math.ceil(v * 1000))
        cmap[v] = color
    #    print "v:", v
  
    cmap[0] = (0, 0, 0)

    for i in range(0, 1001):
        if not cmap.has_key(i):
            cmap[i] = cmap[i-1]

   # print "NumColors:", len(cmap)

    return cmap



class frameThread(threading.Thread):

    def __init__(self, clients, target=None, group=None):
        self.clients = clients
        threading.Thread.__init__(self, name="mqThread", target=target, group=group)
        logging.info('Created')
        self.cmap = generate_heat_map()
        self.once = 0

    def build_image(self, framebuf):
        if self.once == 1:
            return

        #print "constructing image..."
        #timeStart = time.time()
        data = framebuf.image
      #  print "len data: ", len(data)
      #  data = numpy.array(data)
      #  print "len data: ", len(data)
        numX = framebuf.num_beams[0]
        numY = framebuf.num_samples[0]
        rangeMax = framebuf.range_max_m[0]
        rows = []
      #  image = Image.new("RGB", (numX, numY))
        #image = Image.fromarray("F", (numX, numY),data)
        #image.show()
        #return

        #pixels = image.load()
        for y in range(0, numY,2):
            row = []
            for x in range(0,numX,2):
                xx = (numX-1) - x
                target = data[y + (xx * numY)]
                
                #key = min(keys, key=lambda x:abs(x-target))
                key = int(target * 1000)
                if key > 1000:
                    key = 1000
                if key < 0:
                    key = 0
            #    pixels[x, y] = self.cmap[key]
                row.append(self.cmap[key])          
            rows.append(row)
        #image.show()
        #self.once = 1
        #timeEnd = time.time()
        #print "time:", timeEnd - timeStart
        return rows

    def run(self):
        logging.info('Running')
        generate_heat_map()
         
        mqr_name = "/nim_display_mq"
        mqr = posix_ipc.MessageQueue(mqr_name, posix_ipc.O_CREAT)
        mqCheckin = posix_ipc.MessageQueue("/nims_framebuffer",posix_ipc.O_RDONLY)
        mqCheckin.send(mqr_name)
        logging.info("Connected to /nims_display_mq")
        time.sleep(1)

        while True:
            mqr_recv = mqr.receive()
            logging.info("recv'd message")
            mqr_recv = mqr_recv[0]
            buff = frames.frame_message(mqr_recv)
            if buff.valid is False:
                logging.info("invalid message: ignoring")
                continue

            buff.print_message()

            try:
                logging.info("connecting to shared memmory")
                memory = posix_ipc.SharedMemory(buff.shm_location, posix_ipc.O_RDONLY,
                    size=buff.frame_length)
            except:
                logging.info("Error connecting to shared memory.")
                continue

            mapfile = mmap.mmap(memory.fd, memory.size)
            memory.close_fd()
            frame_header = mapfile.read(buff.frame_length)
            mapfile.close()

            framebuf = frames.frame_buffer(frame_header)
            if framebuf.valid is False:
                continue
            framebuf.print_header()
           
            image = self.build_image(framebuf)
  
            logging.info("sending image to clients...")
            for client in self.clients:
                client.send_image(image, framebuf)

            #logging.info("Num Clients: %d" % len(self.clients) )


        return

