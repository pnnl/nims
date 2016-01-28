/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_blueview.h
 *  
 *  Created by Shari Matzner on 01/19/2016.
 *  Copyright 2016 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_DATA_SOURCE_BLUEVIEW_H__
#define __NIMS_DATA_SOURCE_BLUEVIEW_H__


#include "data_source.h" // base class

 #include <string>

#include <bvt_sdk.h> // BlueView SDK

/*-----------------------------------------------------------------------------
Class for Teledyne BlueView Imaging Sonar.
*/

class DataSourceBlueView : public DataSource {
 public:
    DataSourceBlueView(std::string const &host_addr);  // Constructor
    ~DataSourceBlueView(); // Destructor
  
  int connect();   // connect to data source
  bool is_good() { return (head_ != NULL); };  // check if source is in a good state
    bool more_data() { return true; };  // TODO: check for not "end of file" condition
  int GetPing(Frame* pdata);     // get the next ping from the source
  //virtual size_t ReadPings(Frame* pdata, const size_t& num_pings) =0; // read consecutive pings
    
    private:
    	std::string host_addr_;
    	BVTSonar son_;
    	BVTHead  head_;
        BVTImageGenerator imager_;
        uint32_t  ping_count_;
    

}; // DataSourceBlueView
   

#endif // __NIMS_DATA_SOURCE_BLUEVIEW_H__
