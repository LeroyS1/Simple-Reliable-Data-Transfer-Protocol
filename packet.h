
#include <stdio.h>
#include <inttypes.h>
#define PAYLOAD 512
struct packet
{
     uint16_t seqNum;
     uint16_t nextSeqNum;
     uint16_t ackNum;
     uint16_t ackFlag;
     uint16_t synFlag;
     uint16_t finFlag;
     uint16_t dataSize;
     char data[PAYLOAD];
};

struct window
{
     struct packet pkt;
     struct timespec start_time;
     int isAcked;
};