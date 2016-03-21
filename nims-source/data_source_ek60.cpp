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

#include <cstring>  // strncpy()
#include <ostream>

#include <stdint.h> // fixed width integer types
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h> // inet_addr()
#include <unistd.h> // close()
#include <assert.h> // assert()

#include "log.h"

 using namespace std;

// Data types from the Simrad EK60 Scientific Echo Sounder Reference Manual
// File Formats, p. 194
#define SHORT int16_t
#define LONG int32_t

/*
 "The DateTime structure contains a 64-bit integer value stating the number of
 100 nanosecond intervals since January 1,1601. This is the internal ”filetime”
 used by the Windows NT operating system."
 */
 struct DateTime {
    LONG lowWord;
    LONG highWord;
}; // DateTime

struct DatagramHeader
{
    char dg_type[4];
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

struct RequestServerInfo
{
    char header[4];
    
    RequestServerInfo()
    {
        static char const str[] = "RSI\0";
        assert( strlen( str ) < sizeof( header ) );
        strcpy( header, str );
    };
    
};

struct ServerInfo2
{
    char header[4];  //”SI2\0”
    char applicationType[64];
    char applicationName[64];
    char applicationDescription[128];
    long applicationID;    //ID of the current application
    long commandPort;      //Port number to send commands to
    long mode;             //local data source or a remote data source
    char hostName[64]; //the computer the application is running on
    
    ServerInfo2()
    {
        memset(&header, '\0', sizeof(header));
        memset(&applicationType, '\0', sizeof(applicationType));
        memset(&applicationName, '\0', sizeof(applicationName));
        memset(&applicationDescription, '\0', sizeof(applicationDescription));
        applicationID = -1;
        commandPort = -1;
        mode = -1;
        memset(&hostName, '\0', sizeof(hostName));
    };
};

ostream& operator<<(ostream& os, const ServerInfo2& is2)
{
    os << "Application Type: " << string(is2.applicationType) << endl
    << "Application Name: " << string(is2.applicationName) << endl
    << "Application Desc: " << string(is2.applicationDescription) << endl
    << "Application ID: " << is2.applicationID << endl
    << "Command Port: " << is2.commandPort << endl
    << "Mode: " << is2.mode << endl
    << "Hostname: " << string(is2.hostName) << endl;
    return os;
}

struct ConnectRequest
{
    char header[4]; //”CON\0”
    char clientInfo[1024];
    //e.g.”Name:Simrad;Password:\0”
    
    ConnectRequest()
    {
       static char const str1[] = "CON\0";
       assert( strlen( str1 ) < sizeof( header ) );
       strcpy( header, str1 );
       static char const str2[] = "Name:NIMS;Password:\0";
       assert( strlen( str2 ) < sizeof( clientInfo ) );
       strcpy( clientInfo, str2 );
   }
};

struct Response
{
    char header[4];      //”RES\0”
    char request[4];     //”CON\0”
    char msgControl[22]; //”\0”
    char msgResponse[1400]; //Response to connection request
    
    Response()
    {
        memset(&header, '\0', sizeof(header));
        memset(&request, '\0', sizeof(request));
        memset(&msgControl, '\0', sizeof(msgControl));
        memset(&msgResponse, '\0', sizeof(msgResponse));

    };
    
};

ostream& operator<<(ostream& os, const Response& r)
{
    os << "Request: " << string(r.request) << endl
    << "Msg Control: " << string(r.msgControl) << endl
    << "Msg Response: " << string(r.msgResponse) << endl;
}



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
 struct SampleDatagram
 {

