#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

typedef struct {
    uint32_t dwValue;     // Starting at byte offset 0, DWORD (32 bits)

    uint32_t CC : 4;      // 4 bits
    uint32_t X : 1;       // 1 bit
    uint32_t P : 1;       // 1 bit, immediately following the V field
    uint32_t V : 2;       // 2 bits, assumed to start at byte offset 4

    uint32_t PT : 7;      // 7 bits
    uint32_t M : 1;       // 1 bit

    uint16_t sequenceNum; // WORD (16 bits), starting at byte offset 6

    uint8_t SIM[6];       // BCD format, starting at byte offset 8, total 6 bytes
    uint8_t byteValue;    // BYTE, starting at byte offset 14

    uint8_t segmentType : 4;    // another 4 bits, these names are placeholders since the actual purpose is not clear
    uint8_t dataType : 4;    // 4 bits

    uint8_t timestamp[8];     // BYTE[8], starting at byte offset 16

    uint16_t lastIFrameInterval; // WORD (16 bits), starting at byte offset 24
    uint16_t lastFrameInterval;  // WORD (16 bits), starting at byte offset 26

    uint16_t wordValue;         // WORD, starting at byte offset 28
} JTT1078Header;


unsigned long long getMillisecTime()
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("Failed to get time");
        return 0;
    }

    // Convert time to milliseconds
    unsigned long long currentTimeMillis = (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
    
    //printf("Current time in milliseconds: %llu\n", currentTimeMillis);
    return currentTimeMillis;
}

unsigned long long htonll(unsigned long long original)
{
    unsigned long long bigEndianValue = 0;

    for (int i = 0; i < sizeof(unsigned long long); i++) {
        bigEndianValue = (bigEndianValue << 8) | ((original >> (i * 8)) & 0xFF);
    }
    return bigEndianValue;
}

uint8_t* readNALUnit(FILE *fp, size_t *nalSize) {
    uint8_t buf[4];
    size_t nalStart = 0, nalEnd = 0;
    int zeros = 0;
    *nalSize = 0;

    while (fread(buf, 1, 1, fp)) {
        if (buf[0] == 0x00) {
            zeros++;
        } else if (buf[0] == 0x01 && zeros >= 2) {
            if (nalStart == 0) {
                nalStart = ftell(fp);
            } else {
                nalEnd = ftell(fp) - 2/*zeros*/ - 1;
                break;
            }
            zeros = 0;
        } else {
            zeros = 0;
        }
    }

	if(nalStart == 0)
		return NULL;

    if (nalEnd == 0) {
        nalEnd = ftell(fp);
    }

    *nalSize = nalEnd - nalStart;
    //printf("NAL size:%u.\n", *nalSize);
    uint8_t* nalData = (uint8_t*)malloc(*nalSize);
    if (!nalData) {
        perror("Failed to allocate memory for NAL unit");
        return NULL;
    }

    fseek(fp, nalStart, SEEK_SET);
    fread(nalData, 1, *nalSize, fp);

    return nalData;
}

uint8_t* readNaluSet(FILE *fp, size_t *nalSize, int* tp)
{
    size_t videoSize;
    uint8_t* videoData;
    int bNalTp = -1;
	uint8_t* frameSetsData;
	size_t frameSetsSize;

	frameSetsData = (uint8_t*)malloc(1024*1024);
	frameSetsSize = 0;
	while (true) {
    	videoData = readNALUnit(fp, &videoSize);
    	uint8_t* videoDataStart = videoData;
    	if (videoData == NULL || videoSize == 0) {
			break;
    	}
		memset(frameSetsData, 0, 3);
		frameSetsData[frameSetsSize + 3] = 0x01;
		frameSetsSize += 4;
    	memcpy(frameSetsData + frameSetsSize, videoData, videoSize);
    	frameSetsSize+= videoSize;
   		uint8_t nalType = videoData[0] & 0x1F;
    	free(videoData);
    	/*
    	NAL:
    		1: Coded slice
			5: IDR (Instantaneous Decoder Refresh) slice
			6: SEI (Supplemental Enhancement Information)
			7: SPS (Sequence Parameter Set)
			8: PPS (Picture Parameter Set)
		*/
		if(nalType == 5 || nalType == 1){
			bNalTp = nalType;
			break;
		}
    }
    *nalSize = frameSetsSize;
    *tp = bNalTp;
    return frameSetsData;
}

