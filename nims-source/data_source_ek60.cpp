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

#include "log.h"


//-----------------------------------------------------------------------------
DataSourceEK60::DataSourceEK60()
{
    
} // DataSourceEK60::DataSourceEK60

//-----------------------------------------------------------------------------
DataSourceEK60::~DataSourceEK60() { close(input_); }

//-----------------------------------------------------------------------------
int DataSourceEK60::connect()
{
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

    
} // DataSourceEK60::GetPing