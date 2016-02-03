/* 	AUTOMATICALLY GENERATED  -- DO NOT EDIT -- */
#ifndef __BVT_RETVAL_H__93567834567
#define __BVT_RETVAL_H__93567834567


/** \ingroup BVTError
 * Define our error code return type
*/
typedef int RetVal;

/** \addtogroup BVTError
 *  @{
 */

/** Invalid error code\*/
#define BVT_INVALID_ERROR -1

/** Success*/
#define BVT_SUCCESS 0

/** Failure*/
#define BVT_FAILED 1

/** Failed to allocate memory.*/
#define BVT_MEMORY_ALLOC 2

/** Failed to open file.*/
#define BVT_FILE_OPEN 3

/** Failed to read from a file.*/
#define BVT_FILE_READ 4

/** Failed to write to a file.*/
#define BVT_FILE_WRITE 5

/** A function parameter was incorrect.*/
#define BVT_BAD_PARAMETER 6

/** A pointer was invalid.*/
#define BVT_INVALID_PTR 7

/** Can't create the file specified.*/
#define BVT_FILE_CREATE 8

/** Couldn't seek.*/
#define BVT_FILE_SEEK 9

/** A file isn't open.*/
#define BVT_FILE_NOTOPEN 10

/** The file already exists.*/
#define BVT_FILE_EXISTS 11

/** Couldn't get a file's information.*/
#define BVT_FILE_STAT 12

/** The function is not implemented.*/
#define BVT_NOT_IMPLEMENTED 13

/** Failed to create a directory.*/
#define BVT_MKDIR_FAILED 14

/** A command line parameter is incorrect*/
#define BVT_BAD_CMDLINE 15

/** Successful program exit flag*/
#define BVT_SUCCESS_EXIT 16

/** The user aborted the action*/
#define BVT_USER_ABORT 17

/** Timeout*/
#define BVT_TIMEOUT 18

/** The buffer was too small*/
#define BVT_BUFFER_TOO_SMALL 19

/** This trial SDK has expired.*/
#define BVT_SDK_EXPIRED 20

/** The specified offset is too large.*/
#define BVT_BAD_OFFSET 1000

/** The buffer is too short.*/
#define BVT_BUFFER_TOO_SHORT 1001

/** The file's header is too short.*/
#define BVT_FILE_HEADER_SHORT 1002

/** The file's header is bad.*/
#define BVT_FILE_HEADER_BAD 1003

/** The file's version is too new.*/
#define BVT_FILE_TOO_NEW 1004

/** A file block allocation failed.*/
#define BVT_BLOCK_ALLOC 1005

/** The file has reached it's max block count.*/
#define BVT_MAX_BLOCKCOUNT 1006

/** Tried to write too much data to a block.*/
#define BVT_WRITE_TOO_BIG 1007

/** The file's blocksize is not valid.*/
#define BVT_INVALID_BLOCKSIZE 1008

/** The file's max block count is not valid.*/
#define BVT_INVALID_MAXBLOCKCOUNT 1009

/** zlib compression failure.*/
#define BVT_ZLIB_COMPRESS 1010

/** zlib uncompression failure.*/
#define BVT_ZLIB_UNCOMPRESS 1011

/** Couldn't acquire a lock.*/
#define BVT_CANT_LOCK 1012

/** Couldn't create a socket.*/
#define BVT_SOCKET_CREATE 1013

/** DNS failure.*/
#define BVT_LOOKUP_HOST 1014

/** Connection timed out.*/
#define BVT_CONNECTION_TIMEDOUT 1015

/** Couldn't bind to a socket.*/
#define BVT_SOCKET_BIND 1016

/** Socket read timeout.*/
#define BVT_SOCKET_READ_TIMEOUT 1017

/** Socket write timeout.*/
#define BVT_SOCKET_WRITE_TIMEOUT 1018

/** Invalid socket.*/
#define BVT_BAD_SOCKET 1019

/** The socket was closed during an operation.*/
#define BVT_SOCKET_CLOSED 1020

/** Socket write failure.*/
#define BVT_SOCKET_WRITE 1021

/** Socket read failure.*/
#define BVT_SOCKET_READ 1022

/** setsockopt failed.*/
#define BVT_SOCKET_SETSOCKOPT 1023

/** Failed to create the logger.*/
#define BVT_LOGGER_CREATE 1024

/** Couldn't create a thread.*/
#define BVT_THREAD_START 1025

/** The specified alarm didn't exist.*/
#define BVT_ALARM_EXIST 1026

/** Invalid units.*/
#define BVT_BAD_UNITS 1027

/** Unit dimensions don't match.*/
#define BVT_INVALID_DIMENSION 1028

/** The variant's data type doesn't match the request.*/
#define BVT_BAD_DATATYPE 1029