void fillJTT1078HeaderPlaceholders(JTT1078Header *header) {
    if (!header) {
        return;
    }

    // Fill the struct with mock data
    header->dwValue = 0x64633130; // Some mock DWORD value
    header->V = 2;
    header->P = 0;
    header->X = 0;
    header->CC = 1; // Mock value, adjust as per your requirement
    header->PT = 98; // Some mock value, adjust accordingly
}

void fillJTT1078Header(JTT1078Header *header, uint16_t rtpPkgIdx, uint8_t fregmentIdx, bool bLastFregment, uint8_t nalType) {
    if (!header) {
        return;
    }

    header->M = bLastFregment ? 1 : 0;
    header->sequenceNum = htons(rtpPkgIdx); // Mock sequence number
    // Fill SIM with mock BCD data 12345678901
    uint8_t mockSIM[6] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0x01 };
    memcpy(header->SIM, mockSIM, 6);

    header->byteValue = 0x01; // Some mock byte value
    if(nalType == 5){
    	header->dataType = 0x0; // Mock values
    	//printf("IIIIIIIII frame\n");
    }
    else if(nalType == 1)
    	header->dataType = 0x1;
    else
    	header->dataType = 0x0;//l.huo

    if(fregmentIdx == 0)
    	header->segmentType = 0x1;
	else if(bLastFregment)
		header->segmentType = 0x2;
	else
		header->segmentType = 0x3;
}

int sendJTT1078Stream(const char *ip, int port) {
    int sockfd;
    struct sockaddr_in server_addr;
    JTT1078Header header;

	fillJTT1078HeaderPlaceholders(&header);
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(sockfd);
        return -1;
    }

	// Open input file
    FILE *fp = fopen("video.h264", "rb");
    if (!fp) {
        perror("Failed to open file");
        return -1;
    }


	unsigned long long timeIframe = 0, timeFrame, timeLastFrame;
	uint16_t rtpPkgIdx = 0;
	uint8_t byteData[30 + 950]; // BYTE[n], n = 950 bytes, starting at byte offset 30
    // Loop to continuously send data
    while (1) {
    	uint8_t fregmentIdx = 0;
    	bool bIframe;
    	//Read one video frame
        size_t videoSize;
        int nalType;
    	uint8_t* videoData = readNaluSet(fp, &videoSize, &nalType);
    	uint8_t* videoDataStart = videoData;
    	if (videoData == NULL || videoSize == 0 || nalType < 0) {
        	printf("Read NAL unit of size: %zu bytes\n", videoSize);
			break;
    	}
    	timeLastFrame = timeFrame;
    	timeFrame = getMillisecTime();

    	bIframe = nalType == 5 || nalType == 7 || nalType == 8;
    	if(bIframe)
    		timeIframe = getMillisecTime();
        //printf(">>type %u\n", videoData[0] & 0x1F);
		do{
			size_t segmentSize = videoSize > 950 ? 950 : videoSize;
			videoSize -= segmentSize;
        	// Fill the header with data...
        	fillJTT1078Header(&header, rtpPkgIdx, fregmentIdx, videoSize > 0 ? false : true, nalType);
        	unsigned long long bigEndianTimeFrame = htonll(timeFrame);
    		memcpy(header.timestamp, &bigEndianTimeFrame, 8);

    		header.lastIFrameInterval = timeIframe == 0 ? 0 : getMillisecTime() - timeIframe; // Mock value
    		header.lastIFrameInterval = htons(header.lastIFrameInterval);
    		header.lastFrameInterval = timeFrame - timeLastFrame;
    		header.lastFrameInterval = htons(header.lastFrameInterval);
    		header.wordValue = segmentSize;
    		header.wordValue = htons(header.wordValue);
    		// Fill the byteData
        	memcpy(byteData, &header, 30);
			memcpy(byteData + 30, videoData, segmentSize);
        	// Send the byteData
        	ssize_t bytes_sent = send(sockfd, byteData, 30 + segmentSize, 0);
        	if (bytes_sent == -1) {
            	perror("Sending failed");
            	break;
        	}
        	else if(bytes_sent != 30 + segmentSize)
        		printf("sent error ...\n");
/*
for(int i = 0; i < 40 && i < segmentSize + 30; i++)
	printf(" %02x", byteData[i]);
printf("\n");
*/
        	rtpPkgIdx++;
        	fregmentIdx++;
        	
        	videoData += segmentSize;
        }while(videoSize > 0);

		free(videoDataStart);
        // Use a delay or a condition to control the loop, if required
        usleep(60*1000);
    }

    close(sockfd);
    fclose(fp);
    return 0;
}

int main2() {
    sendJTT1078Stream("127.0.0.1", 0x1a92);
    return 0;
}
