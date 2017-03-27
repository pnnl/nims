/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_ek60.cpp
 *
 *  Created by Shari Matzner on 01/19/2016.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#include "data_source_ek60.h"

#include <cstring>  // strncpy(), memcpy()
#include <cstdlib> // malloc()
#include <ostream>

#include <stdint.h> // fixed width integer types
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // inet_addr()
#include <unistd.h> // close()
#include <assert.h> // assert()
//#include <ctime> // time functions
//#include <boost/date_time/posix_time/posix_time_io.hpp>
 
#include "log.h"

namespace dt = boost::date_time;
namespace pt = boost::posix_time;
namespace greg = boost::gregorian;
using namespace std;

// Data types from the Simrad EK60 Scientific Echo Sounder Reference Manual
// File Formats, p. 194
#define SHORT int16_t
#define LONG int32_t
#define BYTE int8_t

// (10*log10(2)/256
const double POWER_SCALE = 0.011758984205624;
// 180 degrees /128 electrical units
const double ANGLE_SCALE = 1.40625; 
// 8-bit resolution
const int NUM_VIRTUAL_BEAMS = 256;
/*
 Sample datagram
 The sample datagram contains sample data from just one transducer channel.
 It can contain power sample data (Mode = 0), or it can contain both power and angle sample data (Mode = 1).
 
 The sample data datagram can contain more than 32 768 sample points.
 Short Power - The power data contained in the sample datagram is compressed.
 In order to restore the correct value(s), you must decompress the value according to the equation below.
 
 y=x*10*log(2)/256;
 
 x = power value derived from the datagram
 y = converted value (in dB)
 
 Short Angle - The fore-and-aft (alongship) and athwartship electrical angles are output as one 16-bit word.
 The alongship angle is the most significant byte while the athwartship angle is the least significant byte.
 Angle data is expressed in 2's complement format, and the resolution is given in steps of 180/128 electrical
 degrees per unit. Positive numbers denotes the fore and starboard directions.
  */
 
//-----------------------------------------------------------------------------
DataSourceEK60::DataSourceEK60(std::string const &host_addr, EK60Params const &params)
{
    // EK60 uses connectionless UDP so this is just used
    // to verify received datagrams are from the EK60 host
    memset(&host_, '\0', sizeof(struct sockaddr_in));
    host_.sin_family = AF_INET;
    //host_.sin_addr.s_addr = inet_addr(host_addr.c_str());
    host_.sin_addr.s_addr = htonl(INADDR_ANY);
    host_.sin_port = htons(params.port);

    // initialize time base
    t_epoch_ = pt::ptime(greg::date(1970,1,1),pt::time_duration(0,0,0));
    pcount_ = 0;

    // initialize angle scaling
    // transducer-dependent, 
    // for three examples I have, the sensitivity is the same for both directions
    // sensitivity is a user-settable parameter, 0 to 100
    // offset is user-settable, -10 to 10
    A_along_ = ANGLE_SCALE/params_.along_sensitivity;
    B_along_ = params.along_offset;
    A_athwart_ = ANGLE_SCALE/params .athwart_sensitivity;
    B_athwart_ = params.athwart_offset;

    params_ = params;
    
    // 
} // DataSourceEK60::DataSourceEK60

//-----------------------------------------------------------------------------
DataSourceEK60::~DataSourceEK60()
{
    close(input_);
} // DataSourceEK60::~DataSourceEK60

