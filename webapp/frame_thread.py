#!/usr/bin/python

# system
import copy
import logging
import os
import signal
import sys
import threading
import time
from mmap import mmap
from posix_ipc import MessageQueue, SharedMemory, O_CREAT, O_RDONLY, unlink_message_queue
from posix_ipc import ExistentialError
import numpy as np
import frames
import select
import ast

logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)-15s : %(levelname)s : line %(lineno)-4d in %(module)s.%(funcName)s   > %(message)s'
                    )
logger = logging.getLogger('frame_thread')


class FrameThread(threading.Thread):
    def __init__(self, config, clients, target=None, group=None):
        """
        Instances a thread to communicate with the transducer translation and tracking modules.

        Parameters
        ----------
        clients: list of class:trunk.webapp.echowebsocket.EchoWebSocket
        target: 'daemon'
        group: none
        """
        threading.Thread.__init__(self, name="mqThread", target=target, group=group)
        self.config = config

        self.display_q_name = "/nim_display_" + repr(os.getpid())
        self.em_q_name = '/nims_em_' + repr(os.getpid())
        self.display_q = None
        self.em_q = None

        logger.info('Instantiated')
        self.clients = clients
        self.once = 0
        self.frames = {}
        self.tracks = {}
        signal.signal(signal.SIGINT, self.signal_handler)

    def signal_handler(self, signal, frame):
        """
        SignalHandler to unlink the MessageQueue created by this thread.

        Parameters
        ----------
        signal: currently will always be signal.SIGINT
        frame: stackframe
        """
        try:
            logger.info('Unlinking MessageQueue::' + self.display_q_name)
            unlink_message_queue(self.display_q_name)

            unlink_message_queue(self.em_q_name)
        except ExistentialError as e:
            pass
        finally:
            sys.exit(0)

    def run(self):  # overrides threading.Thread.run()
        """
        Overrides threading.Thread.run()
        Creates initial connection to expected message queues; echometrics, frames, and tracks.  It polls each one
        waiting for the read-ready flag to be set and consumes the data.  It reformats the data into an object and
        writes it out to clients who are registered.

        Returns
        -------
        None
        """

        logger.info('Creating message_queues: ' + self.display_q_name)
        display_q = MessageQueue(self.display_q_name, O_CREAT)
        em_q = MessageQueue(self.em_q_name, O_CREAT)

        # TODO:  READ THE NIMS FRAMEBUFFER NAME FROM THE YAML
        try: # connect to nims ingestor to get backscatter data
            nims_framebuffer = '/' + self.config['FRAMEBUFFER_NAME']
            logger.info('Connecting to ' + nims_framebuffer)
            framebuffer_q = MessageQueue(nims_framebuffer, O_RDONLY)
            framebuffer_q.send(self.display_q_name)
            logger.info(" - sent queue: " + self.display_q_name)
        except ExistentialError as e:
            logger.info(' - Could not connect to ' + nims_framebuffer + '::' + e.__repr__())

        try: # connect to nims tracker to get track data (probably out of sync)
            tracker_name = '/' + self.config['TRACKER_NAME']
            logger.info('Connecting to ' + tracker_name)
            trackbuffer_q = MessageQueue(tracker_name, O_RDONLY)
        except ExistentialError as e:
            logger.info(' - Could not connect to ' + tracker_name + '::' + e.__repr__())


        try: # connect to the echometrics queue for periodic em data
            em_queue_name = self.config['ECHOMETRICS']['queue_name']

            logger.info('Connecting to ' + em_queue_name)
            em_request_q = MessageQueue(em_queue_name, O_RDONLY)
            em_request_q.send(self.em_q_name)
        except:
            logger.info(' - Could not connect to ' + em_queue_name)

        poller = select.poll()
        poller.register(em_q.mqd, select.POLLIN)
        poller.register(display_q.mqd, select.POLLIN)
        poller.register(trackbuffer_q.mqd, select.POLLIN)

        time.sleep(1)  # apparently necessary to create this latency for the frame_buffer app?

        while True:
            frame_buffer = None
            track_buffer = None
            em_buffer = None

            mq_state = poller.poll()
            for state in mq_state:
                mqd = state[0]
                if mqd == em_q.mqd:
                    buf = em_q.receive()[0]
                    em_buffer = ast.literal_eval(buf)
                elif mqd == display_q.mqd:
                    frame = frames.frame_message(display_q.receive()[0])
                    if frame.valid is False:
                        logger.info('Received invalid message: ignoring')
                        continue
                    try:
                        #logger.info(' -- Connecting to ' + frame.shm_location)
                        shm_frame = SharedMemory(frame.shm_location, O_RDONLY, size=frame.frame_length)
                    except StandardError as e:
                        logger.info(' -- Error connecting to', frame.shm_location, '::', e.__repr__())
                        continue
                    mapped = mmap(shm_frame.fd, shm_frame.size)
                    shm_frame.close_fd()
                    frame_buffer = frames.frame_buffer(mapped.read(frame.frame_length))
                    mapped.close()
                    if frame_buffer.valid is False:
                        logger.info(' -- Error Parsing Frame')
                        continue

                    image = np.array(frame_buffer.image)
                    image = image.reshape((frame_buffer.num_samples[0], frame_buffer.num_beams[0]))
                    image = image[0:-1:4, ::]
                    frame_buffer.image = image.flatten().tolist()
                    frame_buffer.num_samples = (frame_buffer.num_samples[0] /4,0)

                elif mqd == trackbuffer_q.mqd:
                    track_buffer = frames.track_message(trackbuffer_q.receive()[0])

            clients = copy.copy(self.clients)
            for client in clients:
                try:
                    if frame_buffer:
                        client.send_image(frame_buffer)
                    if track_buffer:
                        client.send_tracks(track_buffer)
                    if em_buffer:
                        client.send_metrics(em_buffer)

                except StandardError as e:
                    logger.info("Error sending data to client")
                    print sys.exc_info()
                    continue

        return
