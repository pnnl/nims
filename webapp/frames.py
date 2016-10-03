#!/usr/bin/python

# Python modules
from struct import *
import sys

# 3rd party modules


class frame_buffer:

    def __init__(self, buff):
        """
        Reads a byte array and converts it to the frame buffer specified by ingester
        Args:
            buff: a byte array
        """
        self.valid = self.parse_buffer(buff)

    def unpacker(self, fmt, buff):
        """
        Healper function for unpacking the byte array.
        Args:
            fmt: a format string
            buff: the buffer containing the data to unpack

        Returns: Unpacked data specified in the format string and the remainder of the buffer.
        """
        size = calcsize(fmt)
        return unpack(fmt, buff[:size]), buff[size:]

    def parse_buffer(self, buff):
        """
        The function that actually unpacks the fields.
        Args:
            buff: the byte array, passed in from __init__()

        Returns: boolean representing a valid packet or invalid packet
        """
        try:
            self.device, buff = self.unpacker('c' * 64, buff)
            self.device = "".join(self.device)
            self.version, buff = self.unpacker('I', buff)
            self.ping_num, buff = self.unpacker('I', buff)
            self.ping_sec, buff = self.unpacker('I', buff)
            self.ping_millisec, buff = self.unpacker('I', buff)
            self.soundspeed_mps, buff = self.unpacker('f', buff)
            self.num_samples, buff = self.unpacker('I', buff)
            self.range_min_m, buff = self.unpacker('f', buff)
            self.range_max_m, buff = self.unpacker('f', buff)
            self.winstart_sec, buff = self.unpacker('f', buff)
            self.winlen_sec, buff = self.unpacker('f', buff)
            self.num_beams, buff = self.unpacker('I', buff)
            self.beam_angles_deg, buff = self.unpacker('f' * 512, buff)
            self.freq_hz, buff = self.unpacker('I', buff)
            self.pulselen_microsec, buff = self.unpacker('I', buff)
            self.pulserep_hz, buff = self.unpacker('f', buff)
            #print self.pulserep_hz
            self.data_len, buff = self.unpacker('Q', buff)
            tot_samples = self.num_samples[0] * self.num_beams[0]
            self.image, buff = self.unpacker('f' * tot_samples, buff)

            return True
        except:
            print "Failed to parse buff:", sys.exc_info()
            return False


    def print_header(self):
        """
        Helper function to print the frame fields.
        """
        print ""
        print "         version:", self.version[0]
        print "     ping number:", self.ping_num[0]
        print "        ping sec:", self.ping_sec[0]
        print "         ping ms:", self.ping_millisec[0]
        print "soundspeed (mps):", self.soundspeed_mps[0]
        print "     num_samples:", self.num_samples[0]
        print "   range min (m):", self.range_min_m[0]
        print "   range max (m):", self.range_max_m[0]
        print "    window start:", self.winstart_sec[0]
        print "   window length:",  self.winlen_sec[0]
        print "       num beams:", self.num_beams[0]
        print "       freq (hz):", self.freq_hz[0]
        print "  pulse len (ms):", self.pulselen_microsec[0]
        print "  pulse rep (hz):", self.pulserep_hz[0]
        print "        data len:", self.data_len


class frame_message:
    def __init__(self, message):
        """
        Frame message identifies a shared memory location and size for the new frame.
        Args:
            message: buffer containing the qq64s set of bytes
        """
        self.valid = self.parse_message(message)

    def unpacker(self, fmt, buff):
        """
        Healper function for unpacking the byte array.
        Args:
            fmt: a format string
            buff: the buffer containing the data to unpack

        Returns: Unpacked data specified in the format string and the remainder of the buffer.
        """
        s = calcsize(fmt)
        return unpack(fmt, buff[:s]), buff[s:]

    def parse_message(self, message):
        """
        Parses the byte array into it's constituent fields
        Args:
            message: byte array

        Returns: Boolean representing valid or not valid.
        """
        try:
            self.frame_number, message = self.unpacker('q', message)
            self.frame_length, message = self.unpacker('Q', message)
            self.shm_location, message = self.unpacker('s'*64, message)

            self.frame_number = self.frame_number[0]
            self.frame_length = self.frame_length[0]
            shm = ""
            for x in self.shm_location:
                if x == '\x00':
                    break
                shm = shm + x
            self.shm_location = shm
            return True
        except:
            print "Failed to parse frame buffer:", sys.exc_info()
            return False

    def print_message(self):
        return

#----------------------------------------------------------------------------


class track_message:


    def __init__(self, message):
        """
        Track message contains the tracks ping by ping.  it works different than the other message queues as there is
        only one of them for some reason.
        Args:
            message: byte array to parse
        """
        self.targets = []
        self.valid = self.parse_message(message)
        self.max_detections = 100


    def unpacker(self, fmt, buff):
        """
        Healper function for unpacking the byte array.
        Args:
            fmt: a format string
            buff: the buffer containing the data to unpack

        Returns: Unpacked data specified in the format string and the remainder of the buffer.
        """
        s = calcsize(fmt)
        return unpack(fmt, buff[:s])[0], buff[s:]

    def parse_message(self, message):
        """
        parses the track message into its consituent parts.
        Args:
            message:  byte array of format: III[SHffffffffffHfffffffff]

        Returns: boolean represent valid or invalid

        """
        try:
            # first 3x32 bit ints are the track header
            self.frame_num, message = self.unpacker('I', message)
            self.ping_num , message = self.unpacker('I', message)
            self.ping_time, message = self.unpacker('d', message)
            self.num_tracks, message = self.unpacker('I', message)

            print 'Frame:', self.frame_num
            print ' - ping:', self.ping_num
            print ' - ping time:', self.ping_time
            print ' - num tracks:', self.num_tracks

            for i in range(self.num_tracks):
                target = {}
                target['id'],message = self.unpacker('H', message)
                target['size_sq_m'], message = self.unpacker('f', message)
                target['speed_mps'], message = self.unpacker('f', message)
                target['target_strength'], message = self.unpacker('f', message)
                target['min_range_m'], message = self.unpacker('f', message)
                target['max_range_m'], message = self.unpacker('f', message)
                target['min_bearing_deg'], message = self.unpacker('f', message)
                target['max_bearing_deg'], message = self.unpacker('f', message)
                target['min_elevation_deg'], message = self.unpacker('f', message)
                target['max_elevation_deg'], message = self.unpacker('f', message)
                target['first_detect'], message = self.unpacker('f', message) #time
                target['pings_visible'], message = self.unpacker('H', message)
                target['last_pos_range'], message = self.unpacker('f', message)
                target['last_pos_bearing'], message = self.unpacker('f', message)
                target['last_pos_elevation'], message = self.unpacker('f', message)
                target['last_vel_range'], message = self.unpacker('f', message)
                target['last_vel_bearing'], message = self.unpacker('f', message)
                target['last_vel_elevation'], message = self.unpacker('f', message)
                target['width'], message = self.unpacker('f', message)
                target['length'], message = self.unpacker('f', message)
                target['height'], message = self.unpacker('f', message)

                self.targets.append(target)

            return True
        except:
            print "Failed to parse track buffer:", sys.exc_info()
            return False

    def print_message(self):
        return