/*
 *  Nekton Interaction Monitoring System (NIMS)
 *
 *  data_source_m3.cpp
 *  
 *  Created by Shari Matzner on 05/15/2015.
 *  Copyright 2015 Pacific Northwest National Laboratory. All rights reserved.
 *
 */

#include "data_source_m3.h"

#include <iostream> // cout, cerr, clog
#include <cstring>
#include <cstdlib> // malloc, free
#include <stdint.h> // fixed width integer types

using namespace std;

//-----------------------------------------------------------------------------
// from Matlab M3 Toolbox common/load_raw_data.c
//-----------------------------------------------------------------------------

#define DEBUG_PRINT_ON

#ifdef DEBUG_PRINT_ON
    #define DEBUG_PRINT(text) clog << text << endl;
    #define DEBUG_PRINT_2(text, arg2) clog << text << arg2 << endl;
    #define DEBUG_PRINT_3(text, arg2, arg3) clog << text << arg2 << arg3 << endl;
    #define DEBUG_PRINT_4(text, arg2, arg3, arg4) clog << text << arg2 << arg3 << arg4 << endl;
    #define DEBUG_PRINT_5(text, arg2, arg3, arg4, arg5) clog << text << arg2<< arg3 << arg4 << arg5 << endl;
#else
    #define DEBUG_PRINT(text)
    #define DEBUG_PRINT_2(text, arg2)
    #define DEBUG_PRINT_3(text, arg2, arg3)
    #define DEBUG_PRINT_4(text, arg2, arg3, arg4)
    #define DEBUG_PRINT_5(text, arg2, arg3, arg4, arg5)
#endif
    
#define ERROR_MSG_EXIT(err_msg)     \
{                                   \
    cerr << err_msg << endl;          \
    return -1;                         \
}                                   \

#define INT32U  uint32_t
#define INT16U  uint16_t
#define INT16S  int16_t
#define INT8U   uint8_t

#define HDR_SYNC_INT16U_1     (INT16U)0x8000
#define HDR_SYNC_INT16U_2     (INT16U)0x8000
#define HDR_SYNC_INT16U_3     (INT16U)0x8000
#define HDR_SYNC_INT16U_4     (INT16U)0x8000

#define PKT_DATA_TYPE_OLD_HDR   	(INT16U)0x1001
#define PKT_DATA_TYPE_BF_HDR    	(INT16U)0x100A
#define PKT_DATA_TYPE_PING_HDR  	(INT16U)0x100B
#define PKT_DATA_TYPE_GENERIC_HDR   (INT16U)0x100C

#define MAX_GENERIC_BYTES	(int)(64*1024)
#define MAX_NUM_IMAGES          (int)8
#define MUM_CUSTOM_TEXT_MAX_LETTERS (int)32
#define MAX_NUM_ELEMENTS        (int)128
#define MAX_NUM_RANGE_CELLS     (int)30000
#define MAX_LENGTH_REF_PULSE    (int)5000
#define MAX_NUM_SPATIAL_PHASES  (INT32U)256
#define MAX_NUM_FOCAL_ZONES     (INT32U)256
#define MAX_NUM_BEAMS           (INT32U)512


typedef struct
{
    INT16S   I;
    INT16S   Q;
} Ipp16sc_Type;

typedef struct
{
    float   I;
    float   Q;
} Ipp32fc_Type;

typedef struct
{
    INT16U  A;
    INT16U  B;
    float   C;
    float   L;
} TVG_Params_Type;

typedef struct
{
    float   fXOffset;
    float   fYOffset;
    float   fZOffset;
    float   fXRotOffset;
    float   fYRotOffset;
    float   fZRotOffset;
	INT32U dwMounting;

}M3_OFFSET;

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
    INT16U data_type;
    INT32U reserved[10];
    INT32U packet_body_size;
} Packet_Header_Struct;

/****************************
 * Old header               *
 ****************************/