/** The data queue is empty.*/
#define BVT_SER_EMPTY_QUEUE 1030

/** The data queue is full.*/
#define BVT_SER_FULL_QUEUE 1031

/** Couldn't open the port.*/
#define BVT_SER_BAD_PORT 1032

/** SetupComm failed.*/
#define BVT_SER_SETUP_COMM 1033

/** PurgeComm failed.*/
#define BVT_SER_PURGE_COMM 1034

/** SetCommTimeouts failed.*/
#define BVT_SER_COMM_TIMEOUTS 1035

/** Get/SetCommState failed.*/
#define BVT_SER_COMM_STATE 1036

/** GetOverlappedResult failed.*/
#define BVT_SER_WIN32_POLL 1037

/** Read failed.*/
#define BVT_SER_READ 1038

/** Write failed.*/
#define BVT_SER_WRITE 1039

/** Flow Control setting not recognized.*/
#define BVT_SER_INV_FLOW 1040

/** Data bit setting not recognized.*/
#define BVT_SER_INV_DATA 1041

/** Parity setting not recognized.*/
#define BVT_SER_INV_PAR 1042

/** Stop bit setting not recognized.*/
#define BVT_SER_INV_STOP 1043

/** Baud rate not recognized.*/
#define BVT_SER_INV_BAUD 1044

/** Undefined configuration error.*/
#define BVT_SER_INV_CONFIG 1045

/** Listen failed.*/
#define BVT_SOCKET_LISTEN 1046

/** The serial port isn't open.*/
#define BVT_SER_NOTOPEN 1047

/** Invalid unit type*/
#define BVT_BAD_UNITTYPE 1048

/** Function not supported for this protocol*/
#define BVT_WRONG_PROTOCOL 1049

/** No data returned on socket read.*/
#define BVT_EMPTY 1059

/** Pipe shutdown request was received.*/
#define BVT_SHUTDOWN 1060

/** The thread is already started*/
#define BVT_ALREADY_STARTED 1061

/** The queue is full*/
#define BVT_QUEUE_FULL 1062

/** The queue is empty*/
#define BVT_QUEUE_EMPTY 1063

/** The specified index is out of bounds*/
#define BVT_INDEX_OUTOFBOUNDS 1064

/** The socket isn't connected*/
#define BVT_SOCKET_NOT_CONNECTED 1065

/** The socket operation would block*/
#define BVT_SOCKET_WOULD_BLOCK 1066

/** A string was too long.*/
#define BVT_STRING_TOO_LONG 1067

/** Bad pulse signature*/
#define BVT_PULSE_SIGNATURE_BAD 2000

/** Bad pulse compression type*/
#define BVT_PULSE_BAD_COMPTYPE 2001

/** Invalid sonar type*/
#define BVT_BAD_SONAR_TYPE 2002

/** Invalid parent*/
#define BVT_BAD_PARENT 2003

/** Invalid ImageBuilder*/
#define BVT_BAD_IMAGE_BUILDER 2004

/** Can't find the requested listener*/
#define BVT_LISTENER_NOT_FOUND 2005

/** Sonar XML parse error*/
#define BVT_SON_XML_PARSE 2006

/** Bad transducer number*/
#define BVT_BAD_TRANSDUCER 2007

/** This device doesn't support temperature reading*/
#define BVT_NO_TEMP 2008

/** This device doesn't support tilt sensing*/
#define BVT_NO_TILT 2009

/** The sonar lacks a head.*/
#define BVT_NO_HEAD 2010

/** The head lacks a transducer.*/
#define BVT_NO_TRANSDUCER 2011

/** Invalid BeamFormer*/
#define BVT_BAD_BEAM_FORMER 2012

/** Invalid sonar*/
#define BVT_BAD_SONAR 2013

/** The match filter could not be perfomed, perhaps because there was no pulse data.*/
#define BVT_MATCH_FILTER_FAILURE 2014

/** The specified normalization is invalid*/
#define BVT_INVALID_NORMALIZATION 2015

/** The number of samples for a signal is invalid*/
#define BVT_INVALID_SIGNAL_LENGTH 2016

/** A V1 file's header signature is bad.*/
#define BVT_V1_SIGNATURE_BAD 4001

/** The specified ping number is too large.*/
#define BVT_PING_NUM_TOO_LARGE 4002

/** The specified ping number is too small.*/
#define BVT_PING_NUM_TOO_SMALL 4003

/** A record was not of the expected type.*/
#define BVT_WRONG_RECORD_TYPE 4004

/** The file didn't contain a settings record.*/
#define BVT_NO_SETTINGS_RECORD 4005

/** The file is missing a head record.*/
#define BVT_NO_HEAD_RECORD 4006

/** Unsupported V2 index type.*/
#define BVT_V2_BAD_INDEX 4007

