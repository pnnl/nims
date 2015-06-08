/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_m3.h
 *  
 *  Created by Shari Matzner on 05/15/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_DATA_SOURCE_M3_H__
#define __NIMS_DATA_SOURCE_M3_H__


#include "data_source.h"

/*-----------------------------------------------------------------------------
Base class for sonar data sources.  Classes for specific devices will be
derived from this class.  The unit of data is a ping, which is both the ping
attributes and the received echo.
*/

class DataSourceM3 : public DataSource {
 public:
  DataSourceM3(const std::string& path) : DataSource(path) {};
  
  int GetPing(Frame* pdata);  // get the next ping from the source
  //size_t ReadPings(Frame* pdata, const size_t& num_pings); // read consecutive pings
  
 private:
  
}; // DataSourceM3
   

#endif // __NIMS_DATA_SOURCE_M3_H__