typedef struct
{
    INT32U  dwVersion;
    INT32U  dwSonarID;
    INT32U  dwSonarInfo[8];
    INT32U  dwModeID;
    INT32U  dwPRI_ID;
    INT16U  wPRI_Index;
    INT32U  dwPingCounter;
    INT32U  dwTimeSec;
    INT32U  dwTimeMillisec;
    float   fFocalTolerance[MAX_NUM_IMAGES];
    float   velocitySound;
    float   fSystemMinSWST[MAX_NUM_IMAGES];
    INT32U  nNumRangeCells;
    INT16U  nNumElements;
    INT16U  nNumImages;
    INT16U  iPhaseSeqLength;
    INT16S  nPhaseSeqIndex;
    INT16U  nImageDestination;
    INT16U  nNumHybridPRIs;
    INT16U  nHybridRangeIndex;
    INT16U  RangeProcessOnOff[MAX_NUM_IMAGES];
    INT16U  AzimuthProcessOnOff[MAX_NUM_IMAGES];
    INT32U  dwSubArrayFrequency[MAX_NUM_IMAGES];
    INT16U  nStartElement[MAX_NUM_IMAGES];
    INT16U  nEndElement[MAX_NUM_IMAGES];
    float   SubArrayGeometryXYZ[MAX_NUM_IMAGES][512][3];
    float   fSWST[MAX_NUM_IMAGES];
    float   fSWL[MAX_NUM_IMAGES];
    float   fMinRangeMeter[MAX_NUM_IMAGES];
    float   fMaxRangeMeter[MAX_NUM_IMAGES];
    INT16U  nExtraSamplesNear[MAX_NUM_IMAGES];
    INT16U  nExtraSamplesFar[MAX_NUM_IMAGES];
    INT32U  nStartRawSample[MAX_NUM_IMAGES];
    INT32U  nEndRawSample[MAX_NUM_IMAGES];
    float   fRawSampleInterval[MAX_NUM_IMAGES];
    INT8U   bFocusing[MAX_NUM_IMAGES];
    INT8U   bRXPhaseAmpCorrection[MAX_NUM_IMAGES];

    INT16U  EIQMode; //not in host ?? 2 bytes here,
    INT16U  bReserved[MAX_NUM_IMAGES-1];//14 bytes here
    INT16U  nNumBeams[MAX_NUM_IMAGES];
    float   fBeamList[MAX_NUM_IMAGES][1024];
    INT16U  wRangeWeighting[MAX_NUM_IMAGES];
    INT16U  wAzimuthWeighting[MAX_NUM_IMAGES];
    Ipp32fc_Type RXPhaseAmpCorrection[MAX_NUM_IMAGES][512];
    float   RangeRefPulseCorrection[MAX_NUM_IMAGES][8];
    float   fRxFilterBW[MAX_NUM_IMAGES];
    float   fNominalResolution[MAX_NUM_IMAGES];
    float   fPulseRepFreq[MAX_NUM_IMAGES];
    char    strAppName[128];
    char    strTxPulse[64];
    INT16U  wImageSeqID;
    INT16U  wXDCRType;
    TVG_Params_Type sTVGParameters[MAX_NUM_IMAGES];
    float   fCompassHeading;
    float   fMagneticVariation;
    float   fPitch;
    float   fRoll;
    float   fDepth;
    float   fTemperature;  

	M3_OFFSET	sOffsets;

    double  dbLatitude;
    double  dbLongitude;
    float   fTXWST[MAX_NUM_IMAGES];
    INT32U  dwPulseLength;
    INT32U  dwPostProcessingMode;
    float   fInternalSensorHeading;
    float   fInternalSensorPitch;
    float   fInternalSensorRoll;

	M3_ROTATOR_OFFSETS	sAxesRotatorOffsets[3];
    
    char strCustomText[MUM_CUSTOM_TEXT_MAX_LETTERS]; //32bytes
	char strROVText[MUM_CUSTOM_TEXT_MAX_LETTERS]; //32 bytes total 64 bytes = 16 DWORD, so 885-16 = 869 reserved
	float fLocalTimeOffset; //combined bias in hours. 4 bytes
	
	INT32U dwProcessingType;
	float  fProcessingParam0;
	float  fProcessingParam1;
	float  fProcessingParam2;

    INT32U  dwReserved[885-21];
    INT32U  dwReferencePulse[MAX_NUM_IMAGES];
} Header_Struct;


/****************************
 * Beamformer packet header *
 ****************************/
typedef struct
{
	INT32U  dwVersion;
    INT32U  dwPacketID;
	INT32U	dwSonarID;
	INT32U	dwSonarInfo[8];
    float   fSystemMinSWST;
    float   fVSound;
    float   fUserVSound;
    char    strAppName[128];
    float   fXOffset;
    float   fYOffset;
    float   fZOffset;
    float   fXRotOffset;
    float   fYRotOffset;
    float   fZRotOffset;
    INT32U  dwMounting;
    float   fRotatorOffsetA[3];
    float   fRotatorOffsetB[3];
    float   fRotatorOffsetR[3];
    INT16U  wProcessingType;
    INT16U  wNumElements;
    float   fArrayGeometryXYZ[256][3];
    Ipp32fc_Type RXPhaseAmpCorrection[256];
    float   RangeRefPulseCorrection[4][8];
    INT16U  wNumTxSpatialPhases;
    INT16U  nNumHybridPRIs;
    float   fProcessingParam0;
    float   fProcessingParam1;
    float   fProcessingParam2;
    float   fMinModeRangeMeter;
    float   fMaxModeRangeMeter;
    INT8U   bReserved1[8];
    
} BF_Hdr_Struct;

typedef struct
{
    INT16U  iTXSpatialPhaseIndex;
    INT16U  wXDCRType;
    INT8U   TXPhase_C0;
    INT8U   TXPhase_C1;
    INT8U   TXPhase_C2;
    INT8U   TXPhase_C3;
    INT8U   TXAmp_C0;
    INT8U   TXAmp_C1;
    INT8U   TXAmp_C2;
    INT8U   TXAmp_C3;
    INT16U  nNumImages;
    INT16U  TxPwr;
    INT8U   reserved[28];    
} BF_Hdr_SpatialPhase_Struct;

typedef struct
{
    INT32U  dwSubArrayFrequency;
    float   fFocalTolerance;
    INT16U  nNumFocalZones;
    INT16U  iImageIndex;
    INT16U  nNumBeams;
    INT8U   reserved[30];
} BF_Hdr_Image_Struct;

