#!/usr/bin/python

import logging
import numpy as np
import os
import select
import signal
import sys
from mmap import mmap
from posix_ipc import ExistentialError
from posix_ipc import MessageQueue, SharedMemory, O_CREAT, O_RDONLY, O_RDWR
import ruamel.yaml
from types import *
import ast
import echometrics
import frames

log_format = '%(asctime)-15s : %(levelname)s : line %(lineno)-4d in %(module)s.%(funcName)s   > %(message)s'
logging.basicConfig(level=logging.DEBUG, format = log_format)
logger = logging.getLogger('realtime_echometrics')

global continue_running


def signal_handler(signal, data):
    """
    Handles the ctrl-c keyboard input and kills the loop
    Args:
        signal: the signal fired
        data:  none
    """
    logger.info('Stop running')
    global continue_running
    continue_running = False

def do_exit(mq):
    """
    Executed when the program is finished.
    It is generally not useful to call on it's own as it's job is to clean up message queues and shut the application.
    Args:
        mq: A list of message queues to close.
    """
    try:
        logger.info('Cleaning up.')
        logger.info(' - closing & destructing MessageQueue')
        MessageQueue.close(mq)
        MessageQueue.unlink_message_queue(mq)
        logger.info('Finished.')
    except ExistentialError as e:
        pass
    finally:
        sys.exit(0)


def fetch_framebuffer(mq):
    """
    When the frame_buffer message queue is ready for read, this function is called to read the data and cast it
    to a frame object.
    Args:
        mq: The message queue containing the frame.

    Returns:frames.frame_buffer

    """
    frame = frames.frame_message(mq.receive()[0])
    if frame.valid is False:
        logger.info(' -- Received invalid message: ignoring')
        return None
    try:
        #logger.info(' -- Connecting to ' + frame.shm_location)
        shm_frame = SharedMemory(frame.shm_location, O_RDONLY, size=frame.frame_length)
    except StandardError as e:
        logger.info(' -- Error connecting to', frame.shm_location, '::', e.__repr__())
        return None

    mapped = mmap(shm_frame.fd, shm_frame.size)
    shm_frame.close_fd()
    frame_buffer = frames.frame_buffer(mapped.read(frame.frame_length))
    mapped.close()

    if frame_buffer.valid is False:
        logger.info(' -- Error Parsing Frame')
        return None

    return frame_buffer

def accept_new_client(mq):
    """
    When a process connects to the request queue, this function is called to connect to the private queue for that
    process.  Throws an exception if it can't connect.
    Args:
        mq: The echometrics request queue

    Returns:The private messagequeue associated with the clint or None.

    """
    try:
        q_name = mq.receive()[0]
        print 'Client request:', q_name
        client_q = MessageQueue(q_name, O_RDWR)
        print ' -- connected'

        return client_q
    except:
        logger.info('error connecting to:' + q_name)
        return None

def background_removal(bs, frequency, min_range, max_range, pulse_len, sound_speed = 1484.0):
    """
    Based on the Robertis algorithm, take backscatter in db, discover the average noise level and remove it from
    the data.
    Args:
        bs: The array-like of backscatter data
        frequency: Transducer frequency
        min_range: The minimum range to average noise at.
        max_range: The maximum range to average noise at.
        pulse_len: Tau in seconds
        sound_speed: Sound velocity in m/s

    Returns: Backscatter with noise removed.

    """
    num_samples, num_pings = bs.shape
    frequency = frequency
    meters_per_sample = (max_range - min_range) / num_samples

    absorption_coeff = ((sound_speed * 10**-6) * frequency**2) / (frequency**2 + sound_speed) + ((sound_speed * 10**-3) * 10**-7 * frequency**2)
    range_vector = np.linspace(min_range, max_range, num_samples)
    absorption_matrix = np.array([range_vector * 2 * absorption_coeff] * num_pings).T
    tvg_matrix = np.array([20 * np.log10(range_vector - (pulse_len * sound_speed / 4))] * num_pings).T
    power_calij = bs - tvg_matrix - absorption_matrix

    num_K = int(np.round(1.92 / meters_per_sample))
    K = num_samples / num_K

    noise = np.min([np.mean(power_calij[k*num_K:k*num_K+num_K]) for k in range(K)])
    sv_corr = 10 * np.log10(10**(bs/10.0) - 10**(noise/10.0))

    return sv_corr

