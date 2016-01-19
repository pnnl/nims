/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_blueview.cpp
 *
 *  Created by Shari Matzner on 01/19/2016.
 *  Copyright 2016 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#include "data_source_blueview.h"

#include "log.h"


//-----------------------------------------------------------------------------
DataSourceBlueView::DataSourceBlueView()
{
    
} // DataSourceBlueView::DataSourceBlueView

//-----------------------------------------------------------------------------
DataSourceBlueView::~DataSourceBlueView() { close(input_); }

//-----------------------------------------------------------------------------
int DataSourceBlueView::connect()
{
    return 0;
    
} // DataSourceBlueView::connect


//-----------------------------------------------------------------------------
int DataSourceBlueView::GetPing(Frame* pframe)
{
    return 0;
    
    if ( input_ == -1 ) {
        NIMS_LOG_ERROR << ("DataSourceBlueView::GetPing() Not connected to source.");
        return -1;
    }

    
} // DataSourceBlueView::GetPing