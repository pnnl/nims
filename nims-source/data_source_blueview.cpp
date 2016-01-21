/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_blueview.cpp
 *
 *  Created by Shari Matzner on 01/19/2016.
 *  Copyright 2016 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

 /***************************************
 NOTE:  The BlueView SDK provides a C interface
 and a C++ wrapper for the C interface.  The
 disadvantage of the C++ wrapper is that it throws
 exceptions which is a pain in the arse. Therefore
 I will use the C interface.
 */

#include "data_source_blueview.h"


#include "log.h"

using namespace std;
//-----------------------------------------------------------------------------
DataSourceBlueView::DataSourceBlueView(std::string const &host_addr)
{
	host_addr_ = host_addr;
	head_ = NULL;
	son_ = BVTSonar_Create();
	if( son_ == NULL )
	{
		NIMS_LOG_ERROR << "BVTSonar_Create: failed";
	}
} // DataSourceBlueView::DataSourceBlueView

//-----------------------------------------------------------------------------
DataSourceBlueView::~DataSourceBlueView() 
{ 
	BVTSonar_Destroy(son_); 
}

//-----------------------------------------------------------------------------
int DataSourceBlueView::connect()
{
	if( son_ == NULL )
	{
		NIMS_LOG_ERROR << "BVTSonar_Create: failed";
		return -1;
	}
    int ret = BVTSonar_Open(son_, "NET", host_addr_.c_str());
	if( ret != 0 )
	{
		NIMS_LOG_ERROR << "BVTSonar_Open: ret=" << ret;
		return -1;
	}

	// Make sure we have the right number of heads
	int heads = -1;
	BVTSonar_GetHeadCount(son_, &heads);
	NIMS_LOG_DEBUG << "BVTSonar_GetHeadCount: " << heads;


	// Get the first head
	ret = BVTSonar_GetHead(son_, 0, &head_);
	if( ret != 0 )
	{
		NIMS_LOG_ERROR << "BVTSonar_GetHead: ret=" << ret;
		return 1;
	}

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