/** The specified path is a file, not a directory.*/
#define BVT_NOT_DIR 4008

/** The specfified range is invalid for this sonar*/
#define BVT_BAD_RANGE 4009

/** The file contains an unknown record type*/
#define BVT_UNKNOWN_RECORD_TYPE 4010

/** The ping chain didn't have any signal records*/
#define BVT_NO_PING_SIGNAL 4011

/** The file is marked readonly*/
#define BVT_READONLY_FILE 4012

/** Invalid compression*/
#define BVT_V2_BAD_COMPRESSION 4013

/** No video frame stored for this ping*/
#define BVT_NO_VIDEO_FRAME 4014

/** The video format stored was not recognized*/
#define BVT_INVALID_VIDEO_FORMAT 4015

/** The ping has no stored profiles*/
#define BVT_NO_STORED_PROFILES 4016

/** The head count is invalid for this type of file*/
#define BVT_INVALID_HEAD_COUNT 4017

/** Head does not match file creation head*/
#define BVT_INVALID_HEAD 4018

/** The heads are inelibible for concatenation*/
#define BVT_MISMATCHED_HEAD_PAIR 4019

/** The polar map is invalid.*/
#define BVT_BAD_MAP 5000

/** The ping is invalid.*/
#define BVT_BAD_PING 5001

/** The color map is invalid.*/
#define BVT_BAD_COLOR_MAP 5002

/** There is no head to process.*/
#define BVT_BAD_HEAD 5003

/** Invalid image type.*/
#define BVT_BAD_IMAGE_TYPE 5004

/** The head lacks transducers*/
#define BVT_IMG_NO_TRANSDUCERS 5005

/** The range policy is invalid.*/
#define BVT_BAD_RANGE_POLICY 5006

/** The transducer orientation definitions are too old for accurate imagery.*/
#define BVT_INACCURATE_TRANSDUCERS 5007

/** The requested image size is not valid*/
#define BVT_INVALID_IMG_SIZE 5008

/** The head has too many transducers*/
#define BVT_INVALID_TXDR_COUNT 5009

/** The specified resolution is too high*/
#define BVT_INVALID_RESOLUTION 5010

/** The median filter is too large*/
#define BVT_MEDIAN_FILTER_BIG 7000

/** There is too little data for the operation*/
#define BVT_NOT_ENOUGH_DATA 7001

/** There is no sonar device.*/
#define BVT_PV_NO_SONAR 9000

/** Failed to load the colormap.*/
#define BVT_PV_LOAD_COLORMAP 9001

/** Failed to load the file.*/
#define BVT_PV_LOAD_FILE 9002

/** Invalid rotationSpeed parameter.*/
#define BVT_PT_BAD_SPEED 10000

/** Invalid brakeLevel parameter.*/
#define BVT_PT_BAD_BRAKE 10001

/** Invalid angle parameter.*/
#define BVT_PT_BAD_ANGLE 10002

/** Node already exists.*/
#define BVT_PT_NODE_EXISTS 10003

/** Node not responding.*/
#define BVT_PT_NODE_NOT_FOUND 10004

/** Invalid node ID.*/
#define BVT_PT_INVALID_NODE 10005

/** Got response from an incorrect node*/
#define BVT_PT_WRONG_NODE 10006

/** The response wasn't the right length*/
#define BVT_PT_WRONG_LEN 10007

/** There was a collision on the RS485 bus*/
#define BVT_PT_BUS_COL 10008

/** Could not initialize the horizontal axis.*/
#define BVT_PT_HORIZ_AXIS_FAIL 10009

/** Could not initialize the vertical axis.*/
#define BVT_PT_VERT_AXIS_FAIL 10010

/** Could not find the horizontal node.*/
#define BVT_PT_NO_HORIZ 10011

/** Could not find the vertical node.*/
#define BVT_PT_NO_VERT 10012

/** Can't use the same node for two axes*/
#define BVT_PT_DUP_NODES 10013

/** Firmware version too old.*/
#define BVT_PT_OLD_FIRMWARE 10014

/** PanTilt did not echo back bytes*/
#define BVT_PT_ECHO_FAILURE 10015

/** PanTilt response was not of the expected format*/
#define BVT_PT_INV_RESPONSE_FORMAT 10016

/** PanTilt command would have been interrupted*/
#define BVT_PT_CMD_IN_PROGRESS 10017

/** PanTilt is not connected*/
#define BVT_PT_NOT_CONNECTED 10018

/** Invalid sonar setting.*/
#define BVT_ST_INVALID_SETTING 11000

/** Invalid transducer orientation.*/
#define BVT_ST_INVALID_ORIENTATION 11001

/** The sonar number was invalid.*/
#define BVT_ST_BAD_SONAR_NUM 11002

/** Failed to create the AVI file*/
#define BVT_AVI_CREATE_FILE 12000

