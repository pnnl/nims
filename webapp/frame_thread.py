#!/usr/bin/python

# system
import copy
import logging
import os
import signal
import sys
import threading
import time
import traceback
from mmap import mmap

# 3rd party
from posix_ipc import MessageQueue, SharedMemory, O_CREAT, O_RDONLY, unlink_message_queue
from posix_ipc import ExistentialError
import numpy as np
import echometrics
# user
import frames

logging.basicConfig(level=logging.DEBUG,
                    format='%(asctime)-15s : %(levelname)s : line %(lineno)-4d in %(module)s.%(funcName)s   > %(message)s'
                    )
logger = logging.getLogger('frame_thread')


class FrameThread(threading.Thread):
    def __init__(self, clients, target=None, group=None):
        """
        Instances a thread to communicate with the transducer translation and tracking modules.

        Parameters
        ----------
        clients: list of class:trunk.webapp.echowebsocket.EchoWebSocket
        target: 'daemon'
        group: none
        """
        threading.Thread.__init__(self, name="mqThread", target=target, group=group)
        self.display_q_name = "/nim_display_" + repr(os.getpid())
        self.display_q = None
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
        except ExistentialError as e:
            pass
        finally:
            sys.exit(0)

    def run(self):  # overrides threading.Thread.run()
        """
        Overrides threading.Thread.run()
        Creates initial connection to expected message queues, receives frames, calculates echometrics and
        disperses the data to the web server for hand off.

        Returns
        -------
        None
        """

        logger.info('Creating message_queue: ' + self.display_q_name)
        display_q = MessageQueue(self.display_q_name, O_CREAT)

        # TODO:  READ THE NIMS FRAMEBUFFER NAME FROM THE YAML
        try:
            logger.info('Connecting to /nims_framebuffer')
            framebuffer_q = MessageQueue("/nims_framebuffer", O_RDONLY)
            framebuffer_q.send(self.display_q_name)
            logger.info(" - sent queue: " + self.display_q_name)
        except ExistentialError as e:
            logger.info(' - Could not connect to /nims_framebuffer::' + e.__repr__())
        # mqTracker = posix_ipc.MessageQueue("/nims_tracker", posix_ipc.O_RDONLY)
        # logger.info("Connected to /nims_tracker")

        time.sleep(1)  # apparently necessary to create this latency for the frame_buffer app?

        while True:
            frame = frames.frame_message(display_q.receive()[0])
            if frame.valid is False:
                logger.info('Received invalid message: ignoring')
                continue
            try:
                logger.info(' -- Connecting to::' + frame.shm_location)
                shm_frame = SharedMemory(frame.shm_location, O_RDONLY, size=frame.frame_length)
            except StandardError as e:
                logger.info(' -- Error connecting to', frame.shm_location, '::', e.__repr__())
                continue

            logger.info('Connecting to memory map')
            mapped = mmap(shm_frame.fd, shm_frame.size)
            shm_frame.close_fd()
            frame_buffer = frames.frame_buffer(mapped.read(frame.frame_length))
            mapped.close()
            if frame_buffer.valid is False:
                logger.info(' -- Error Parsing Frame')
                continue

            # self.frames[frame_buffer.ping_num[0]] = frame_buffer
            logger.info('Calculating EchoMetrics')
            echo_metrics = calculate_echometrics(frame_buffer)

            clients = copy.copy(self.clients)
            logger.info('Sending frame to clients')
            for client in clients:
                try:
                    client.send_data(frame_buffer, echo_metrics, None)
                except StandardError as e:
                    logger.info("Error sending image to client")
                    print sys.exc_info()
                    continue

        return


def calculate_echometrics(frame_buffer):
    """
    Parameters
    ----------
    frame_buffer:class:frames.frame_buffer

    Returns
    -------
    metrics:dict
    """

    try:
        image_data = frame_buffer.image
        num_beams = frame_buffer.num_beams[0]
        num_samples = frame_buffer.num_samples[0]
        range_max = frame_buffer.range_max_m[0]
    except AttributeError as e:
        print calculate_echometrics.__name__, "failed to parse frame_buffer:", e
        import traceback
        line_no = traceback.extract_stack()[-1][1]
        print 'line:', line_no
        return None

    x_index = range(1, num_beams + 1)
    range_step = float(range_max) / num_samples
    y_range = [i * range_step for i in range(0, num_samples)]
    array = np.array(image_data).reshape(num_samples, num_beams, )

    echo = echometrics.Echogram(array, np.array(y_range), index=np.array(x_index))
    sv = np.average(echometrics.sv_avg(echo))
    metrics = dict(depth_integral=np.average(echometrics.depth_integral(echo)),
                   avg_sv=sv,
                   center_of_mass=np.average(echometrics.center_of_mass(echo)),
                   inertia=np.average(echometrics.inertia(echo)),
                   proportion_occupied=np.average(echometrics.proportion_occupied(echo)),
                   aggregation_index=np.average(echometrics.aggregation_index(echo)),
                   equivalent_area=np.average(echometrics.equivalent_area(echo)))

    return metrics
