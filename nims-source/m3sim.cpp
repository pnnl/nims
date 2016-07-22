//std
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>
#include <thread> // sleep_for()
#include <chrono> // time
//networking
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <bits/signum.h>
#include <arpa/inet.h>
#include <netdb.h>
// os
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

using namespace std;

#define esc 27

#if 1
	#define save_cursor printf("%c7", esc)
	#define restore_cursor printf("%c8",esc)

#else
	#define save_cursor
	#define restore_cursor
#endif

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

typedef struct
{
	Packet_Header_Struct header;
	Data_Header_Struct data_header;
	Ipp32fc_Type * data;
	Packet_Footer_Struct footer;
} frame;

void error_and_die(string msg)
{
	perror(msg.c_str());
	exit(1);
}

sig_atomic_t signaled = 0;
int sock = -1;
void inthandler (int param)
{
	signaled = 1;
	close(sock);
}

void * mbegin;
off_t mlen;
void maphandler(int param)
{
	signaled = 1;
	munmap(mbegin, mlen);
	printf("stopping server.\n");
}
void do_record(string hostaddr, int port, string filename)
{
	// signal handler to kill connection
	void (*prev_handler)(int);
	prev_handler = signal (SIGINT, inthandler);

	ofstream output;
	output.open(filename.c_str() );
	if (!output)
	{
		cout << "Error opening " << filename << endl;
		exit(1);
	}

	sockaddr_in m3;
	m3.sin_family = AF_INET;
	m3.sin_addr.s_addr = inet_addr(hostaddr.c_str());
	m3.sin_port = htons(port);
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (connect(sock, (struct sockaddr *) &m3, sizeof(struct sockaddr)))
	{
		cout << "Failed to connect to " << hostaddr << ":" << port << endl;
		exit(1);
	}

	cout << "Connected to " << hostaddr << " (" << port << ")" << endl << endl;
	cout << "Receiving: cntrl-c to finish and record" << endl;

	int count = 0;
	size_t current_malloc_size = 0;
	frame f; f.data = '\0';
	while (!signaled) // control-c toggles this
	{
		count++;

		ssize_t bytes_read = recv(sock, (char *)&f.header, sizeof(Packet_Header_Struct), MSG_WAITALL);
		if (f.header.sync_word_1 == 0x8000)
		{
			bytes_read += recv(sock, (char *)&f.data_header, sizeof(Data_Header_Struct),MSG_WAITALL);
			cout << count << ": Reading PingID: " << f.data_header.dwPingNumber << ": Expected (bytes): " << f.header.packet_body_size << endl;

			size_t data_size = sizeof(Ipp32fc_Type) * f.data_header.nNumBeams * f.data_header.nNumSamples;
			if (current_malloc_size < data_size)
			{
				f.data = (Ipp32fc_Type *)realloc(f.data, data_size);
				current_malloc_size = data_size;
			}
			if (f.data == '\0')
			{
				cout << endl << endl << "Error mallocing data - too much memory?" << endl;
				close(sock);
				exit(1);
			}
			bytes_read += recv(sock, (char *)f.data, data_size, MSG_WAITALL);
			recv(sock, (char *)&f.footer, sizeof(f.footer), MSG_WAITALL);
			cout  << " / Received (bytes): " << bytes_read << endl;
		}
		else if (signaled)
		{
			cout << endl << endl << "Closing Connection to M3 ..." << endl << endl;
			close(sock);
		}
		else if (bytes_read <= 0)
		{
			cout << endl << "Remote Server Terminated Connection." << endl;
			break;
		}
		else
		{
			cout << endl << "Error reading frame " << count << "(skipping)";
			string temp;
		}
		cout << "Writing Ping ID: " << f.data_header.dwPingNumber << endl;
		output.write((char *)&(f.header), sizeof(Packet_Header_Struct));
		output.write((char *)&(f.data_header), sizeof(Data_Header_Struct));
		size_t dlen = f.data_header.nNumBeams * f.data_header.nNumSamples * sizeof(Ipp32fc_Type);
		output.write((char *)f.data, dlen);
		output.write((char *)&f.footer, sizeof(Packet_Footer_Struct));

	}
	free(f.data);
	output.close();
	cout << endl << endl << "Recording complete." << endl;
}

vector<size_t> index_map()
{
	vector<size_t> index;
	size_t pos = 0;
	size_t hdrsize=sizeof(Packet_Header_Struct) + sizeof(Packet_Footer_Struct) + sizeof(Data_Header_Struct);
	if (mbegin == '\0')
		return index;
	char * curp = (char *)mbegin;
	frame * f;
	while (pos < mlen)
	{
		f = (frame *)curp;
		if (f->header.sync_word_1 != 0x8000)
		{
			printf("Bad Syncword (skipping)\n");
			continue;
		}
		index.push_back(pos);
		size_t datasize = f->data_header.nNumSamples * f->data_header.nNumBeams * sizeof(Ipp32fc_Type);
		pos+=datasize+hdrsize;
		//curp += (datasize + hdrsize);
	}

	cout << "Indexed " << index.size() << " pings." << endl;
	return index;
}

