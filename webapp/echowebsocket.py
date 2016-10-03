import datetime
import logging
from random import randint

import tornado.websocket


class EchoWebSocket(tornado.websocket.WebSocketHandler):
    def data_received(self, chunk):
        pass

    def on_message(self, message):
        pass

    # noinspection PyAttributeOutsideInit,PyMethodOverriding
    def initialize(self, register_socket):
        """
        When registering a new web socket client, set up the logging and register for data callbacks.
        Args:
            register_socket: The function to call when the socket opens so that we will receive data callbacks.s
        """
        self.logger = logging.getLogger('echowebsocket')
        self.logger.info('Initializing EchoWebSocket')
        self.register_socket = register_socket

    def open(self):
        """
        When the websocket opens, this function registers it for read / write
        """
        self.logger.info('Register EchoWebSocket')
        self.register_socket(self, True)

    def send_config(self, config):
        """
        Sends the NIMS config data to the web client but with the keyword 'type' appended
        Args:
            config: JSON config data
        """
        config['type'] = 'config'
        self.write_message(config)

    def send_image(self, f):

        """
        Send the frame_buffer recieved by ingester.  The bulk of the data is contained in intensity, otherwise, it is
        a simple dictionary.
        Args:
            f: The frame_buffer
        """
        ts = datetime.datetime.fromtimestamp(f.ping_sec[0]).strftime('%H:%M:%S')
        ts += '.%d' % f.ping_millisec[0]
        a = dict(type='ping',
                 device=f.device,
                 version=f.version[0],
                 pingid=f.ping_num[0],
                 ping_sec=f.ping_sec[0],
                 ping_ms=f.ping_millisec[0],
                 soundspeed=f.soundspeed_mps[0],
                 num_samples=f.num_samples[0],
                 range_min='%.1f' % f.range_min_m[0],
                 range_max='%.lf' % f.range_max_m[0],
                 num_beams=f.num_beams[0],
                 freq=f.freq_hz[0],
                 pulse_len=f.pulselen_microsec[0],
                 pulse_rep=f.pulserep_hz[0],
                 min_angle=f.beam_angles_deg[0],
                 max_angle=f.beam_angles_deg[-1],
                 beam_angles=f.beam_angles_deg[0],
                 sector_size=f.beam_angles_deg[-1] - f.beam_angles_deg[0],
                 intensity=f.image,
                 ts=ts)

        self.write_message(a)

    def send_metrics(self,  metrics):
        """
        Write a JSON metrics structure to the websocket.
        Args:
            metrics: A dictionary of metric values as well as the ping id it ended on.
        """
        a = dict(type='metrics',
                 pingid=metrics['pingid'] ,
                 center_of_mass=metrics['center_of_mass'],
                 sv_area=metrics['avg_sv'],
                 sv_volume=metrics['depth_integral'],
                 inertia=metrics['inertia'],
                 proportion_occupied=metrics['proportion_occupied'],
                 aggregation_index=metrics['aggregation_index'],
                 equivalent_area=metrics['equivalent_area'])

        self.write_message(a)

    def send_tracks(self, tracks):
        """
        Write a JSON tracks structure containing the number of tracks as well as a list of targets
        Args:
            tracks: The tracks structure provided by the tracker module
        """
        a = dict(type='tracks',
                 num_tracks = tracks.num_tracks,
                 tracks=tracks.targets)
        self.write_message(a)

    def on_close(self):
        """
        Remove this client from the registry so we don't try to send data to it.
        """
        self.register_socket(self, False)