def calculate_echometrics(frame_buffers, depth_to_bin):
    """
    Given a set of pings, calculate the echometrics values based on the depth bin provided.
    Args:
        frame_buffers: array-like of single beam data (ping_bin x num_samples)
        depth_to_bin: the depth bins to average on

    Returns: A dict of metric values as described by Urmy 2012

    """
    depth_to_bin = int(depth_to_bin)
    print 'depth to bin is ...', depth_to_bin
    num_beams = len(frame_buffers)
    num_samples = frame_buffers[0].num_samples[0]
    max_range = frame_buffers[0].range_max_m[0]
    min_range = frame_buffers[0].range_min_m[0]
    first_ping = frame_buffers[0].ping_num[0]
    last_ping = frame_buffers[-1].ping_num[0]
    freq_khz = frame_buffers[0].freq_hz[0] / 1000.0
    pulse_len = frame_buffers[0].pulselen_microsec[0] * 10**-6

    echogram = np.zeros((len(frame_buffers), num_samples))
    for i in range(len(frame_buffers)):
        echogram[i] = frame_buffers[i].image
    echogram = echogram.T
    bg_removed_echogram = background_removal(np.copy(echogram), freq_khz, min_range, \
                          max_range, pulse_len, 1486.0)

    #logger.info(' -- Generating New Echogram')

    # calculating over 1 meter bins, change to use env variable
    samples_per_meter = num_samples / (max_range - min_range)
    start_sample = 0
    sample_extent = int(np.round(depth_to_bin * samples_per_meter))
    stop_sample = num_samples - 1 - sample_extent
    intervals = range(start_sample, stop_sample, sample_extent)

    logger.info(' -- Calculating Metrics')
    index = np.arange(num_beams) + 1 # TODO: change to ping bins
    range_step = np.absolute(max_range - min_range) / num_samples

    metrics = dict(depth_integral=[],
               avg_sv=[],
               center_of_mass=[],
               inertia=[],
               proportion_occupied=[],
               aggregation_index=[],
               equivalent_area=[])

    for interval in intervals:
        depths = np.array([((i * range_step) + min_range) for i in range(interval, interval + sample_extent)])
        subbg = bg_removed_echogram[interval: interval+sample_extent]
        echo = echometrics.Echogram(subbg, depth=depths, index=index)
        metrics['depth_integral']+=np.average(echometrics.depth_integral(echo)),
        metrics['avg_sv']+=np.average(echometrics.sv_avg(echo)),
        metrics['center_of_mass']+=np.average(echometrics.center_of_mass(echo)),
        metrics['inertia']+=np.average(echometrics.inertia(echo)),
        metrics['proportion_occupied']+=np.average(echometrics.proportion_occupied(echo)),
        metrics['aggregation_index']+=np.average(echometrics.aggregation_index(echo)),
        metrics['equivalent_area']+=np.average(echometrics.equivalent_area(echo)),

    #print 'metrics =', metrics
    for k in metrics.keys():
        metrics[k] = np.mean(metrics[k])

    #print 'meetrics =', metrics
    #logger.info(' -- Finished.')
    return metrics


def compress_beams(buf):
    """
    Multibeam data (the M3 in particular) needs to have its beams averaged per ping.
    The m3 also comes in as linear data so it is logified before processign.
    Args:
        buf: the multi-beam ping to compress

    Returns: the framebuffer with the compressed beam image.

    """
    num_beams = buf.num_beams[0]
    num_samples = buf.num_samples[0]
    image = buf.image
    image = np.mean(np.array(image).reshape(num_samples, num_beams).T, axis=0)
    image = 10 * np.log10(image)
    buf.image = image.T
    return buf

