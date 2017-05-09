/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_amp-m3.cpp
 *  
 *  Created by Shari Matzner on 04/13/2017.
 *  Copyright 2017 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#include "data_source_amp-m3.h"

#include <iostream> // cout, cerr, clog
 #include <fstream> // file I/O
#include <cstring>
#include <cstdlib>  // malloc, free
#include <stdint.h> // fixed width integer types
#include <math.h>   // sqrt, pow

#include <sys/types.h>

#include "log.h"

using namespace std;

//-----------------------------------------------------------------------------
// from M3 IMB Beamformed Data Format, document 922-20007002
//-----------------------------------------------------------------------------
    
#define ERROR_MSG_EXIT(err_msg)     \
{                                   \
    if (bfData != nullptr) free(bfData); \
    NIMS_LOG_ERROR << err_msg;          \
    return -1;                         \
}                                   \

#define INT32U  uint32_t
#define INT32S  int32_t
#define INT16U  uint16_t
#define INT16S  int16_t
#define INT8U   uint8_t

#define HDR_SYNC_INT16U_1     (INT16U)0x8000
#define HDR_SYNC_INT16U_2     (INT16U)0x8000
#define HDR_SYNC_INT16U_3     (INT16U)0x8000
#define HDR_SYNC_INT16U_4     (INT16U)0x8000

#define PKT_DATA_TYPE_BEAMFORMED (INT16U)0x1002

#define MAX_NUM_BEAMS           (INT32U)1024


typedef struct
{
    float   I;
    float   Q;
} Ipp32fc_Type;

#define ABS_IQ(x) sqrt(pow(x.I,2)+pow(x.Q,2));

// Time Varying Gain (TVG)
// see section 9.9.3 Changing the TVG, p. 94 in User's Manual
// Chu, D. and Hufnagle, Jr., L., "Time varying gain (TVG) measurements
//    of a multibeam echo sounder for applications to quantitative acoustics.", 2006
// www.dtic.mil/cgi-bin/GetTRDoc?AD=ADA498689
// http://www.hydro-international.com/issues/articles/id890-Digital_Sidescan_is_this_the_end_of_TVG_and_AGC.html
//
// TL = max(A log10 r + B r + C, L)
// where TL is transmission loss, r is range
typedef struct
{
    INT16U  A; // the spreading coefficient
    INT16U  B; // the absorption coefficient in dB/km
    float   C; // the TVG curve offset in dB
    float   L; // the maximum gain limit in dB
} TVG_Params_Type;

typedef struct
{
	float fOffsetA;	// rotator offset A in meters
	float fOffsetB;	// rotator offset B in meters
	float fOffsetR;	// rotator offset R in degrees
	float fAngle;	// rotator angle in degrees

}M3_ROTATOR_OFFSETS;

typedef struct
{
    INT16U sync_word_1;
    INT16U sync_word_2;
    INT16U sync_word_3;
    INT16U sync_word_4;
    INT16U data_type;       // always 0x1002
    INT16U reserved_field;  // NOTE:  this is in spec but not in load_image_data.c
    INT32U reserved[10];
    INT32U packet_body_size;
} Packet_Header_Struct;

typedef struct
{
    INT32U packet_body_size; // this should match value in header
    INT32U reserved[10];
} Packet_Footer_Struct;

/****************************
 * Data header               *
 ****************************/
typedef struct
{
    INT32U  dwVersion;
    INT32U  dwSonarID;
    INT32U  dwSonarInfo[8];
    INT32U  dwTimeSec;
    INT32U  dwTimeMillisec;
    float   fVelocitySound;
    INT32U  nNumSamples;
    float   fNearRange;
    float   fFarRange;
    float   fSWST;
    float   fSWL;
    INT16U  nNumBeams;
    INT16U  wReserved1;
    float   fBeamList[MAX_NUM_BEAMS];
    float   fImageSampleInterval;
    INT16U  wImageDestination;
    INT16U  wReserved2;
    INT32U  dwModeID;
    INT32S  nNumHybridPRI;
    INT32S  nHybridIndex;
    INT16U  nPhaseSeqLength;
    INT16U  iPhaseSeqIndex;
    INT16U  nNumImages;
    INT16U  iSubImageIndex;
    
    INT32U  dwSonarFreq;
    INT32U  dwPulseLength;
    INT32U  dwPingNumber;
    
    float   fRXFilterBW;
    float   fRXNominalResolution;
    float   fPulseRepFreq;
    char    strAppName[128];
    char    strTXPulseName[64];
    TVG_Params_Type sTVGParameters;
    float   fCompassHeading;
    float   fMagneticVariation;
    float   fPitch;
    float   fRoll;
    float   fDepth;
    float   fTemperature;
    
    float   fXOffset;  // M3_OFFSET
    float   fYOffset;
    float   fZOffset;
    float   fXRotOffset;
    float   fYRotOffset;
    float   fZRotOffset;
	INT32U dwMounting;
    
    double  dbLatitude;
    double  dbLongitude;
    float   fTXWST;
    
	unsigned char bHeadSensorsVersion;
	unsigned char bHeadHWStatus;
    INT8U   byReserved1;
    INT8U   byReserved2;
    
    float fInternalSensorHeading;
	float fInternalSensorPitch;
	float fInternalSensorRoll;
	M3_ROTATOR_OFFSETS aAxesRotatorOffsets[3];

    INT16U nStartElement;
    INT16U nEndElement;
    char   strCustomText1[32];
    char   strCustomText2[32];
    float  fLocalTimeOffset;
    
    unsigned char  reserved[3876];
    
    
 } Data_Header_Struct;

