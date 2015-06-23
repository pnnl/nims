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


#include "frame_buffer.h"
/*-----------------------------------------------------------------------------
Base class for sonar data sources.  Classes for specific devices will be
derived from this class.  The unit of data is a ping, which is both the ping
attributes (metadata) and the received echo data.
*/

class DataSource {
 public:
    DataSource() {};  // Constructor
    ~DataSource() {}; // Destructor
  
  virtual bool is_good()   =0;  // check if source is in a good state
  virtual bool more_data() =0;  // check for not "end of file" condition
  virtual int GetPing(Frame* pdata) =0;     // get the next ping from the source
  //virtual size_t ReadPings(Frame* pdata, const size_t& num_pings) =0; // read consecutive pings
  
 protected:
  int input_; // file descriptor for the source
  
}; // DataSource
   

#endif // __NIMS_DATA_SOURCE_H__
