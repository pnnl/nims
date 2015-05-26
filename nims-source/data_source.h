/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source.h
 *  
 *  Created by Shari Matzner on 05/15/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_DATA_SOURCE_H__
#define __NIMS_DATA_SOURCE_H__

#include <fstream>

#include "frame_buffer.h"
/*-----------------------------------------------------------------------------
Base class for sonar data sources.  Classes for specific devices will be
derived from this class.  The unit of data is a ping, which is both the ping
attributes and the received echo.
*/

class DataSource {
 public:
  DataSource(const std::string& path); // Constructor
  ~DataSource();                       // Destructor
  
  bool is_open()   { return input_.is_open(); }; // check if source was opened successfully
  bool more_data() { return !input_.eof(); };    // check for not "end of file" condition
  
  virtual void GetPing(Frame* pdata) =0;     // get the next ping from the source
  //virtual size_t ReadPings(Frame* pdata, const size_t& num_pings) =0; // read consecutive pings
  
 protected:
  std::ifstream input_;
  
}; // DataSource
   

#endif // __NIMS_DATA_SOURCE_H__
