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
        self.logger = logging.getLogger('echowebsocket')
        self.logger.info('Initializing EchoWebSocket')
        self.register_socket = None
        # test code
        # create a default track that can be perturbed per ping
        self.center_range = 300
        self.center_beam = 40
        self.is_new_track = True
        self.tracks = []
        track = (25, self.center_range, self.center_beam, self.is_new_track)
        self.tracks.append(track)
        self.register_socket = register_socket

    def open(self):
        self.logger.info('Register EchoWebSocket')
        self.register_socket(self, True)

    def send_data(self, f, metrics, tracks):
        a = dict(device=f.device,
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
                 max_angle=f.beam_angles_deg[f.num_beams[0] - 1],
                 sector_size=f.beam_angles_deg[f.num_beams[0] - 1] - f.beam_angles_deg[0],
                 intensity=f.image,
                 center_of_mass=metrics['center_of_mass'],
                 sv_area=metrics['avg_sv'],
                 sv_volume=metrics['depth_integral'],
                 inertia=metrics['inertia'],
                 proportion_occupied=metrics['proportion_occupied'],
                 aggregation_index=metrics['aggregation_index'],
                 equivalent_area=metrics['equivalent_area'])

        # if tracks.num_detections[0] > 0:
        #    print 'Sending track data'
        #    a['tracks'] = tracks.

        for i in range(len(self.tracks)):
            self.center_range += randint(-2, 2)
            self.center_beam += randint(-1, 1)
            self.is_new_track = False
            self.tracks[i] = (25, self.center_range, self.center_beam, self.is_new_track)

        a['tracks'] = self.tracks
        ts = datetime.datetime.fromtimestamp(f.ping_sec[0]).strftime('%H:%M:%S')
        ts += '.%d' % f.ping_millisec[0]
        a['ts'] = ts

        self.write_message(a)

    def on_close(self):
        self.register_socket(self, False)
