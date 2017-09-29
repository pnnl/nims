/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_amp-m3.h
 *  
 *  Created by Shari Matzner on 04/13/2017.
 *  Copyright 2017 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#ifndef __NIMS_DATA_SOURCE_AMP_M3_H__
#define __NIMS_DATA_SOURCE_AMP_M3_H__


#include "data_source.h"

#include <string>
#include <boost/filesystem.hpp>
#include <fstream>

namespace fs = boost::filesystem;

/*-----------------------------------------------------------------------------
Class for University of Washinton's Adaptable Monitoring Package (AMP)

This is a post-processing data source.  In other words, it reads recorded data from files
rather than from a live instrument.

AMP General Data Format:

- Data is stored in the following format: *\yyyy_mm_dd\yyyy_mm_dd HH_MM_SS\INSTRUMENT
- There is one directory for each day of data collection
- There is one subdirectory for each AMP offload, which is named with the timestamp when 
  it was created
- There is one subdirectory for each instrument (BlueView, M3, PAM, Camera 1, Camera 2)

AMP M3 Data Format

- There are three subdirectories ("BeaamList","Header", and "Intensity")
- The .txt file in each subdirectory is named with the first Unix timestamp of the 
  contained sequence 

Note: BeaamList is a typo from early in development which has been written into 
    processing codes to avoid conflicts.

- Header contains n rows of data (where n is the number of M3 pings contained in the 
  sequence). Column headings can be found in the "Header Format" file. All units
	match the IMB data stream.
- Beaamlist contains n rows of data containing the beam angle for each column of M3 data 
  (# columns = # beams)
- Intensity contains the raw intensity data from the M3, which is # beams x # samples 
  (found in header) x n

Notes:
- All data should be in imaging 15 degree mode (128 beams x 1681 samples)
*/
struct AmpM3Params {
  std::string datadir; // path for files to process
  std::string file; // specific file to process
};

class DataSourceAMP_M3 : public DataSource {
public:
    DataSourceAMP_M3(AmpM3Params const &params);
    ~DataSourceAMP_M3();
    
    int connect();
    bool is_good()   { return (input_ != -1); };  // check if source is in a good state
    bool more_data() { return true; };  // TODO: check for not "end of file"
    int GetPing(Frame* pdata);  // get the next ping from the source
    //size_t ReadPings(Frame* pdata, const size_t& num_pings); // read consecutive pings
    
private:
    bool files_;
    fs::path dir_path_;
    std::vector<fs::path> file_list_;
    std::ifstream header_;
    std::ifstream beamlist_;
    std::ifstream intensity_;

}; // DataSourceM3


#endif // __NIMS_DATA_SOURCE_AMP_M3_H__