/** The stream has already been compressed, can't recompress*/
#define BVT_AVI_ALREADY_COMP 12001

/** Failed to create an AVI stream*/
#define BVT_AVI_CREATE_STREAM 12002

/** Failed to set the stream format*/
#define BVT_AVI_SET_FORMAT 12003

/** Couldn't write a AVI frame*/
#define BVT_AVI_WRITE_FRAME 12004

/** CreateDIBSection failed*/
#define BVT_AVI_CREATE_DIB 12005

/** Unable to initialize the OLE subsystem needed.*/
#define BVT_V_OLE_FAILURE 13000

/** Errors communicating with the DirectShow subsytem, most likely a Windows configuration conflict.*/
#define BVT_V_DSHOW_ERROR 13001

/** Unable to find a video capture device. Please be sure yours is installed and connected.*/
#define BVT_V_NO_VIDEO_CAPTURE 13002

/** Unable to use the video capture device, please make sure it's connected and not in use by another application.*/
#define BVT_V_CANT_USE_VIDEO 13003

/** Unable to start the video playing. Exiting and restarting your application may correct this problem.*/
#define BVT_V_DSHOW_NOPLAY 13004

/** Unable to set the video format*/
#define BVT_V_VID_FORMAT 13005

/** Incorrect packet size*/
#define BVT_NET_BAD_PACKET_SIZE 14000

/** Incorrect packet type*/
#define BVT_NET_BAD_PACKET_TYPE 14001

/** Network XML Parse error*/
#define BVT_NET_XML_PARSE 14002

/** Not connected to the server*/
#define BVT_NET_NOT_CONNECTED 14003

/** Unknown packet version*/
#define BVT_NET_BAD_PACKET_VER 14004

/** Invalid Network Settings*/
#define BVT_NET_BAD_NETSETTINGS 14005

/** Not connected to a NetDAQ device*/
#define BVT_NDAQ_NOT_OPEN 15000

/** Failed to write to a NetDAQ register*/
#define BVT_NDAQ_SET_REG 15001

/** Failed to read from a NetDAQ register*/
#define BVT_NDAQ_GET_REG 15002

/** Failed to write to NetDAQ memory*/
#define BVT_NDAQ_MEM_WRITE 15003

/** Failed to read from NetDAQ memory*/
#define BVT_NDAQ_MEM_READ 15004

/** Failed to allocate NetDAQ memory*/
#define BVT_NDAQ_MEM_ALLOC 15005

/** Failed NetDAQ memory check*/
#define BVT_NDAQ_MEM_TEST 15006

/** Incorrect FPGA ID*/
#define BVT_NDAQ_BAD_ID 15007

/** mmap call failed*/
#define BVT_NDAQ_MMAP 15008

/** Config data too large*/
#define BVT_NDAQ_CFG_TOO_LARGE 15009

/** Config erase failed*/
#define BVT_NDAQ_CFG_ERASE 15010

/** Ping Holdoff Signal Timeout*/
#define BVT_NDAQ_HOLDOFF_TIMEOUT 15011

/** Unknown base txdr for a virtual txdr*/
#define BVT_NDAQ_BAD_BASE_TXDR 15012

/** Invalid master port for a head*/
#define BVT_NDAQ_MASTER_PORT_ERR 15013

/** No nav data stored in this field*/
#define BVT_NAV_NO_DATA 16000

/** NMEA input is disabled*/
#define BVT_NMEA_INPUT_DISABLED 17000

/** NMEA output is disabled*/
#define BVT_NMEA_OUTUT_DISABLED 17001

/** NMEA Server is uninitialized*/
#define BVT_NMEA_SRV_UNINIT 17002

/** NMEA is improperly formatted*/
#define BVT_NMEA_INVALID 17003

/** Unknown NMEA ID*/
#define BVT_NMEA_ID_UNKNOWN 17004

/** External trigger not available for this type of sonar*/
#define BVT_TRIGGER_NOT_PRESENT 18000

/** Trigger options are invalid or unavailable for this sonar*/
#define BVT_TRIG_OPTIONS_INVALID 18001

/** Invalid request for an input-configured hardware trigger*/
#define BVT_TRIGGER_IS_INPUT 18002

/** Invalid request for an output-configured hardware trigger*/
#define BVT_TRIGGER_IS_OUTPUT 18003

/** Proviewer interface is uninitialized*/
#define BVT_PV_IF_UNINIT 19000

/** Invalid message from Proviewer*/
#define BVT_PV_IF_BAD_MSG 19001

/** Invalid config. file*/
#define BVT_FEAT_BAD_CFG 20000

/** Unable to export to feature config. file*/
#define BVT_FEAT_EXPORT_FAILURE 20001

/** Invalid XML node*/
#define BVT_FEAT_INVALID_NODE 20002

/** @}*/


#endif

