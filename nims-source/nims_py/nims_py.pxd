
cdef extern from "tracks_message.h":
    
    DEF _MAX_ACTIVE_TRACKS = 50
    
    struct Track:
        unsigned short id       # unique track identifier
        float size_sq_m         # avg size in square meters
        float speed_mps         # avg speed in meters per sec.
        float target_strength   # avg intensity
        
        float min_range_m       # bounding box of the entire track
        float max_range_m 
        float min_bearing_deg 
        float max_bearing_deg 
        float min_elevation_deg 
        float max_elevation_deg
        
        float first_detect      # time when track started
        unsigned short pings_visible # number of pings the target was visible
        
        float last_pos_range    # last observed position
        float last_pos_bearing 
        float last_pos_elevation
        
        float last_vel_range    # last observed velocity
        float last_vel_bearing
        float last_vel_elevation
        
        float width             # last observed size
        float length
        float height
        
    struct TracksMessage:
        unsigned int frame_num      # NIMS internal ping number from FrameHeader
        unsigned int ping_num_sonar # sonar ping number
        unsigned int num_tracks     # number of tracks
        Track tracks[_MAX_ACTIVE_TRACKS]

cdef extern from "nims_py_ext.h":
  
    TracksMessage get_next_track(int mq)
    TracksMessage get_next_track_timed(int mq, int secs, int ns)
    int get_next_message(int mq, void *msg, size_t msgsize)
    int create_message_queue(size_t message_size, char *name)
    int get_next_message_timed(int mq, void *msg, size_t msgsize, int secs, int ns)
    int nims_checkin()
    size_t sizeof_tracks_message()
    size_t sizeof_track()