typedef struct
{
    INT8U   iFocalZoneIndex;
    INT8U   reserved[3];
    float   fNearEdge;
    float   fFarEdge;
    float   fCenter;
    INT8U   reserved2[8];    
} BF_Hdr_Image_Focal_Zone_Struct;

typedef struct
{
    INT16U  iBeamIndex;
    INT8U   reserved[2];
    float   fBeamAngle;
    float   fBeamOrigin_X;
    float   fBeamOrigin_Y;
    float   fBeamOrigin_Z;
    INT8U   reserved2[4];    
} BF_Hdr_Beam_Struct;

typedef struct
{
    INT8U   iFocalZoneIndex;
    INT8U   nStartElement;
    INT8U   nEndElement;
    INT8U   nAzimuthWeightingType;
    INT8U   nAzimuthWeightingCoef;
    INT8U   reserved[3];
} BF_Hdr_Beam_Focal_Zone_Struct;

typedef struct
{
	INT32U	dwVersion;
    INT32U  dwSonarID;
	INT32U	dwBFPacketID;	
	INT32U  dwModeID;						
	INT32U	dwPRI_ID;						
	INT16U	wPRI_Index;	
	INT16U	nPhaseSeqIndex;
	INT16U	nImageDestination;
	INT16U	wImageSeqID;
	INT32U	dwPingCounter;			
	INT32U	dwTimeSec;					
	INT32U	dwTimeMillisec;			
	INT32U	nNumRawSamples;				
	float	fSWST[MAX_NUM_IMAGES];			
	float	fSWL[MAX_NUM_IMAGES];				
	float	fMinRangeMeter[MAX_NUM_IMAGES];				
	float	fMaxRangeMeter[MAX_NUM_IMAGES];				
	INT16U	wExtraSampleNear[MAX_NUM_IMAGES];			
	INT16U	wExtraSampleFar[MAX_NUM_IMAGES];				
	INT32U	nStartRawSample[MAX_NUM_IMAGES];				
	INT32U	nEndRawSample[MAX_NUM_IMAGES];				
	float	fRawSampleInterval[MAX_NUM_IMAGES];				
	float	fRxFilterBW[MAX_NUM_IMAGES]; 
	float	fNominalResolution[MAX_NUM_IMAGES];
    
	INT8U	nRangeWeightingType;
	INT8U	nRangeWeightingCoef;
	INT16U	Reserved1;
    
	float fPulseRepFreq;	
	TVG_Params_Type	sTVGParameters[4];
    
	INT8U	bHeadSensorsVersion;
	INT8U	bHeadHWStatus;
	INT8U	bFocusing;
	INT8U	bRXPhaseAmpCorrection;
    
	char	strTxPulse[64];
	float   fCompassHeading;					
	float   fMagneticVariation;		
	float   fPitch;																	
	float   fRoll;																	
	float   fDepth;																					
	float   fTemperature;								
	float   fTXWST;			
	INT32U	dwPulseLength;				
	
	float   fInternalSensorHeading;				
	float   fInternalSensorPitch;				
	float   fInternalSensorRoll;			
	
	char    strCustomText[32];
	char    strCustomText2[32];
	float   fLocalTimeOffset;
	
	float   fRotatorAngle[3];
	INT8U	Reserved2[516];
    
    double  dbLatitude;												
	double  dbLongitude;
	INT32U  dwReferencePulse[MAX_NUM_IMAGES];
} Ping_Hdr_Struct;

typedef struct
{
	INT16U	wVersion;
    INT16U  wPortType;
    INT32U  dwSonarID;
	INT32U	dwPingCounter;
    INT32U  dwBaudrate;
    INT16U  wPortNumber;
    INT16U  wDataSize;
    char    SensorName[32];
    char    SensorProtocol[32];
    INT32U  Reserved[3];
} Generic_Hdr_Struct;

typedef struct
{
    INT32U packet_body_size;
    INT32U reserved[10];
} Footer_Struct;

typedef struct
{
    INT32U  iBeamIndex;
    float   fBeamAngle;
    float   fBeamOrigin_X;
    float   fBeamOrigin_Y;
    float   fBeamOrigin_Z;
    INT16U  nStartElement;
    INT16U  nEndElement;
 
} Beam_Struct;