//-----------------------------------------------------------------------------
int DataSourceEK60::connect()
{
    // EK60 uses connectionless UDP, so we just create
    // the socket here but no need to connect
    input_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (input_ == -1)
    {
        NIMS_LOG_ERROR << "socket() failed";
        return -1;
    }
    // set this so bind doesn't fail because address is in use from last attempt
    setsockopt(input_,SOL_SOCKET,SO_REUSEADDR, nullptr, 0);
    // bind to the port to receive incoming messages
    if (bind(input_, (struct sockaddr *)&host_, sizeof(host_)) == -1)
    {
        NIMS_LOG_ERROR << "bind() failed";
        close(input_);
        input_ = -1; // reset this so we'll try again
        return -1;
    }
/*
// NOTE: the parameter datagram is only sent when params change
    // get parameter datagrams
        sockaddr_in sender;
        socklen_t socklen = sizeof(sender);
        int n = recvfrom(input_, buf_, DATA_BUFFER_SIZE, 0, (struct sockaddr *)&sender, &socklen);
        if (n < 0)
        {
            // TODO: maybe try again before returning.
            NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Error reading datagram.";
            return -1;    
        }
        NIMS_LOG_DEBUG << "read " << n << " bytes from " << inet_ntoa(sender.sin_addr) << ": " << string(buf_, 12)<< endl;
*/
    strncpy(header_.device, "Simrad EK60 echo sounder", sizeof(header_.device));
    
    header_.version  = 0;
    header_.ping_num = 0;

    // PE,22114619,/UTILITY MENU/Sound Velocity=1489 m
    header_.soundspeed_mps = 0.0;

    // PE,22114619,/TRANSCEIVER MENU/Transceiver-1 Menu/Sample Interval=0.0953 m
    header_.num_samples    = 0;
    header_.range_min_m    = 0.0;
    header_.range_max_m    = 1000.0;

    header_.winstart_sec   = 0; // not used
    header_.winlen_sec     = 0; // not used
 
    // create virtual beams to represent angles within a single beam
    header_.num_beams      = NUM_VIRTUAL_BEAMS; 
   for (int m=0; m<header_.num_beams; ++m)
    {
        header_.beam_angles_deg[m] = ANGLE_SCALE/params_.along_sensitivity*(-128.0 + m) 
           + params_.along_offset;
    }


    header_.freq_hz           = 0;

    // PE,22114619,/TRANSCEIVER MENU/Transceiver-1 Menu/Pulse Length=0.512 ms
    header_.pulselen_microsec = 0 * 1e6;
    header_.pulserep_hz       = params_.ping_rate_hz;

    return 0;
    
} // DataSourceEK60::connect

// NT time starts Jan 1, 1601
// UTC time starts Jan 1, 1970
// 11644473600 seconds from Jan 1, 1601 to Jan 1, 1970
//const double SECS_PER_DAY = 86400.0; // 24*60*60
// days, including leap years but no leap year 1700, 1800 or 1900
// (1970-1601) * 365 + (1970-1601)/4 - 3 = 134685 + 89 = 134774
//const double DAYS_1601_TO_1970 =  134774.0; 
//const double SECS_PER_NANO = 1.0e-9;
//#define NT_TO_UTC(nt) { nt/100 * SECS_PER_NANO - (DAYS_1601_TO_1970 * SECS_PER_DAY) }



//-----------------------------------------------------------------------------
int DataSourceEK60::GetPing(Frame* pframe)
{

    if ( input_ == -1 ) {
        NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Not connected to source.";
        return -1;
    }


/*
If power and angle are both selected in the EK500 Datagram dialog, first angle 
datagrams are sent then power.  The datagrams will have the same timestamp.  
All the datagrams for each type will be sent consecutively followed by the next
type.  This is true if there are multiple transducers.
*/
    char* buf_power = nullptr;
    char* buf_angle = nullptr;

string ptime_str;
size_t data_bytes = 0;
data_bytes = get_datagram("B1", ptime_str, buf_angle);
if (!(data_bytes>0)) {
    NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Error getting angle data.";
    return -1;
}
data_bytes = get_datagram("W1", ptime_str, buf_power);
if (!(data_bytes>0)) {
    NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Error getting power data.";
    return -1;
}

    //NIMS_LOG_DEBUG << "    constructing header";
    pframe->header = header_; // copy constant part of header
    
    // TODO:  need to assign a ping number since there is none in datagram
    pframe->header.ping_num = ++pcount_;

    // time of ping in seconds since midnight 1-Jan-1970
    // NOTE:  Assumes sonar host computer is set to same timezone
    // as NIMS computer.  Assumes current time is always later than
    // ping time, since ping happened in the past.
    pt::ptime tnow(pt::second_clock::local_time());
    // ISO time sting: YYYYMMDDTHHMMSS,fffffffff
    string tnow_str = pt::to_iso_string(tnow);
    //NIMS_LOG_DEBUG << "time now is " << tnow_str;
    string tping_str = string(tnow_str,0,9) + string(buf_+3,6) + ',' + string(buf_+9,2) + "0000000";
    //NIMS_LOG_DEBUG << "ping time is " << tping_str;
    pt::ptime tping = pt::from_iso_string(tping_str);
    if (tping > tnow) tping = tping - greg::days(1);
    pt::time_duration diff = tping - t_epoch_;
    pframe->header.ping_sec      = (uint32_t)(diff.total_seconds()); 
    pframe->header.ping_millisec = (uint32_t)stoi(string(buf_+9,2),nullptr,10)*10;

    pframe->header.num_samples = data_bytes/sizeof(SHORT);
    
    // copy data to frame 
    //NIMS_LOG_DEBUG << "    allocating frame memory";
    size_t frame_data_size = sizeof(framedata_t)*(pframe->header.num_samples)*(pframe->header.num_beams);
    pframe->malloc_data(frame_data_size);
    if ( pframe->size() != frame_data_size )
    {
        NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Error allocating memory for frame data.";
        free(buf_angle);
        free(buf_power);
        return -1;
    }
    //NIMS_LOG_DEBUG << "    extracting data, " << pframe->header.num_samples << " samples";
    framedata_t* fdp = pframe->data_ptr();
    BYTE b;
    SHORT raw;
    for (int r = 0; r < pframe->header.num_samples; ++r) // row
    { 
        memcpy(&b, buf_angle+(2*r), 1);      
        memcpy(&raw, buf_power+(2*r), 2);
        //NIMS_LOG_DEBUG << "r = " << r << ", b = " << +b << ", power = " << raw;
        fdp[(b+128)*(pframe->header.num_samples) + r] = (framedata_t)(raw*POWER_SCALE);
    }

    //NIMS_LOG_DEBUG << " done getting ping data.";
    free(buf_angle);
    free(buf_power);
     return 0;

    
} // DataSourceEK60::GetPing


