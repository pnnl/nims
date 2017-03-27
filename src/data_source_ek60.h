/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_ek60.h
 *  
 *  Created by Shari Matzner on 01/19/2016.
 *  Copyright 2016 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_DATA_SOURCE_EK60_H__
#define __NIMS_DATA_SOURCE_EK60_H__


#include "data_source.h"

#include <string>
#include <netinet/in.h> // struct sockaddr_in
#include "boost/date_time/posix_time/posix_time.hpp" //no i/o just types
/*-----------------------------------------------------------------------------
Class for Simrad EK60 Echosounder.
*/

// SampleDatagram size is 
// header size + power samples + angle samples
// = 88 + LONG_MAX * sizeof(SHORT) * 2
const long DATA_BUFFER_SIZE  = 65507; // practical limit for UDP packets

// TODO: Make a pure virtual DataSourceParams class/struct in data_source.h
struct EK60Params {
  int port;
  float along_beamwidth;
  float athwart_beamwidth;
  float along_sensitivity;
  float athwart_sensitivity;
  float along_offset;
  float athwart_offset;
  float ping_rate_hz;
};

class DataSourceEK60 : public DataSource {
 public:
    DataSourceEK60(std::string const &host_addr, EK60Params const &params);  // Constructor
    ~DataSourceEK60(); // Destructor
  
  int connect();   // connect to data source
  bool is_good() { return (input_ != -1); };  // check if source is in a good state
  bool more_data() { return true; };  // TODO: check for not "end of file" condition
  int GetPing(Frame* pdata);     // get the next ping from the source
  //virtual size_t ReadPings(Frame* pdata, const size_t& num_pings) =0; // read consecutive pings
    
private:
    size_t get_datagram(std::string dtype_str, std::string& dtime_str, char* &buf_data);
    
    struct sockaddr_in host_;
    char buf_[DATA_BUFFER_SIZE];
    // constant part of frame header
    FrameHeader header_;
    // used to calc utc seconds time of ping
    boost::posix_time::ptime t_epoch_;
    // scale and offset for converting electrical angles to deg.
    float A_along_, B_along_, A_athwart_, B_athwart_;
    long pcount_;
    EK60Params params_;
}; // DataSourceEK60
   

#endif // __NIMS_DATA_SOURCE_EK60_H__