// TODO:  Make this cleaner -- overloaded function, member function, something more object oriented
bool valid_packet(const Packet_Header_Struct &packet_header, const Header_Struct& header, const BF_Hdr_Struct& bf_header, const Ping_Hdr_Struct ping_header)
{
    switch (packet_header.data_type)
    {
        case PKT_DATA_TYPE_OLD_HDR:
            /****** Old Data Format ******/
            if (header.nNumElements > MAX_NUM_ELEMENTS)
            {
                clog << "Num elements = " << header.nNumElements << endl;;
                cerr << "ERROR: number of elements exceeds max" << endl;
                return false;
            }
            
            if (header.nNumRangeCells > MAX_NUM_RANGE_CELLS)
            {
                clog << "Num range cells = " << header.nNumRangeCells << endl;
                cerr << "ERROR: number of range cells exceeds max" << endl;
                return false;
            }
            
            if (header.dwReferencePulse[0] > MAX_LENGTH_REF_PULSE)
            {
                clog << "Ref Pulse length = " << header.dwReferencePulse[0] << endl;
                cerr << "ERROR: Ref Pulse length exceeds max" << endl;
                return false;
            }
            
            if (header.nNumBeams[0] > MAX_NUM_BEAMS)
            {
                clog << "Number of Beams = " << header.nNumBeams[0] << endl;
                cerr << "ERROR: Number of beams exceeds max" << endl;
                return false;
            }
            
            break;
            
        case PKT_DATA_TYPE_PING_HDR:
            /****** New Data Format ******/
            if (bf_header.wNumElements > MAX_NUM_ELEMENTS)
            {
                clog << "Num elements = " << bf_header.wNumElements << endl;
                cerr << "ERROR: number of elements exceeds max" << endl;
                return false;
            }
            
            if (bf_header.wNumTxSpatialPhases > MAX_NUM_SPATIAL_PHASES)
            {
                clog << "Num spatial phases = " << bf_header.wNumTxSpatialPhases << endl;
                cerr << "ERROR: number of spatial phases exceeds max" << endl;
                return false;
            }
            
            
            if (ping_header.nNumRawSamples > MAX_NUM_RANGE_CELLS)
            {
                clog << "Num range cells = " << ping_header.nNumRawSamples << endl;
                cerr << "ERROR: number of range cells exceeds max" << endl;
                return false;
            }
            
            if (ping_header.dwReferencePulse[0] > MAX_LENGTH_REF_PULSE)
            {
                clog << "Ref Pulse length = " << ping_header.dwReferencePulse[0] << endl;
                cerr << "ERROR: Ref Pulse length exceeds max" << endl;
                return false;
            }
            
            break;
            
        default:
            cerr << "Error: incorrect packet data type" << endl;
            return false;
            break;
    }
    return true;

} //valid_packet