size_t DataSourceEK60::get_datagram(string dtype_str, string& dtime_str, char* &buf_data)
{
     
    BYTE pkt_num;
    SHORT firstbyte;
    SHORT bytes_in_pkt;
    size_t bytes_so_far = 0; // data bytes copied to buffer so far

// loop here until we get a full datagram of the desired type, which may be
// split over multiple packets
bool got_it = false;
while (!got_it)
{
    //NIMS_LOG_DEBUG << "getting datagram...";
    if (bytes_so_far > 0) free(buf_data);
    buf_data = nullptr;
    bytes_so_far = 0;
    int last_pkt = 0; // sequence number of last packet received

    BYTE eod = 0x00; // end of data flag
    while (eod == 0x00) // more packets, 0x80 indicates last packet
    {
        sockaddr_in sender;
        socklen_t socklen = sizeof(sender);
        int n = recvfrom(input_, buf_, DATA_BUFFER_SIZE, 0, (struct sockaddr *)&sender, &socklen);
        if (n < 0)
        {
            // TODO: maybe try again before returning.
            NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Error reading datagram.";
            return -1;    
        }
       // NIMS_LOG_DEBUG << "read " << n << " bytes from " << inet_ntoa(sender.sin_addr) << ": " << string(buf_, 12)<< endl;

        // check type
        string dtype = string(buf_,2);
        if (dtype != dtype_str) 
        {
            NIMS_LOG_WARNING << "ignoring invalid datagram type: " << dtype;
            break; // start over looking for datagram
        }

        // parse datagram
        // TODO: make offsets #defines for maintainability
        string pingtimestr = string(buf_+3,8);
        memcpy(&pkt_num, buf_+12, sizeof(pkt_num));
        memcpy(&eod, buf_+13, sizeof(eod));
        memcpy(&firstbyte, buf_+14, sizeof(firstbyte));
        memcpy(&bytes_in_pkt, buf_+16, sizeof(bytes_in_pkt));
 
 NIMS_LOG_DEBUG << "pkt num = " << (short)pkt_num << ", firstbyte = " << firstbyte << ", bytes_in_pkt = " << bytes_in_pkt;

        // check for lost packets
        if ((int)pkt_num > last_pkt + 1)
        {   
            //last_pkt = -1;
            NIMS_LOG_WARNING << "dropped packet, waiting for next ping";
            break; // start over looking for angle datagram
        }
        // TODO: could also check for same timestring
        last_pkt = pkt_num;
        char* new_buf = (char *)realloc(buf_data, bytes_so_far + bytes_in_pkt);
        if (new_buf == nullptr)
        {
            NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Error allocating memory";
            return -1;
        }
        buf_data = new_buf;
        memcpy(buf_data+bytes_so_far,buf_+18,bytes_in_pkt);
        bytes_so_far += bytes_in_pkt;

    } // while (eod == 0x00) 
    
    //NIMS_LOG_DEBUG << "eod: " << eod;
    if (eod == -128) got_it = true;
} // while (!got_it)
//NIMS_LOG_DEBUG << "got datagram: " << pkt_num +1 << " packets, " << bytes_so_far << " total bytes";

return bytes_so_far;

} // DataSourceEK60::get_datagram