// forward declaration; implementation is at the end of this file, out of the way.
std::ostream& operator<<(std::ostream& strm, const Data_Header_Struct& hdr);

//-----------------------------------------------------------------------------
// DataSourceAMP_M3::DataSourceAMP_M3
// open a connection to the M3 host program
// expecting a host address in xxx.xxx.xxx.xxx form
DataSourceAMP_M3::DataSourceAMP_M3(AmpM3Params const &params)
{
    
   dir_path_ = fs::path(params.datadir);    


} // DataSourceAMP_M3::DataSourceAMP_M3

DataSourceAMP_M3::~DataSourceAMP_M3() 
{ 
    // close any open files 
}

//-----------------------------------------------------------------------------
// DataSourceAMP_M3::connect
// connect to the M3 host
int DataSourceAMP_M3::connect()
{
  // TO DO: check if "m3" is already at the end of the path
  dir_path_ /= "m3";

    if ( !fs::exists(dir_path_) )
    {
        // TODO: perror
        NIMS_LOG_ERROR << "Error data files not found " << dir_path_;
        return -1;
    }

    // for now, just process one set of files
    fs::path header_path = dir_path_;
    header_path /= "Header";
    fs::path beam_path = dir_path_;
    beam_path /= "BeaamList";
    fs::path intensity_path_ = dir_path_;
    intensity_path_ /= "Intensity";

    file_list_.clear();
    fs::directory_iterator end_itr;
     for (fs::directory_iterator it(intensity_path_); it != end_itr; ++it)
     {
        if ( fs::is_regular_file(*it) && it->path().extension() == ".txt" )
            file_list_.push_back(it->path());
     }
    // sort in reverse time order so we can process the list by popping off
    std::sort(file_list_.begin(), file_list_.end(),
              [](const fs::path& p1, const fs::path& p2)
              {
                  return fs::last_write_time(p1) > fs::last_write_time(p2);
              });

    if (file_list_.size() == 0)
    {
        NIMS_LOG_ERROR << "Error no data files found " << intensity_path_;
        return -1;
    }
    for (int i = 0; i < file_list_.size(); ++i)
    {
        NIMS_LOG_DEBUG << file_list_[i];
    }
    string file_name = file_list_.back().c_str();
    file_list_.pop_back();

    // open the header format file and read the column labels for the header
    fs::path header_format_path = header_path;
    header_format_path /= "Header Format.txt";
    ifstream header_format_file(header_format_path.string());
    if ( !header_format_file.good() )
    {
         NIMS_LOG_ERROR << "Error reading header format " << header_format_path;
        return -1;
    }
    // TO DO:  test for old format or new format -- see loadM3FromFileName.m
    // For now, we'll assume new format.
    string header_format;
    getline(header_format_file, header_format);

    input_ == 1;
    return 0;

} // DataSourceAMP_M3::connect


//-----------------------------------------------------------------------------
// DataSourceAMP_M3::GetPing
// get the next ping from the source
int DataSourceAMP_M3::GetPing(Frame* pframe)
{
       
    //NIMS_LOG_DEBUG << "    extracting header";
    /*
    strncpy(pframe->header.device, "Kongsberg M3 Multibeam sonar", 
            sizeof(pframe->header.device));
    pframe->header.version = header.dwVersion;
    pframe->header.ping_num = header.dwPingNumber;
    pframe->header.ping_sec = header.dwTimeSec;
    pframe->header.ping_millisec = header.dwTimeMillisec;
    pframe->header.soundspeed_mps = header.fVelocitySound;
    pframe->header.num_samples = header.nNumSamples;
    pframe->header.range_min_m = header.fNearRange;
    pframe->header.range_max_m = header.fFarRange;
    pframe->header.winstart_sec = header.fSWST;
    pframe->header.winlen_sec = header.fSWL;
    pframe->header.num_beams = header.nNumBeams;
    for (int m=0; m<header.nNumBeams; ++m)
    {
        if (kMaxBeams == m) break; // max in frame_buffer.h
        pframe->header.beam_angles_deg[m] = header.fBeamList[m];
    }
    pframe->header.freq_hz = header.dwSonarFreq;
    pframe->header.pulselen_microsec = header.dwPulseLength;
    pframe->header.pulserep_hz = header.fPulseRepFreq;
   
    //NIMS_LOG_DEBUG << "    extracting data";
    // copy data to frame as real intensity value
    size_t frame_data_size = sizeof(framedata_t)*(header.nNumSamples)*(header.nNumBeams);
    pframe->malloc_data(frame_data_size);
    if ( pframe->size() != frame_data_size )
        ERROR_MSG_EXIT("Error allocating memory for frame data.");

    framedata_t* fdp = pframe->data_ptr();
    for (int n = 0; n < header.nNumBeams; ++n) // beam
    {
        for (int m = 0; m < header.nNumSamples; ++m) // sample
        {
            fdp[m*(header.nNumBeams) + n] = ABS_IQ( bfData[n*(header.nNumSamples) + m] );
            
        }
    }
 */   
//	free(bfData);
    return 0;
} // DataSourceAMP_M3::GetPing

/*
//-----------------------------------------------------------------------------
// DataSourceAMP_M3::ReadPings
// read the specified number of pings
size_t DataSourceAMP_M3::ReadPings() 
{
    return 0;
    
} // DataSourceAMP_M3::ReadPings
*/


