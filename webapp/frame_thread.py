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
import numpy as np
import copy
import echometrics as em
import sys
import os

logging.basicConfig(level=logging.DEBUG,
					format='(%(threadName)-10s) %(message)s',
					)

import math

def generate_heat_map():
	cmap = {}
	for i in range(0, 256):
		color = (i ,int(.4*i), int(.2*i))
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
	#	print "v:", v
  
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
		self.frames = {}
		self.tracks = {} 

	def calculate_echometrics(self, framebuf):
		data = framebuf.image
		#maxVal = max(data)
		numX = framebuf.num_beams[0]
		numY = framebuf.num_samples[0]
		rangeMax = framebuf.range_max_m[0]

		xindex = range(1, numX+1)
		rangeStep = float(rangeMax) / numY
		yrange = []
		currentRange = 0
		i = 0
		while i < numY:
			yrange.append(currentRange)
			currentRange += rangeStep
			i+=1
		
		ar = np.array(data)
		#ar = em.to_dB(ar)
		ar = ar.reshape(numY, numX, )
	   


		echo = em.Echogram(ar, np.array(yrange), index=np.array(xindex))
		metrics = {}


		metrics['depth_integral'] = np.average(em.depth_integral(echo))

		sv = em.sv_avg(echo)
		sv = np.average(sv)
		#sv = 20 * math.log10(sv)
		metrics['avg_sv'] = sv


		metrics['center_of_mass'] = np.average(em.center_of_mass(echo))
		metrics['inertia'] = np.average(em.inertia(echo))
		metrics['proportion_occupied'] = np.average(em.proportion_occupied(echo))
		metrics['aggregation_index'] = np.average(em.aggregation_index(echo))
		metrics['equivalent_area'] = np.average(em.equivalent_area(echo))
	   # from pprint import pprint as pp
	 #   pp(metrics)
		return metrics

	def build_ppi_image(self, framebuf):
		if self.once == 1:
			return

		data = framebuf.image
		numX = framebuf.num_beams[0]
		numY = framebuf.num_samples[0]
		#print "num_beams:", numX
		#print "num_samples:", numY


		min_angle = framebuf.beam_angles_deg[0]
		max_angle = framebuf.beam_angles_deg[numX-1]
		sector_size = math.fabs(max_angle - min_angle)
		angle_offset = (180 - sector_size) / 2
		beam_width = sector_size / numX

		max_reach = numY * math.cos(math.radians(angle_offset))
		row = [(0,0,0)] * int(((max_reach/2) + 10))

		rows = []
		for y in range(0, (numY / 2)+10):
			rows.append(copy.copy(row))

		beam_angles = {}
		for x in range(0, numX):
			beam_angles[x] =  180 - angle_offset - (x * beam_width)


		for y in range(0, numY,2):
			for x in range(0, numX,2):
				xx = (numX-1) - x
				target = data[y + (xx * numY)]
				key = int(target * 350)
				if target > 0:
					target = 10 * math.log10(target)

				if key > 1000:
					key = 1000
				if key < 0:
					key = 0

				beam_angle = beam_angles[x]
				xcoord = y * math.cos(math.radians(beam_angle)) + (numY / 2.0)
				ycoord = y * math.sin(math.radians(beam_angle)) 
				xcoord = xcoord / 4
				ycoord = ycoord / 2
			 #   print "---"
			 #   print "r:", y, " t:", beam_angle
			 #   print "(", int(xcoord), ",", int(ycoord), ")"
				rows[int(ycoord)][int(xcoord)] = self.cmap[key]

		return rows

	def sparsify_image(self, frame, sparse_rate):
		data = frame.image
		numX = frame.num_beams[0]
		numY = frame.num_samples[0]

		new_data = []
		for y in range(0, numY, sparse_rate):
			for x in range(0, numX):
				xx = (numX-1) - x
				target = data[y + (xx * numY)]
				target = int(target * 1000)	
				new_data.append(target)
		
		print "Original Image Length: ", len(data)
		print "New Image Length: ", len(new_data)	
		return new_data

	def build_image(self, framebuf):
		if self.once == 1:
			return

		#timeStart = time.time()
		data = framebuf.image

		#maxVal = max(data)
		numX = framebuf.num_beams[0]
		numY = framebuf.num_samples[0]
		rangeMax = framebuf.range_max_m[0]
		rows = []

		for y in range(0, numY,2):
			row = []
			for x in range(0,numX,2):
				xx = (numX-1) - x

				target = data[y + (xx * numY)] 

				key = int(target *350) 
				if target > 0:
					target = 10* math.log10(target)
				#target = target / 4

				#key = int(target *1000) 
				if key > 1000:
					key = 1000
  
				if key < 0:
					key = 0
 
			#	pixels[x, y] = self.cmap[key]
				row.append(self.cmap[key])		  
			rows.append(row)


		#timeEnd = time.time()
		#print "image construct time:", timeEnd - timeStart
		return rows

	def generate_echometrics(self, framebuf):
		data = framebuf.image
		numX = framebuf.num_beams[0]
		numY = framebuf.num_samples[0]

		emdata = np.array(data)
		emdata.reshape(num_beams, num_samples)
		
	def run(self):
		logging.info('Running')
		generate_heat_map()
		 
		mqr_name = "/nim_display_" + repr(os.getpid())
		mqr = posix_ipc.MessageQueue(mqr_name, posix_ipc.O_CREAT)
		mqCheckin = posix_ipc.MessageQueue("/nims_framebuffer",posix_ipc.O_RDONLY)
		mqCheckin.send(mqr_name)
		logging.info("Connected to /nims_display_mq")

		mqTracker = posix_ipc.MessageQueue("/nims_tracker", posix_ipc.O_RDONLY)
		logging.info("Connected to /nims_tracker")

		time.sleep(1)
		#track_out = "tracks.txt"
		#f=open(track_out, 'r')

		#do_read = True
		#while (do_read):
		#	ping = f.readline()
		#	if len(ping) == 0:
		#		do_read = False
		#		continue
		#	ping = int(ping)
		#	numdetect = int(f.readline())
		#	self.tracks[ping] = []
		#	if numdetect > 0:
		#		ttracks = []
		#		for i in range(0, numdetect):
		#			line = f.readline()
		#			line.rstrip('\n')
		#			track = line.split()
		#
		#			track[0] = int(track[0])
		#			track[1] = int(track[1])
					
		#			ttracks.append([track])
		#		self.tracks[ping] = ttracks

		#f.close()

		#print self.tracks.keys()



		while True:
 
			mqr_recv = mqr.receive()
			mqr_recv = mqr_recv[0]

			buff = frames.frame_message(mqr_recv)
			if buff.valid is False:
				logging.info("invalid message: ignoring")
				continue

			try:
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

			self.frames[framebuf.ping_num[0]] = framebuf

			#print "looking for ping id:", framebuf.ping_num[0]
			#track_id = -1
			#while track_id != framebuf.ping_num[0]:
			#	track = mqTracker.receive()
		  	#	track = track[0]
			#	track_frame = frames.track_message(track)
			#	track_id = track_frame.pingid[0]

			#	print "want:", framebuf.ping_num[0], "-- found", track_frame.pingid[0]

			#print framebuf.ping_num[0], ":", track_frame.pingid[0]
			#if self.frames.has_key(track_frame.pingid[0]) == False:
				#print self.frames.has_key(track_frame.pingid[0])
				#print "Recv'd track for ping:", track_frame.pingid[0]
				#print " -- Missing frame buffer."
				#print self.frames.keys()
			#	continue

			#framebuf = self.frames.pop(track_frame.pingid[0])

            
			if len(self.clients) == 0:
				continue


			#image = self.build_image(framebuf)
			metrics = self.calculate_echometrics(framebuf)
			#framebuf.image = self.sparsify_image(framebuf)
			#framebuf.num_samples = (framebuf.num_samples[0] / 2, 0 )

			clients = copy.copy(self.clients)
			
			for client in clients:
				try:
					#print "trying to send data"
					#if self.tracks.has_key(framebuf.ping_num[0]):
					#	track_frame = self.tracks[framebuf.ping_num[0]]
					#else:
					#	track_frame = None
					#	print "missing track frame for:", framebuf.ping_num[0]
					client.send_data(framebuf, metrics, None)
				except:
					logging.info("Error sending image to client")
					print sys.exc_info()
					continue

			#logging.info("Num Clients: %d" % len(self.clients) )


		return

