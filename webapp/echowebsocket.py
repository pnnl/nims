import tornado.websocket
import math
from random import randint
import datetime
class EchoWebSocket(tornado.websocket.WebSocketHandler):       

    def initialize(self, register_socket):
        print "EchoWebSocket::__init__()"
        self.register_socket = register_socket

    def open(self):
        print "EchoWebSocket::Open()"
        self.register_socket(self, True)
        self.com = 50
        self.sv = 50
        self.sa = 50

    def send_data(self, f, metrics, tracks):
   #     print "EchoWebSocket::send_data()"

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

        self.write_message(a)



    def on_close(self):
        self.register_socket(self, False)

