/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  frame_buffer.cpp
 *  
 *  Created by Shari Matzner on 04/23/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */
#include "frame_buffer.h"

// Constructor
FrameBufferInterface::FrameBufferInterface()
{
} // Constructor

// Destructor
FrameBufferInterface::~FrameBufferInterface()
{
} // Destructor

// Put a new frame into the buffer.  Returns the
// index of the new frame.
int FrameBufferInterface::PutNewFrame(const Frame &new_frame)
{
    return 0; 
} // PutNewFrame
	    
// Get the next frame in the buffer, "next" meaning
// relative to the last frame that was retrieved by the
// calling process.  Returns the index of the frame.
int FrameBufferInterface::GetNextFrame(Frame* next_frame)
{
    return 0; 
} // GetNextFrame
	    
	    

