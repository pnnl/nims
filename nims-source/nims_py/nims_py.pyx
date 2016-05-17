
# import nims_py.pxd
from nims_py cimport get_next_message, get_next_message_timed, create_message_queue
from nims_py cimport Track, TracksMessage, sizeof_tracks_message, sizeof_track
import math
import sys

cdef class PyTrack:
    
    cdef Track _trk
    _allkeys = ('id', 'size_sq_m', 'speed_mps', 'target_strength', \
    'min_range_m', 'max_range_m', 'min_bearing_deg', 'max_bearing_deg', \
    'min_elevation_deg', 'max_elevation_deg', 'first_detect', 'pings_visible',\
    'last_pos_range', 'last_pos_bearing', 'last_pos_elevation', \
    'last_vel_range', 'last_vel_bearing', 'last_vel_elevation', \
    'width', 'length', 'height')
    
    # could initialize all members, but...
    def __cinit__(self):
        self._trk.id = 0
        
    property id:
        def __get__(self):
            return self._trk.id

    property size_sq_m:
        def __get__(self):
            return self._trk.size_sq_m

    property speed_mps:
        def __get__(self):
            return self._trk.speed_mps

    property target_strength:
        def __get__(self):
            return self._trk.target_strength

    property min_range_m:
        def __get__(self):
            return self._trk.min_range_m

    property max_range_m:
        def __get__(self):
            return self._trk.max_range_m

    property min_bearing_deg:
        def __get__(self):
            return self._trk.min_bearing_deg

    property max_bearing_deg:
        def __get__(self):
            return self._trk.max_bearing_deg

    property min_elevation_deg:
        def __get__(self):
            return self._trk.min_elevation_deg

    property max_elevation_deg:
        def __get__(self):
            return self._trk.max_elevation_deg

    property first_detect:
        def __get__(self):
            return self._trk.first_detect

    property pings_visible:
        def __get__(self):
            return self._trk.pings_visible

    property last_pos_range:
        def __get__(self):
            return self._trk.last_pos_range

    property last_pos_bearing:
        def __get__(self):
            return self._trk.last_pos_bearing

    property last_pos_elevation:
        def __get__(self):
            return self._trk.last_pos_elevation

    property last_vel_range:
        def __get__(self):
            return self._trk.last_vel_range

    property last_vel_bearing:
        def __get__(self):
            return self._trk.last_vel_bearing

    property last_vel_elevation:
        def __get__(self):
            return self._trk.last_vel_elevation

    property width:
        def __get__(self):
            return self._trk.width

    property length:
        def __get__(self):
            return self._trk.length

    property height:
        def __get__(self):
            return self._trk.height
                
    def __repr__(self):
        return repr(self.dict_value())
        
    def __str__(self):
        my_class = self.__class__
        s = "<%s.%s (0x%x)>" % (my_class.__module__, my_class.__name__, id(self))
        s += " = {\n"
        kl = [len(k) for k in self._allkeys]
        klmax = max(kl)
        for k in self._allkeys:
            s += "%s = %s\n" % (k.rjust(klmax + 4), getattr(self, k))
        s += "}"
        return s

    def dict_value(self, ensure_finite=False):
        ret = dict()
        for k in self._allkeys:
            v = getattr(self, k)
            
            # Tracker is stuffing nan/inf/-inf values in, and
            # the python JSON conversion changes them to a
            # string representation that is not valid.
            #
            # Cleaner to change here rather than force the clients
            # to iterate the nested dictionaries.
            def _is_finite_value(v):
                try:
                    if math.isnan(v):
                        return False
                    if math.isinf(v):
                        return False
                except Exception, e:
                    pass
                return True
                
            if ensure_finite and _is_finite_value(v) == False:
                v = None
            ret[k] = v
            
        return ret

cdef class PyTracksMessage:
    
    cdef TracksMessage _msg
    
    def __cinit__(self):
        self._msg.frame_num = 0
        self._msg.ping_num_sonar = 0
        self._msg.ping_time = 0.0
        self._msg.num_tracks = 0
        
    property frame_num:
        def __get__(self):
            return self._msg.frame_num

    property ping_num_sonar:
        def __get__(self):
            return self._msg.ping_num_sonar

    property num_tracks:
        def __get__(self):
            return self._msg.num_tracks
            
    property ping_time:
        def __get__(self):
            return self._msg.ping_time
            
    def tracks(self):
        # returns PyTrack objects
        all_tracks = []
    
        for idx in xrange(self.num_tracks):
        
            ptrk = PyTrack()
            # !!! fixme wtf
            ptrk._trk = self._msg.tracks[idx]    
            all_tracks.append(ptrk)
        return all_tracks
            
    def __len__(self):
        return self.num_tracks
        
    def __getitem__(self, key):
        assert isinstance(key, int), "Only integer indexing is supported"
        assert key >= 0 and key < self.num_tracks, "Index out of range"
        obj = self._msg.tracks[key]
        return obj
        
    def __iter__(self):
        for pytrk in self.tracks():
            yield pytrk

    def __str__(self):
        return str(self.dict_value())    
        
    def dict_value(self, ensure_finite=False):
        ret = dict()
        ret["frame_num"] = self.frame_num
        ret["ping_num_sonar"] = self.ping_num_sonar
        ret["ping_time"] = self.ping_time
        ret["num_tracks"] = self.num_tracks
        ret["tracks"] = [ptrk.dict_value(ensure_finite=ensure_finite) for ptrk in self.tracks()]
        return ret

cpdef int nims_checkin_py():
    ret = nims_checkin()
    return ret

cpdef int open_tracks_message_queue_py(char *queue_name):

    msg_size = sizeof_tracks_message()
    mq = create_message_queue(msg_size, queue_name)
    return mq

cpdef object get_next_tracks_message_py(int mq):

    cdef TracksMessage msg
    msg_size = sizeof_tracks_message()

    sz = get_next_message(mq, &msg, msg_size)
    pmsg = PyTracksMessage()
    if sz > 0:
        pmsg._msg = msg
    else:
        sys.stderr.write("get_next_tracks_message_py: error getting next message\n")
    
    return pmsg
            
cpdef object get_next_tracks_message_timed_py(int mq, float timeout_secs):

    cdef TracksMessage msg
    msg_size = sizeof_tracks_message()
    secs = int(timeout_secs)
    nsecs = int((timeout_secs - secs) * 1e9)

    sz = get_next_message_timed(mq, &msg, msg_size, secs, nsecs)
    pmsg = PyTracksMessage()
    if sz > 0:
        pmsg._msg = msg
    elif -1 == sz:
        sys.stderr.write("get_next_tracks_message_timed_py: error getting next message\n")
    else:
        # timeout is not an error
        pass
    
    return pmsg