def main():

    signal.signal(signal.SIGINT, signal_handler)
    global continue_running
    continue_running = True


    yaml_path = os.path.join(os.getenv("NIMS_HOME", "../build"), "config.yaml")
    try:
        config = ruamel.yaml.load(open(yaml_path, "r"), ruamel.yaml.RoundTripLoader)
    except:
        import yaml
        stream = file(config)
        config = yaml.load(stream)

    if type(config) == StringType:
       config = ast.literal_eval(config)

    logger.info('Configuration Path: ' + yaml_path)

    emmq_name = os.getenv('EMMQ_NAME', '/em_mq') + repr(os.getpid())
    nims_fb_name = '/' + config['FRAMEBUFFER_NAME']
    bin_ping = int(config['ECHOMETRICS']['ping_bin'])
    bin_depth = int(config['ECHOMETRICS']['depth_bin'])
    mode = int(config['SONAR_TYPE'])

    logger.info('Starting realtime_echometrics ...')
    logger.info(' -- echoMetrics MessageQueue: ' + emmq_name)
    logger.info(' -- framebuffer MessageQueue: '+ nims_fb_name)
    logger.info(' -- ping bin size: ' + str(bin_ping))
    logger.info(' -- depth bin size: ' + str(bin_depth))
    logger.info(' -- mode = some mode')
    logger.info('Creating MessageQueue: ' + emmq_name)

    em_request_mq_name = config['ECHOMETRICS']['queue_name']
    # we need a queue for requesting backscatter data and a queue for pushing em data
    em_request_mq = MessageQueue(em_request_mq_name, O_CREAT)
    em_clients_mq = []

    em_mq = MessageQueue(emmq_name, O_CREAT)
    try:
        #logger.info('Connecting to' + nims_fb_name)
        framebuffer_mq = MessageQueue(nims_fb_name, O_RDONLY)
        framebuffer_mq.send(emmq_name)
        logger.info(" -- sent private queue: " + emmq_name)
    except ExistentialError as e:
        logger.error(' -- Could not connect to' + nims_fb_name + e.__repr__())


    # here, unless specified in the log,  both processes are at least ready to communicate
    poller = select.poll()
    poller.register(em_mq.mqd, select.POLLIN)
    poller.register(em_request_mq.mqd, select.POLLIN)
    current_frames = []
    #file_ = open('metrics.csv', 'w+')
    #need_hdr = True
    #import csv
    #csvwriter = csv.writer(file_, delimiter=',')

    logger.info('Starting fetch cycle.')
    while continue_running:
        mq_state = poller.poll()
        for state in mq_state:
            if state[0] == em_mq.mqd:
                frame_buffer = fetch_framebuffer(em_mq)
                if frame_buffer:
                    if mode == 1: # multbeam
                        frame_buffer = compress_beams(frame_buffer)
                    current_frames.append(frame_buffer)
            if state[0] == em_request_mq.mqd: # TODO catch error to drop this person.
                client = accept_new_client(em_request_mq)
                if client:
                    em_clients_mq.append(client)

        if len(current_frames) >= bin_ping:
            echo_metrics = calculate_echometrics(current_frames, bin_depth)
            echo_metrics['pingid'] = frame_buffer.ping_num[0]
            current_frames = []

            #if need_hdr:
                #csvwriter.writerow(echo_metrics.keys())
                #need_hdr == False
            #csvwriter.writerow(echo_metrics.values())
            
            for q in em_clients_mq:
                try:
                    print 'sending echometrics to' + q.name
                    q.send(str(echo_metrics))
                except Exception as e:
                    logger.info('error writing to client q, removing from active clients.')
                    print e.__repr__()
                    em_clients_mq.remove(q)


    do_exit(em_mq)
    return

if __name__ == "__main__":
    main()