	//DatagramHeader	DgHeader; // "RAW0"
	SHORT channel; // Channel number
	SHORT mode; // Datatype
	float transducerDepth; // [m]
	float frequency; // [Hz]
	float transmitPower; // [W]
	float pulseLength; // [s]
	float bandWidth; // [Hz]
	float sampleInterval; // [s]
	float SoundVelocity; // [m/s]
	float absorptionCoefficient; // [dB/m]
	float heave; // [m]
	float roll; // [deg]
	float pitch; // [deg]
	float temperature; // [C]
	short trawlUpperDepthValid; // None=0, expired=1, valid=2
	short trawlOpeningValid; // None=0, expired=1, valid=2
	float trawlUpperDepth; // [m]
	float trawlOpening; // [m]
	LONG offset; // First sample
	LONG count; // Number of samples
	SHORT powAng[0];

}; 


//-----------------------------------------------------------------------------
DataSourceEK60::DataSourceEK60(std::string const &host_addr, uint16_t in_port)
{
    // EK60 uses connectionless UDP so this is just used
    // to verify received datagrams are from the EK60 host
    memset(&host_, '\0', sizeof(struct sockaddr_in));
    host_.sin_family = AF_INET;
    //host_.sin_addr.s_addr = inet_addr(host_addr.c_str());
    host_.sin_addr.s_addr = htonl(INADDR_ANY);
    host_.sin_port = htons(in_port);

    //cmd_port_ = -1;
    
} // DataSourceEK60::DataSourceEK60

//-----------------------------------------------------------------------------
DataSourceEK60::~DataSourceEK60()
{
    // destroy data subscriptions
    // stop "keep alive" thread
    // disconnect from server
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
    if (bind(input_, (struct sockaddr *)&host_, sizeof(host_)) == -1)
    {
        NIMS_LOG_ERROR << "bind() failed";
        return -1;
    }
    /*
    // NOTE:  Have to indicate external connect with "::"
    //        to distguish from member function connect()
    // NOTE:  ::connect() will block for some time interval, waiting for connection
    //        and eventually time out.  POSIX does not specify interval.
    if ( ::connect(input_, (struct sockaddr *) &host_, sizeof(struct sockaddr)) < 0 )
    {
        nims_perror("EK60::connect() failed");
        close (input_);
        input_ = -1;
        return -1;
    }
    // request server information
    /* manual p. 202
       Send the following RequestServerInfo message to the specific IP address of
       the server, or broadcast the message to receive server information from all 
       servers on the LAN.  The message should be send to the UDP port number found 
       in the Local port field on the Server page in the Remoting dialog. in the server 
       application.
    */
       /*
    NIMS_LOG_DEBUG << "sending server request";
    RequestServerInfo rsi;
    if ( send(input_, (const void *)&rsi, sizeof(rsi), MSG_DONTWAIT) < 0 )
    {
        nims_perror("EK60 send request to server failed");
        close (input_);
        input_ = -1;
        return -1;
        
    };

    ssize_t bytes_read = 0;

    ServerInfo2 si2;
    bytes_read = recv(input_, (char *)&si2, sizeof(si2), MSG_WAITALL);
    if ( bytes_read != sizeof(si2) ) {
        nims_perror("incorrect number of bytes from recv()");
        NIMS_LOG_ERROR << "bytes requested = " << sizeof(si2) << ", bytes_read =  " << bytes_read;
        return -1;
    }
    NIMS_LOG_DEBUG << "received server response: " << string(si2.header) << endl << si2;

    // Hopefully this is the same port we used in the constructor.
    if    ( host_.sin_port != htons(si2.commandPort) )
    {
        NIMS_LOG_WARNING << "port mismatch, reconnecting";
    }
    cmd_port_ = si2.commandPort;
    
    // connect to server
    NIMS_LOG_DEBUG << "sending connect request";
    ConnectRequest cr;
    if ( send(input_, (const void *)&cr, sizeof(cr), MSG_DONTWAIT) < 0 )
    {
        nims_perror("EK60 request to connect failed");
        close (input_);
        input_ = -1;
        return -1;
        
    };
    
    Response res;
    bytes_read = recv(input_, (char *)&res, sizeof(res), MSG_WAITALL);
    if ( bytes_read != sizeof(res) ) {
        nims_perror("incorrect number of bytes from recv()");
        NIMS_LOG_ERROR << "bytes requested = " << sizeof(res) << ", bytes_read =  " << bytes_read;
        return -1;
    }
    NIMS_LOG_DEBUG << "received server response: " << string(res.header) << endl << res;
    
    // have to parse the msgResponse field (jeez)
    // ResultCode:S_OK | E_ACCESSDENIED | E_FAIL
    // Parameters:{ClientID:1,AccessLevel:1}\0
    
    // start "keep alive" thread
    // issue data request command
    */
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
    // read from source to get datagram size
    // TODO:  may have to do byte swapping if length values don't seem to match
    // can assume EK60 host is Intel little-endian?
    //struct sockaddr_in serveraddr;
    //int serverlen = sizeof(serveraddr);
    int n;
    LONG datagram_size_in_bytes = 0;
    cout << "attempting to read datagram size..." << endl;
    //while ( serveraddr.sin_addr.s_addr != host_.sin_addr.s_addr )
    //{
        //n = recvfrom(input_, (char*)(&datagram_size_in_bytes), sizeof(datagram_size_in_bytes), 0, &serveraddr, &serverlen);
    n = recvfrom(input_, (char*)(&datagram_size_in_bytes), sizeof(datagram_size_in_bytes), 0, nullptr, nullptr);
    if (n < 0)
    {
        NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Error receiving first length datagram.";
        return -1;    
    }
    //}
    cout << "read " << n << " bytes" << endl;
    cout << "datagram_size_in_bytes = " << datagram_size_in_bytes << endl;

    if ( datagram_size_in_bytes > DATA_BUFFER_SIZE )
    {
        NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Invalid datagram size " << datagram_size_in_bytes;
        return -1;
    }
*/

// read from source to get header and datagram
    cout << "attempting to read datagram..." << endl;
    //serveraddr.sin_addr.s_addr = 0;
    //while ( serveraddr.sin_addr.s_addr != host_.sin_addr.s_addr )
    //{
       // n = recvfrom(input_, buf, datagram_size_in_bytes, 0, &serveraddr, &serverlen);
    sockaddr_in sender;
    socklen_t socklen = sizeof(sender);

    int n = recvfrom(input_, buf_, DATA_BUFFER_SIZE, 0, (struct sockaddr *)&sender, &socklen);
    if (n < 0)
    {
        NIMS_LOG_ERROR << "DataSourceEK60::GetPing() Error reading datagram header.";
        return -1;    
    }
    //}
    cout << "read " << n << " bytes from " << inet_ntoa(sender.sin_addr) << endl;

    NIMS_LOG_DEBUG << "    extracting header";
    cout << string(buf_, 12) << endl;

    strncpy(pframe->header.device, "Simrad EK60 echo sounder",
        sizeof(pframe->header.device));
    
    pframe->header.version  = 0;
    pframe->header.ping_num = 0;

    /*
    "The DateTime structure contains a 64-bit integer value stating the number of
    100 nanosecond intervals since January 1,1601. This is the internal ”filetime”
    used by the Windows NT operating system."
    */
    /*
    // time of ping in seconds since midnight 1-Jan-1970
    double ntSecs = (hdr.date_time.highWord * 2^32 + hdr.date_time.lowWord) / 100 / 1e9;
    double utc = NT_TO_UTC(ntSecs);
    pframe->header.ping_sec      = (uint32_t)floor(utc); 
    pframe->header.ping_millisec = (uint32_t)(utc - pframe->header.ping_sec)*1000;


    pframe->header.soundspeed_mps = dg.SoundVelocity;
    pframe->header.num_samples    = dg.count;
    pframe->header.range_min_m    = 0;
    pframe->header.range_max_m    = 0;
    pframe->header.winstart_sec   = 0; // not used
    pframe->header.winlen_sec     = 0; // not used
    pframe->header.num_beams      = 0; // for EK60, this is num pings
    //for (int m=0; m<header.nNumBeams; ++m)
    //{
     //   if (kMaxBeams == m) break; // max in frame_buffer.h
     //   pframe->header.beam_angles_deg[m] = header.fBeamList[m];
    //}
    pframe->header.freq_hz           = dg.frequency;
    pframe->header.pulselen_microsec = dg.pulseLength * 1e6;
    pframe->header.pulserep_hz       = 0;
    
    NIMS_LOG_DEBUG << "    extracting data";
    // copy data to frame 
    /*
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