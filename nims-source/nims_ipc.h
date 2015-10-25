/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  nims_ipc.h
 *  
 *  Created by Shari Matzner on 10/23/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_NIMS_IPC_H__
#define __NIMS_NIMS_IPC_H__

#include <string>

// Share data using shared memory, optionally prefaced with a header.
// The size of the data is written to shared mem, then the data itself.
int share_data(const std::string& shared_name,
               size_t hdr_size=0, char* phdr=nullptr,
               size_t data_size=0, char* pdata=nullptr );

#endif // __NIMS_NIMS_IPC_H__
