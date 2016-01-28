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

#include <stdint.h> // fixed width integer types
#include <cstring>  // strncpy

#include "log.h"

// Data types from the Simrad EK60 Scientific Echo Sounder Reference Manual
// File Formats, p. 194

#define LONG uint32_t

/*
 "The DateTime structure contains a 64-bit integer value stating the number of
 100 nanosecond intervals since January 1,1601. This is the internal ”filetime”
 used by the Windows NT operating system."
 */
struct DateTime {
    LONG LowDateTime;
    LONG HighDateTime;
}; // DateTime

struct DatagramHeader
{
    LONG dg_type;
    DateTime date_time;
}; // DatagramHeader

/*
   "The byte order of the length tags and all binary fields within a datagram is
   always identical to the native byte order of the computer that writes the 
   data file. It is the responsibility of the software that reads the file to 
   perform byte swapping of all multibyte numbers within a datagram if required.
   Byte swapping is required whenever there is an apparent mismatch between the 
   head and the tail length tags. Hence, the two length tags may be used to 
   identify the byte order of the complete datagram."
 */

struct Datagram {
    LONG length1; // datagram length in bytes
    DatagramHeader hdr;
    LONG length2; // datagram length in bytes
}; // Datagram

// See p. 201, Data subscriptions overview
enum ConnectionState {
    NONE,
    INFO_REQUESTED,
    INFO_RECEIVED,
    CONNECTING,
    ALIVE,
    FAILED
}; // ConnectionState

//-----------------------------------------------------------------------------
DataSourceEK60::DataSourceEK60()
{
    
} // DataSourceEK60::DataSourceEK60

//-----------------------------------------------------------------------------
DataSourceEK60::~DataSourceEK60()
{
    // destroy data subscriptions
    // stop "keep alive" thread
    // disconnect from server
    
} // DataSourceEK60::~DataSourceEK60

//-----------------------------------------------------------------------------
int DataSourceEK60::connect()
{
    // request server information
    // connect to server
    // start "keep alive" thread
    // issue data request command
    
    return 0;
    
} // DataSourceEK60::connect


//-----------------------------------------------------------------------------
int DataSourceEK60::GetPing(Frame* pframe)
{
    return 0;
    
    if ( input_ == -1 ) {
        NIMS_LOG_ERROR << ("DataSourceEK60::GetPing() Not connected to source.");
        return -1;
    }

    NIMS_LOG_DEBUG << "    extracting header";
    
    strncpy(pframe->header.device, "Simrad EK60 echo sounder",
            sizeof(pframe->header.device));
    /*
    pframe->header.version = header.dwVersion;
    pframe->header.ping_num = header.dwPingNumber;
    pframe->header.ping_sec = header.dwTimeSec;
    pframe->header.ping_millisec = header.dwTimeMillisec;
    pframe->header.soundspeed_mps = header.fVelocitySound;
    pframe->header.num_samples = header.nNumSamples;
    pframe->header.range_min_m = header.fNearRange;
    pframe->header.range_max_m = header.fFarRange;
    pframe->header.winstart_sec = header.fSWST;
    pframe->header.winlen_sec = header.fSWL;
    pframe->header.num_beams = header.nNumBeams;
    for (int m=0; m<header.nNumBeams; ++m)
    {
        if (kMaxBeams == m) break; // max in frame_buffer.h
        pframe->header.beam_angles_deg[m] = header.fBeamList[m];
    }
    pframe->header.freq_hz = header.dwSonarFreq;
    pframe->header.pulselen_microsec = header.dwPulseLength;
    pframe->header.pulserep_hz = header.fPulseRepFreq;
    
    NIMS_LOG_DEBUG << "    extracting data";
    // copy data to frame as real intensity value
    size_t frame_data_size = sizeof(framedata_t)*(header.nNumSamples)*(header.nNumBeams);
    pframe->malloc_data(frame_data_size);
    if ( pframe->size() != frame_data_size )
        ERROR_MSG_EXIT("Error allocating memory for frame data.");
    framedata_t* fdp = pframe->data_ptr();
    for (int ii = 0; ii < header.nNumBeams; ++ii) // column
    {
        for (int jj = 0; jj < header.nNumSamples; ++jj) // row
        {
            fdp[ii*(header.nNumSamples) + jj] = ABS_IQ( bfData[ii*(header.nNumSamples) + jj] );
            
        }
    }
*/
    
    
} // DataSourceEK60::GetPing