void do_replay(string host, int port, string filename, float rate, bool loop)
{
	struct stat sb;
	int fd = open(filename.c_str(), O_RDONLY);
	if (fd == -1)
		error_and_die("error opening file");
	if (fstat(fd, &sb) == -1)
		error_and_die("could not retrieve file properties");
	mlen = sb.st_size;
	mbegin = mmap(0, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if ((int *)mbegin ==(int*)-1)
		perror("mmap");
	vector<size_t> index = index_map();
	close(fd);

	int parentfd;
	int childfd;

	int clientlen;
	struct sockaddr_in serveraddr;
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	char *hostaddrp;
	int optval;
	int n;

	printf("standing up server ...\n");
	parentfd = socket(AF_INET, SOCK_STREAM, 0);
	if (parentfd < 0)
		error_and_die("error: opening socket");
	optval = 1;
	setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR,
			   (const void *)&optval , sizeof(int));
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);
	if (bind(parentfd, (struct sockaddr *) &serveraddr,
			 sizeof(serveraddr)) < 0)
		error_and_die("error: on binding");
	if (listen(parentfd, 5) < 0) /* allow 5 requests to queue up */
		error_and_die("error: on listen");

	clientlen = sizeof(clientaddr);

	childfd = accept(parentfd, (struct sockaddr *) &clientaddr, (socklen_t *) &clientlen);
	if (childfd < 0)
		error_and_die("error: on accept");
	hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
						  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	if (hostp == NULL)
		error_and_die("error: on gethostbyaddr");
	hostaddrp = inet_ntoa(clientaddr.sin_addr);
	if (hostaddrp == NULL)
		error_and_die("error: on inet_ntoa\n");
	printf("server established connection with %s (%s)\n",
		   hostp->h_name, hostaddrp);



	int last_ping = 1000;
	char * cur_p = (char *)mbegin;
	char * base_p = (char *)mbegin;
	frame * f;
	int hdrsize=sizeof(Packet_Header_Struct) + sizeof(Packet_Footer_Struct) + sizeof(Data_Header_Struct);

	// NOTE:  sleep takes an integer number of seconds so this
	//        float gets truncated to 0.
	//float tosleep = 1.0 / rate;
	long int tosleep = (1.0 / rate)*1000;

	while (1)
	{
		if (last_ping > 1000000)
		{
			last_ping = 1000;
		}
		for (int i = 0; i < index.size(); i++)
		{
			printf("-- pingid: %d\n", last_ping);
			cur_p = base_p + index[i];
			f = (frame *) cur_p;
			last_ping++;
			if (loop) f->data_header.dwPingNumber = last_ping;
			int dsize = f->data_header.nNumSamples * f->data_header.nNumBeams * sizeof(Ipp32fc_Type);
			int n = write(childfd, (char *) f, dsize + hdrsize);
			if (n <= 0)
				error_and_die("Error Writing to Socket.");
			//sleep(tosleep);
			this_thread::sleep_for(chrono::milliseconds(tosleep));

		}

		if (!loop) break;

		for (int i = 0; i < index.size(); i++)
		{
			printf("-- pingid: %d\n", last_ping);
			cur_p = base_p + index[index.size()-1-i];
			f = (frame *) cur_p;
			f->data_header.dwPingNumber = last_ping++;
			int dsize = f->data_header.nNumSamples * f->data_header.nNumBeams * sizeof(Ipp32fc_Type);
			int n = write(childfd, (char *) f, dsize + hdrsize);
			if (n <= 0)
				error_and_die("Error Writing to Socket.");
			//sleep(tosleep);
			this_thread::sleep_for(chrono::milliseconds(tosleep));
		}

	}


}

void usage_and_die()
{
	cout << "Usage: m3sim -m [ record | serve ] -r hz -h hostaddr -p port -f filename -l " << endl;
	exit(1);
}

int main(int argc, char * argv[]) {

	string mode;
	string filename;
	string hostaddr;
	int port = 20001;
	float rate = 1;
	bool loop = false;
	//opterr = 0;
	int c;
	printf("\n");
	while ((c = getopt (argc, argv, "m:p:f:h:r:l:")) != -1)
	{

		switch (c)
		{
			case 'm':
				mode = optarg;
				if (!mode.compare("record") && !mode.compare("replay")) {
					cout << "Unrecognized mode." << endl;
					usage_and_die();
				}
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'h':
				hostaddr = optarg;
				break;
			case 'f':
				filename = optarg;
				break;
			case 'r':
				rate = atof(optarg);
				break;
				case 'l':
				loop = true;
			case '?':
				printf("? %s\n", optarg);
				usage_and_die();
				break;
			default:
				abort();

		}
	}

	if (!mode.compare("record"))
		do_record(hostaddr, port, filename);

	else
		do_replay(hostaddr, port, filename, rate, loop);

    return 0;
}
