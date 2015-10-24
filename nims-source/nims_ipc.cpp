/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  nims_ipc.cpp
 *
 *  Created by Shari Matzner on 10/23/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
// for POSIX shared memory
#include <fcntl.h>    // O_* constants
#include <unistd.h>   // sysconf
#include <assert.h>   // assert
#include <sys/mman.h> // mmap, shm_open

#include <string>
#include <cstring> // memcpy

#include "log.h"

using namespace std;

/*
 Mmap doesn't technically require rounding up to a page boundary
 for the length you pass in, but we'll do that anyway. If other
 components start sharing memory, we can make this external.
*/
const size_t kPageSize = sysconf(_SC_PAGE_SIZE);
static size_t SizeForSharedData(size_t size_data_to_share)
{
    assert(kPageSize > 0);
    size_t number_of_pages = size_data_to_share / kPageSize + 1;
    return kPageSize * number_of_pages;

}

int share_data(const string& shared_name, size_t hdr_size, char* phdr, size_t data_size, char* pdata )
{
/*
     Create -rw------- since we don't need executable pages.
     Using O_EXCL could be safer, but these can still exist
     in /dev/shm after a program crash.
    */
    int fd = shm_open(shared_name.c_str(), O_CREAT | O_TRUNC | O_RDWR, 
            S_IRUSR | S_IWUSR);
    
    // !!! early return
    if (-1 == fd) {
        nims_perror("shm_open() in share_data()");
        return -1;
    }
    
    const size_t map_length = SizeForSharedData(hdr_size + data_size);
    assert(map_length > hdr_size + data_size);
    
    // !!! early return
    // could use fallocate or posix_fallocate, but ftruncate is portable
    if (0 != ftruncate(fd, map_length)) {
        nims_perror("ftruncate() in share_data()");
        shm_unlink(shared_name.c_str());
        return -1;
    }

    // mmap the file descriptor from shm_open into this address space
    char *pshared;
    pshared = (char *)mmap(NULL, map_length,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    close(fd);
    
    // !!! early return
    if (MAP_FAILED == pshared) {
        nims_perror("mmap() in share_data");
        shm_unlink(shared_name.c_str());
        return -1;
    }
        
    // copy data to the shared mem
    if (hdr_size > 0)
    {
        memcpy(pshared, phdr, hdr_size);
    }
   if (data_size > 0)
   {
       memcpy(pshared + hdr_size, &data_size, sizeof(data_size));
       memcpy(pshared + hdr_size + sizeof(data_size), pdata, data_size);
   }
     
    // done with the region in this process; ok to do this?
    // pretty sure we can't shm_unlink here
    munmap(pshared, map_length);

}; // share_data()