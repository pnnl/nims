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

#include <string>
#include <netinet/in.h> // struct sockaddr_in

/*-----------------------------------------------------------------------------
Class for Kongsberg M3 Multibeam Sonar
*/

class DataSourceM3 : public DataSource {
public:
    DataSourceM3(std::string const &host_addr);
    ~DataSourceM3();
    
    int connect();
    bool is_good()   { return (input_ != -1); };  // check if source is in a good state
    bool more_data() { return true; };  // TODO: check for not "end of file"
    int GetPing(Frame* pdata);  // get the next ping from the source
    //size_t ReadPings(Frame* pdata, const size_t& num_pings); // read consecutive pings
    
private:
    struct sockaddr_in m3_host_;
    
}; // DataSourceM3


#endif // __NIMS_DATA_SOURCE_M3_H__
