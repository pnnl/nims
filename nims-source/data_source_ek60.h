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

/*-----------------------------------------------------------------------------
Class for Simrad EK60 Echosounder.
*/

// SampleDatagram size is 
// header size + power samples + angle samples
// = 88 + LONG_MAX * sizeof(SHORT) * 2
const long DATA_BUFFER_SIZE  = 1024 * 1024; // call it 1 Mb

class DataSourceEK60 : public DataSource {
 public:
    DataSourceEK60(std::string const &host_addr, uint16_t in_port);  // Constructor
    ~DataSourceEK60(); // Destructor
  
  int connect();   // connect to data source
  bool is_good() { return (input_ != -1); };  // check if source is in a good state
    bool more_data() { return true; };  // TODO: check for not "end of file" condition
  int GetPing(Frame* pdata);     // get the next ping from the source
  //virtual size_t ReadPings(Frame* pdata, const size_t& num_pings) =0; // read consecutive pings
    
private:
    struct sockaddr_in host_;
    long cmd_port_;
    char buf_[DATA_BUFFER_SIZE];
    
    
}; // DataSourceEK60
   

#endif // __NIMS_DATA_SOURCE_EK60_H__
