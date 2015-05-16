/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source.cpp
 *  
 *  Created by Shari Matzner on 05/15/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#include "data_source.h"

#include <iostream>
using namespace std;

//-----------------------------------------------------------------------------
// Constructor
DataSource::DataSource(const std::string& path) 
{
    cout << "opening data source " << path << endl;
    input_.open(path, ios::in | ios::binary);
    
} // Constructor
//-----------------------------------------------------------------------------
// Destructor
DataSource::~DataSource()
{
    input_.close();
    
} // Destructor
  