//-----------------------------------------------------------------------------
// DataSourceM3::GetPing
// get the next ping from the source
int DataSourceM3::GetPing(Frame* pframe)
{
    if ( !input_.good() ) return -1;
    
        /* Initialize variables */
    Packet_Header_Struct packet_header;
    Header_Struct        header;
	// NOTE:  One BF Packet is followed by multiple Ping Packets
    static BF_Hdr_Struct        bf_header;
    static BF_Hdr_SpatialPhase_Struct       bf_hdr_spatial_phase;
    static BF_Hdr_Image_Struct              bf_hdr_image;
    static BF_Hdr_Image_Focal_Zone_Struct   bf_hdr_image_focal_zone;
    static BF_Hdr_Beam_Struct               bf_hdr_beam;
    static Beam_Struct                      hdr_beam[MAX_NUM_BEAMS];
    static BF_Hdr_Beam_Focal_Zone_Struct    bf_hdr_beam_focal_zone;
   static char* bf_hdr2_ptr;
   
    Ping_Hdr_Struct       ping_header;
    Generic_Hdr_Struct    generic_header;
    Footer_Struct         footer;

    INT16U TXSpatialPhaseIndex[MAX_NUM_SPATIAL_PHASES];
    INT16U NumImages[MAX_NUM_SPATIAL_PHASES];

    memset(&packet_header, 0, sizeof(Packet_Header_Struct));
    memset(&header,        0, sizeof(Header_Struct));
    //memset(&bf_header,     0, sizeof(BF_Hdr_Struct));
    memset(&ping_header,   0, sizeof(Ping_Hdr_Struct));
    memset(&footer,        0, sizeof(Footer_Struct));
	memset(&generic_header,0, sizeof(Generic_Hdr_Struct));

     INT32U num_bytes_ref_pulse = 0;
    Ipp32fc_Type* refPulse = nullptr;
    double* ref_pulse_I;
    double* ref_pulse_Q;
    
    INT32U num_bytes_raw_data = 0;
    Ipp16sc_Type* rawData = nullptr;
    double* raw_data_I;
    double* raw_data_Q;

    INT32U footer_offset = 0;
    
    int sensor_data_search = 1; // TODO: understand "generic sensor data" better
	int sensor_data_found = 0;
	char generic_str[MAX_GENERIC_BYTES+1];
    memset(generic_str, 0, MAX_GENERIC_BYTES+1);
    INT32U num_bytes_generic_data = 0;
    
    bool got_ping = false;
    int start_pos = input_.tellg(); // save the start position
    
    while ( !got_ping) {

    /* Extract the packet header */
    //memcpy(&packet_header, bfr_ptr, sizeof(Packet_Header_Struct));
     input_.read((char *)&packet_header, sizeof(packet_header));
     if ( !input_.good() ) return -1; // TODO: more sophisticated error checking
        
    /* Check header sync words */
    if ((packet_header.sync_word_1 != HDR_SYNC_INT16U_1) ||
         (packet_header.sync_word_2 != HDR_SYNC_INT16U_2) ||
         (packet_header.sync_word_3 != HDR_SYNC_INT16U_3) ||
         (packet_header.sync_word_4 != HDR_SYNC_INT16U_4))
    {
            //clog << "header.packet_header.sync_word_1 0x%04x\n", packet_header.sync_word_1 << endl;
            //clog << "header.packet_header.sync_word_2 0x%04x\n", packet_header.sync_word_2 << endl;
            //clog << "header.packet_header.sync_word_3 0x%04x\n", packet_header.sync_word_3 << endl;
            //clog << "header.packet_header.sync_word_4 0x%04x\n", packet_header.sync_word_4 << endl;
            
            cerr << "ERROR: failed to find header sync words" << endl;
            return -1;
    }
    cout << "packet type is " << std::hex << packet_header.data_type << endl;
    
    switch (packet_header.data_type)
    {
        case PKT_DATA_TYPE_OLD_HDR:
        {
            clog << "PKT_DATA_TYPE_OLD_HDR" << endl;
            /* Extract the header and read it */
            memset(&header, 0, sizeof(Header_Struct));
            //memcpy(&header, bfr_ptr + sizeof(Packet_Header_Struct), sizeof(Header_Struct));
            input_.read((char *)&header, sizeof(header));
            if ( !input_.good() ) return -1; // TODO: more sophisticated error checking

            
            if ((header.EIQMode==1) && (header.nNumImages>1)) 
            { //for fast EIQ mode, because it has 4 reference pulses for each ping
				clog << "EIQ mode" << endl;
                num_bytes_ref_pulse = 
                    sizeof(Ipp32fc_Type)*(header.dwReferencePulse[0])*header.nNumImages;
            }
            else
            {
                num_bytes_ref_pulse = sizeof(Ipp32fc_Type)*(header.dwReferencePulse[0]);
            }
                
            num_bytes_raw_data = sizeof(Ipp16sc_Type)*(header.nNumRangeCells)*(header.nNumElements);
                
            DEBUG_PRINT_2("Header Version = ", header.dwVersion);
            DEBUG_PRINT_2("num_bytes_ref_pulse = ", num_bytes_ref_pulse);
            DEBUG_PRINT_2("num_bytes_raw_data = ", num_bytes_raw_data);
            DEBUG_PRINT_2("header.dwPingCounter = ", header.dwPingCounter);
                
            //footer_offset = sizeof(Packet_Header_Struct) + sizeof(Header_Struct)
            //                + num_bytes_ref_pulse + num_bytes_raw_data;
            footer_offset = num_bytes_ref_pulse + num_bytes_raw_data;
            
            //curr_ping++;
            got_ping = true;
        }  
        break; // PKT_DATA_TYPE_OLD_HDR
           
        case PKT_DATA_TYPE_BF_HDR:
        {
           clog << "PKT_DATA_TYPE_BF_HDR" << endl;
           memset(&bf_header, 0, sizeof(BF_Hdr_Struct));
            //memcpy(&bf_header, bfr_ptr + sizeof(Packet_Header_Struct), sizeof(BF_Hdr_Struct));
            input_.read((char *)&bf_header, sizeof(bf_header));
            if ( !input_.good() ) return -1; // TODO: more sophisticated error checking

            DEBUG_PRINT_2("BF Packet ID        = ", bf_header.dwPacketID);
            
            if ( bf_hdr2_ptr != nullptr )
            {
                free(bf_hdr2_ptr);
                bf_hdr2_ptr = nullptr;
            }
            
            bf_hdr2_ptr = (char *)malloc(packet_header.packet_body_size);
            INT32U bf_hdr2_offset = 0;
            
            //memcpy(bf_hdr2_ptr,
             //      bfr_ptr + sizeof(Packet_Header_Struct) + sizeof(BF_Hdr_Struct),
              //     packet_header.packet_body_size);
            streampos pos = input_.tellg();
            input_.read((char *)bf_hdr2_ptr, packet_header.packet_body_size);
            input_.seekg(pos);
            if ( !input_.good() ) return -1; // TODO: more sophisticated error checking

            //footer_offset = sizeof(Packet_Header_Struct) + packet_header.packet_body_size;
            footer_offset = packet_header.packet_body_size - sizeof(BF_Hdr_Struct);
			DEBUG_PRINT_2("Num elements = ", bf_header.wNumElements);
			if (bf_header.wNumElements > MAX_NUM_ELEMENTS)
			{
				//DEBUG_PRINT_2("Num elements = ", bf_header.wNumElements);
				free(bf_hdr2_ptr);
				ERROR_MSG_EXIT("ERROR: number of elements exceeds max");
			}
			DEBUG_PRINT_2("Num spatial phases = ", bf_header.wNumTxSpatialPhases);
			if (bf_header.wNumTxSpatialPhases > MAX_NUM_SPATIAL_PHASES)
			{
				//DEBUG_PRINT_2("Num spatial phases = ", bf_header.wNumTxSpatialPhases);
				free(bf_hdr2_ptr);
				ERROR_MSG_EXIT("ERROR: number of spatial phases exceeds max");
			}

			// Loop over spatial phases to get number of images only, 
			// other fields are filled in PKT_DATA_TYPE_PING_HDR 
			for (int nn = 0; nn < bf_header.wNumTxSpatialPhases; nn++)
			{
				memcpy(&bf_hdr_spatial_phase,
					   bf_hdr2_ptr + bf_hdr2_offset,
					   sizeof(BF_Hdr_SpatialPhase_Struct));
				bf_hdr2_offset += sizeof(BF_Hdr_SpatialPhase_Struct);

				TXSpatialPhaseIndex[nn] = bf_hdr_spatial_phase.iTXSpatialPhaseIndex;
				NumImages[nn] = bf_hdr_spatial_phase.nNumImages;
            
				if (bf_hdr_spatial_phase.nNumImages > MAX_NUM_IMAGES)
				{
					DEBUG_PRINT_2("Num Images = ",  bf_hdr_spatial_phase.nNumImages);
					ERROR_MSG_EXIT("Error: num images exceeds max");
				}

				for (int mm = 0; mm < bf_hdr_spatial_phase.nNumImages; mm++)
				{ // Loop over the images 

					memcpy(&bf_hdr_image, bf_hdr2_ptr + bf_hdr2_offset, 
					  sizeof(BF_Hdr_Image_Struct));
					bf_hdr2_offset += sizeof(BF_Hdr_Image_Struct);

					if (bf_hdr_image.nNumFocalZones > MAX_NUM_FOCAL_ZONES)
					{
						DEBUG_PRINT_4("Image Num = ", mm, "Num Focal Zones = ",
						 bf_hdr_image.nNumFocalZones);
						ERROR_MSG_EXIT("Error: num focal zones exceeds max");
					}

			        for (int ii = 0; ii < bf_hdr_image.nNumFocalZones; ii++)
			        { // Loop over the focal zones 
				        memcpy(&bf_hdr_image_focal_zone,
						        bf_hdr2_ptr + bf_hdr2_offset,
						        sizeof(BF_Hdr_Image_Focal_Zone_Struct));
				        bf_hdr2_offset += sizeof(BF_Hdr_Image_Focal_Zone_Struct);
			        } // END loop over the focal zones 

			        if (bf_hdr_image.nNumBeams > MAX_NUM_BEAMS)
			        {
				        DEBUG_PRINT_2("Num Beams = %d\n",  bf_hdr_image.nNumBeams);
				        ERROR_MSG_EXIT("Error: number of beams exceeds max");
			        }

				    for (int ii = 0; ii < bf_hdr_image.nNumBeams; ii++)
				    { // Loop over the focal beams 
					    memcpy(&bf_hdr_beam,
							    bf_hdr2_ptr + bf_hdr2_offset,
							    sizeof(BF_Hdr_Beam_Struct));
					    bf_hdr2_offset += sizeof(BF_Hdr_Beam_Struct);

					    for (int jj = 0; jj < bf_hdr_image.nNumFocalZones; jj++)
					    { // Loop over the focal zones
						    memcpy(&bf_hdr_beam_focal_zone,
								    bf_hdr2_ptr + bf_hdr2_offset,
								    sizeof(BF_Hdr_Beam_Focal_Zone_Struct));
						    bf_hdr2_offset += sizeof(BF_Hdr_Beam_Focal_Zone_Struct);
					    } // END loop over the focal zones  
				    } // END loop over the beams  
			    } // END loop over the images  
		    } // END loop over the spatial phases  
				
            //free(bf_hdr2_ptr);
            }
            // NOTE:  Don't set got_ping = true because need to read another packet
	        break; // PKT_DATA_TYPE_BF_HDR
	        		           
        case PKT_DATA_TYPE_PING_HDR:
        {
            clog << "PKT_DATA_TYPE_PING_HDR" << endl;
            memset(&ping_header, 0, sizeof(Ping_Hdr_Struct));
            //memcpy(&ping_header, bfr_ptr + sizeof(Packet_Header_Struct), sizeof(Ping_Hdr_Struct));
            input_.read((char *)&ping_header, sizeof(ping_header));
            if ( !input_.good() ) return -1; // TODO: more sophisticated error checking

			if (ping_header.dwBFPacketID != bf_header.dwPacketID)
            {
                cerr << "BF Packet ID =  " << bf_header.dwPacketID << " vs. "
                 << ping_header.dwBFPacketID << endl;
                ERROR_MSG_EXIT("Beamformer Packet ID Mismatch!");
            }

			/* find TX Spatial Phase index */
			int PhaseSeqIndex = -1;
			for (int nn = 0; nn < bf_header.wNumTxSpatialPhases; nn++)
			{
				if (ping_header.nPhaseSeqIndex == TXSpatialPhaseIndex[nn])
				{
					PhaseSeqIndex = nn;
					break;
				}
			}

			if (PhaseSeqIndex < 0)
            {
                DEBUG_PRINT_2("bf_header.wNumTxSpatialPhases = ", bf_header.wNumTxSpatialPhases);
                ERROR_MSG_EXIT("Ping header iPhaseSeqIndex does not match TXSpatialPhaseIndex in BF header!");
            }

			num_bytes_ref_pulse = 0;   				
			for (int nn = 0; nn < NumImages[PhaseSeqIndex]; nn++)
			{
				num_bytes_ref_pulse += sizeof(Ipp32fc_Type)*(ping_header.dwReferencePulse[nn]); 
			}

            num_bytes_raw_data = sizeof(Ipp16sc_Type)*(ping_header.nNumRawSamples)*(bf_header.wNumElements);
            
            DEBUG_PRINT_2("Ping Header Version = ", ping_header.dwVersion);
            DEBUG_PRINT_2("num_bytes_ref_pulse = ", num_bytes_ref_pulse);
            DEBUG_PRINT_2("num_bytes_raw_data  = ", num_bytes_raw_data);
            DEBUG_PRINT_2("BF Packet ID        = ", ping_header.dwBFPacketID);
            DEBUG_PRINT_2("Ping Counter          ", ping_header.dwPingCounter);
            DEBUG_PRINT_2("Num Raw Samples     = ", ping_header.nNumRawSamples);
            DEBUG_PRINT_2("Num Elements        = ", bf_header.wNumElements);
            
            DEBUG_PRINT_2("PRF                 = ", ping_header.fPulseRepFreq);
            
//                 DEBUG_PRINT_4("Rotator Angles: %f %f %f\n", ping_header.fRotatorAngle[0], ping_header.fRotatorAngle[1], ping_header.fRotatorAngle[2]);
            
//                 DEBUG_PRINT_2("Compass            = %f\n", ping_header.fCompassHeading);
//                 DEBUG_PRINT_2("Magnetic Variation = %f\n", ping_header.fMagneticVariation);
            DEBUG_PRINT_2("Pitch              = ", ping_header.fPitch);
            DEBUG_PRINT_2("Roll               = ", ping_header.fRoll);
//                 DEBUG_PRINT_2("Depth              = %f\n", ping_header.fDepth);
//                 
//                 DEBUG_PRINT_2("Temperature        = %f\n", ping_header.fTemperature);
//                 DEBUG_PRINT_2("Latitude           = %f\n", ping_header.dbLatitude);
//                 DEBUG_PRINT_2("Longitude          = %f\n", ping_header.dbLongitude);
//                 DEBUG_PRINT_2("TXWST              = %f\n", ping_header.fTXWST);
//                 DEBUG_PRINT_2("Pulse Length       = %f\n", ping_header.dwPulseLength);
//                 
//                 DEBUG_PRINT_2("Internal Compass   = %f\n", ping_header.fInternalSensorHeading);
//                 DEBUG_PRINT_2("Internal Pitch     = %f\n", ping_header.fInternalSensorPitch);
//                 DEBUG_PRINT_2("Internal Roll      = %f\n", ping_header.fInternalSensorRoll);
//                 
//                 DEBUG_PRINT_2("Local Time Offset  = %f\n", ping_header.fLocalTimeOffset);
            
            //footer_offset = sizeof(Packet_Header_Struct) + sizeof(Ping_Hdr_Struct) + num_bytes_ref_pulse + num_bytes_raw_data;
            footer_offset = num_bytes_ref_pulse + num_bytes_raw_data;
            
            //curr_ping++;
            got_ping = true;
        }      
        break; // PKT_DATA_TYPE_PING_HDR
            
        case PKT_DATA_TYPE_GENERIC_HDR:
            if (sensor_data_search)
            {
                // look for optional generic sensor data packet associated with new ping data records
                DEBUG_PRINT("\ngeneric sensor data packet");
                memset(&generic_header, 0, sizeof(Generic_Hdr_Struct));
                //memcpy(&generic_header, bfr_ptr + sizeof(Packet_Header_Struct), sizeof(Generic_Hdr_Struct));
                input_.read((char *)&generic_header, sizeof(generic_header));
                if ( !input_.good() ) return -1; // TODO: more sophisticated error checking
				
                num_bytes_generic_data = packet_header.packet_body_size - sizeof(Generic_Hdr_Struct);
                if (num_bytes_generic_data != generic_header.wDataSize)
                {
                    DEBUG_PRINT("ERROR: packet & generic sensor data size mismatch");
                    DEBUG_PRINT_2("# generic data bytes declared in data hdr:  ", generic_header.wDataSize);
                    DEBUG_PRINT_2("# generic data bytes implied by packet hdr: ", num_bytes_generic_data);
                    //						ERROR_MSG_EXIT("Packet & Sensor data size mismatch!");
                }
				
                /* Extract the generic sensor data */
                //memcpy(generic_str, bfr_ptr + sizeof(Packet_Header_Struct) + sizeof(Generic_Hdr_Struct), num_bytes_generic_data);
                input_.read((char *)&generic_str, num_bytes_generic_data);
                if ( !input_.good() ) return -1; // TODO: more sophisticated error checking
                generic_str[num_bytes_generic_data] = 0;            // ensure null termination
                sensor_data_found = 1;
                DEBUG_PRINT_2("sensor str = ", generic_str);
            }
            
            /* Increment the buffer pointer to the next ping */
//            footer_offset = sizeof(Packet_Header_Struct) + packet_header.packet_body_size;
            footer_offset = 0;
            break;
            
        default:
            ERROR_MSG_EXIT( "Error: incorrect packet data type");
            break;

    } // switch (packet_header.data_type)
        
    /* Extract the footer and compare packet sizes */
    memset(&footer, 0, sizeof(Footer_Struct));
    //memcpy(&footer, bfr_ptr + footer_offset, sizeof(Footer_Struct));
        clog << "footer_offset = " << footer_offset << endl;
    input_.seekg(footer_offset, ios_base::cur);
    input_.read((char *)&footer, sizeof(footer));
    if ( !input_.good() ) return -1; // TODO: more sophisticated error checking

    DEBUG_PRINT_2("header.packet_body_size = ", packet_header.packet_body_size);
    DEBUG_PRINT_2("footer.packet_body_size = ", footer.packet_body_size);
    
    if (packet_header.packet_body_size != footer.packet_body_size)
    {
        DEBUG_PRINT_2("dwProcessingType = ", header.dwProcessingType);
        ERROR_MSG_EXIT("ERROR: packet size mismatch!");
    }
    } // while ( !got_ping )
    
    

    if ( !valid_packet(packet_header, header, bf_header, ping_header) )
    {
        ERROR_MSG_EXIT( "Error: Invalid packet");
    }
    clog << "Valid packet!" << endl;
    
    // Backup and read data.
    input_.seekg(-(num_bytes_ref_pulse + num_bytes_raw_data 
                 + sizeof(footer)),ios_base::cur);
    /* Extract the reference */
	clog << "extracting reference pulse" << endl;
    refPulse = (Ipp32fc_Type*)malloc(num_bytes_ref_pulse);
 //   memcpy(refPulse,
 //                  bfr_ptr + sizeof(Packet_Header_Struct) + sizeof(Header_Struct),
 //                  num_bytes_ref_pulse);
    input_.read((char *)refPulse, num_bytes_ref_pulse);
	clog << "reading raw data" << endl;
    rawData = (Ipp16sc_Type*)malloc(num_bytes_raw_data);
	if ( rawData == nullptr )
	{
		if ( refPulse != nullptr ) free(refPulse);
		ERROR_MSG_EXIT( "Error: Can't allocate memory for raw data.");
	}
    //memcpy(rawData,
    //       bfr_ptr + sizeof(Packet_Header_Struct) + sizeof(Ping_Hdr_Struct) + num_bytes_ref_pulse,
    //       num_bytes_raw_data);
	input_.read((char *)rawData, num_bytes_raw_data);
	clog << "advancing to next packet" << endl;
	// advance to the next packet
	input_.seekg(sizeof(Footer_Struct),ios_base::cur);
	
    // Beamform.
    /*
    int beamform(const RawData& raw_data, 
                 const Settings& settings, 
                 const Options& options,
                 ImageData* image_data, 
                 RangeList* range_list);
    */
    
    // Return the ping data in a device-independent structure.
    strncpy(pframe->header.device, "M3", 2);
    pframe->header.device[2] = '\0';
    
	/*
    pframe->malloc_data(num_bytes_raw_data);
    if (pframe->size() != num_bytes_raw_data)
    {
        ERROR_MSG_EXIT( "Error: Can't allocate memory for frame data.");
    }
*/
clog << "populating header" << endl;
    switch (packet_header.data_type)
    {
        case PKT_DATA_TYPE_OLD_HDR:
        {
            pframe->header.version = header.dwVersion;
            pframe->header.ping_num = header.dwPingCounter;
            pframe->header.ping_sec = header.dwTimeSec;
            pframe->header.ping_millisec = header.dwTimeMillisec;
            pframe->header.soundspeed_mps = header.velocitySound;
            pframe->header.num_samples = header.nNumRangeCells;
            pframe->header.range_min_m = header.fMinRangeMeter[0];
            pframe->header.range_max_m = header.fMaxRangeMeter[0];
            pframe->header.winstart_sec = 0;
            pframe->header.winlen_sec = 0;
            pframe->header.num_beams = header.nNumElements;
            //pframe->header.beam_angles_deg[kMaxBeams] = ;
            pframe->header.freq_hz = header.dwSubArrayFrequency[0];
            pframe->header.pulselen_microsec = header.dwPulseLength;
            pframe->header.pulserep_hz = header.fPulseRepFreq[0];
            
        }
            break;
        case PKT_DATA_TYPE_PING_HDR:
            pframe->header.version = ping_header.dwVersion;
            pframe->header.ping_num = ping_header.dwPingCounter;
            pframe->header.ping_sec = ping_header.dwTimeSec;
            pframe->header.ping_millisec = ping_header.dwTimeMillisec;
            pframe->header.soundspeed_mps = bf_header.fVSound;
            pframe->header.num_samples = ping_header.nNumRawSamples;
            pframe->header.range_min_m = ping_header.fMinRangeMeter[0];
            pframe->header.range_max_m = ping_header.fMaxRangeMeter[0];
            pframe->header.winstart_sec = 0;
            pframe->header.winlen_sec = 0;
            pframe->header.num_beams = bf_header.wNumElements;
            //pframe->header.beam_angles_deg[kMaxBeams] = 0;
            pframe->header.freq_hz = bf_hdr_image.dwSubArrayFrequency;
            pframe->header.pulselen_microsec = ping_header.dwPulseLength;
            pframe->header.pulserep_hz = ping_header.fPulseRepFreq;
    } // switch
    
	free(rawData);
	free(refPulse);
     return 0;
} //  DataSourceM3::GetPing

/*
//-----------------------------------------------------------------------------
// DataSourceM3::ReadPings
// read the specified number of pings
size_t DataSourceM3::ReadPings() 
{
    return 0;
    
} // DataSourceM3::ReadPings
*/

