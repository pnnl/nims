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
/*-----------------------------------------------------------------------------
Class for Simrad EK60 Echosounder.
*/

class DataSourceEK60 : public DataSource {
 public:
    DataSourceEK60();  // Constructor
    ~DataSourceEK60(); // Destructor
  
  int connect();   // connect to data source
  bool is_good() { return (input_ != -1); };  // check if source is in a good state
    bool more_data() { return true; };  // TODO: check for not "end of file" condition
  int GetPing(Frame* pdata);     // get the next ping from the source
  //virtual size_t ReadPings(Frame* pdata, const size_t& num_pings) =0; // read consecutive pings
    
}; // DataSourceEK60
   

#endif // __NIMS_DATA_SOURCE_EK60_H__
