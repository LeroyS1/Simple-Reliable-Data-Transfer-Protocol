//
// Created by Leroy Le on 5/9/20.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <netdb.h>
#include <time.h>

#include "packet.h"
#define BILLION 1E9
#define PACKAGE_SIZE 524
#define PAYLOAD 512
#define MAX_SEQ_NUM 25600
#define WINDOW_SIZE 10
#define MAX_FILE_SIZE 20000 // 10MB

static struct window wind[MAX_FILE_SIZE];

int main(int argc, char *argv[])
{
    int UDPsockFd, byteSend = 0, byteRecv = 0;
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    socklen_t addr_len;
    int port;
    int opt = 1;
    char *hostname;
    char *filename;
    struct hostent *host;
    //packet variables
    struct packet packetRecv;
    struct packet packetSend;

    uint16_t currSeq;
    uint16_t currAck;

    fd_set rfds;
    int ntime = 0;
    struct timespec start, stop;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;
    srand(time(NULL));

    //file variables
    FILE *fd;
    if (argc != 4)
    {
        fprintf(stderr, "Error!!!");
        exit(EXIT_FAILURE);
    }

    hostname = argv[1];
    port = atoi(argv[2]);
    filename = argv[3];
    UDPsockFd = socket(AF_INET, SOCK_DGRAM, 0);

    if (UDPsockFd < 0)
    {
        fprintf(stderr, "Socket failed!!!");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(UDPsockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        fprintf(stderr, "setsockopt failed!!!");
        exit(EXIT_FAILURE);
    }

    host = gethostbyname(hostname);
    if (host == NULL)
    {
        fprintf(stderr, "Error!!! Get hostname failed!");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    bcopy((char *)host->h_addr, (char *)&serverAddr.sin_addr.s_addr, host->h_length);
    serverAddr.sin_port = htons(port);

    addr_len = sizeof(serverAddr);
    memset((char *)&packetSend, 0, sizeof(packetSend));

    //starting 3-way handshake
    //start sending SYN

    packetSend.seqNum = rand() % (MAX_SEQ_NUM);
    packetSend.ackNum = 0;
    packetSend.ackFlag = 0;
    packetSend.synFlag = 1;
    packetSend.finFlag = 0;
    packetSend.dataSize = 0;
    byteSend = sendto(UDPsockFd, &packetSend, sizeof(packetSend), 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (byteSend < 0)
    {
        fprintf(stderr, "Error!!!sending message");
        exit(EXIT_FAILURE);
    }
    currSeq = packetSend.seqNum;
    if (currSeq > MAX_SEQ_NUM)
    {
        currSeq -= MAX_SEQ_NUM;
    }
    currAck = 0;

    if (packetSend.synFlag == 1)
    {
        fprintf(stdout, "SEND %d %d SYN\n", packetSend.seqNum, packetSend.ackNum);
    }

    //if timeout, resend SYN packet
    //otherwise, receive SYN ACK and process to next step
    while (1)
    {
        FD_ZERO(&rfds);
        FD_SET(UDPsockFd, &rfds);
        tv.tv_usec = 500000; // 500ms
        ntime = select(UDPsockFd + 1, &rfds, NULL, NULL, &tv);
        if (ntime == 0)
        {
            fprintf(stdout, "TIMEOUT %d\n", packetSend.seqNum);
            byteSend = sendto(UDPsockFd, &packetSend, sizeof(packetSend), 0, (const struct sockaddr *)&serverAddr, sizeof(serverAddr));
            if (byteSend < 0)
            {
                close(UDPsockFd);
                fprintf(stderr, "Error!!!Could not write to socket");
                exit(EXIT_FAILURE);
            }
            fprintf(stdout, "RESEND %d %d SYN\n", packetSend.seqNum, packetSend.ackNum);
        }
        else
        {
            byteRecv = recvfrom(UDPsockFd, &packetRecv, sizeof(packetRecv), 0, (struct sockaddr *)&serverAddr, &addr_len);
            if (packetRecv.synFlag == 1 && packetRecv.ackFlag == 1)
            {
                fprintf(stdout, "RECV %d %d SYN ACK\n", packetRecv.seqNum, packetRecv.ackNum);
                break;
            }
        }
    }

    currSeq = packetRecv.ackNum;
    if (currSeq > MAX_SEQ_NUM)
    {
        currSeq -= MAX_SEQ_NUM;
    }
    currAck = (packetRecv.seqNum + 1) % MAX_SEQ_NUM;

    //Processing file to send to server
    int totalPackets = 0;
    fd = fopen(filename, "r"); //open the file
    if (fd == NULL)
    {
        fprintf(stderr, "Error!!! in opening file\n");
        exit(1);
    }
    fseek(fd, 0L, SEEK_END);
    long fileSize = ftell(fd);
    fseek(fd, 0L, SEEK_SET);
    char *file_buf = malloc(sizeof(char) * fileSize);
    fread(file_buf, sizeof(char), fileSize, fd);

    //determine total packets need to be sent
    totalPackets = (fileSize / PAYLOAD) + (fileSize % PAYLOAD != 0);
    int ackedPackets = 0;
    // exit(1);

    int numPackets = 0;
    int currByte = 0;
    int currPacket = 0;
    while (ackedPackets < totalPackets)
    {
        while (numPackets < WINDOW_SIZE && currByte < fileSize)
        {
            //calcuate length of data in packet
            int len = PAYLOAD;
            if (currByte + PAYLOAD > fileSize)
                len = fileSize - currByte;

            //create packet to send out , and add to window
            packetSend.ackFlag = 1;
            packetSend.synFlag = 0;
            packetSend.finFlag = 0;
            packetSend.seqNum = currSeq;
            packetSend.nextSeqNum = packetSend.seqNum + len;
            if (packetSend.nextSeqNum > MAX_SEQ_NUM)
                packetSend.nextSeqNum -= MAX_SEQ_NUM;
            packetSend.dataSize = (fileSize - currByte < PAYLOAD) ? fileSize - currByte : PAYLOAD;
            memcpy(packetSend.data, file_buf + currByte, packetSend.dataSize);

            packetSend.ackNum = currAck;
            currByte += len;
            currSeq = packetSend.seqNum + len;
            if (currSeq > MAX_SEQ_NUM)
                currSeq -= MAX_SEQ_NUM;
            wind[numPackets].pkt = packetSend;
            wind[numPackets].isAcked = 0;

            if (sendto(UDPsockFd, &packetSend, sizeof(packetSend), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
            {
                fprintf(stderr, "Error!!! sending message failed\n");
                exit(EXIT_FAILURE);
            }
            if (currPacket > 0)
                fprintf(stdout, "SEND %d %d ACK\n", packetSend.seqNum, 0);
            else
            {
                fprintf(stdout, "SEND %d %d ACK\n", packetSend.seqNum, packetSend.ackNum);
            }

            if (byteSend < 0)
            {
                close(UDPsockFd);
                fprintf(stderr, "Error writing to socket\n");
            }

            //Set the beginning time of when packet is sent.
            ntime = clock_gettime(CLOCK_MONOTONIC_RAW, &start);
            if (ntime != 0)
            {
                close(UDPsockFd);
                fprintf(stderr, "Error getting start time.\n");
            }
            wind[numPackets].start_time = start;

            currPacket++;
            numPackets++;
        }

        //Check current time to check against wind[0].pkttv to see if there is a need to update tv.
        ntime = clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        if (ntime != 0)
        {
            close(UDPsockFd);
            fprintf(stderr, "Error getting stop time.\n");
        }

        long sdiff = stop.tv_sec - wind[0].start_time.tv_sec;
        long nsdiff = stop.tv_nsec - wind[0].start_time.tv_nsec;
        long totDiffNS = sdiff * BILLION + nsdiff;

        if (totDiffNS > 500000000)
            tv.tv_usec = 0;

        else
            tv.tv_usec = 500000 - totDiffNS / 1000;

        FD_ZERO(&rfds);
        FD_SET(UDPsockFd, &rfds);
        ntime = select(UDPsockFd + 1, &rfds, NULL, NULL, &tv);
        if (ntime == -1)
        {
            fclose(fd);
            close(UDPsockFd);
            fprintf(stderr, "Error!!! with select\n");
            exit(EXIT_FAILURE);
        }
        if (ntime == 0)
        {
            //resend packet to client
            fprintf(stdout, "TIMEOUT %d\n", wind[0].pkt.seqNum);
            byteSend = sendto(UDPsockFd, &wind[0].pkt, sizeof(packetSend), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
            fprintf(stdout, "RESEND %d %d ACK\n", wind[0].pkt.seqNum, wind[0].pkt.ackNum);
            if (byteSend < 0)
            {
                close(UDPsockFd);
                fprintf(stderr, "Error writing to socket\n");
                exit(EXIT_FAILURE);
            }

            ntime = clock_gettime(CLOCK_MONOTONIC_RAW, &wind[0].start_time);
            if (ntime != 0)
            {
                close(UDPsockFd);
                fprintf(stderr, "Error getting start time.\n");
                exit(EXIT_FAILURE);
            }
        }

        else
        {
            memset((char *)&packetRecv, 0, sizeof(packetRecv));
            byteRecv = recvfrom(UDPsockFd, &packetRecv, sizeof(packetRecv), 0, (struct sockaddr *)&serverAddr, &addr_len);
            if (byteRecv < 0)
            {
                close(UDPsockFd);
                fprintf(stderr, "Error reading from socket\n");
                exit(EXIT_FAILURE);
            }
            if (packetRecv.ackFlag == 1)
            {
                fprintf(stdout, "RECV %d %d ACK\n", packetRecv.seqNum, packetRecv.ackNum);
            }

            //Update window: check to see which packet was ACKed
            int i = 0;
            while (i < numPackets)
            {
                if (wind[i].pkt.nextSeqNum == packetRecv.ackNum)
                {
                    wind[i].isAcked = 1;
                    ackedPackets++;
                    break;
                }
                i++;
            }

            //Update window: remove packets from window if consecutively ACKed
            while (wind[0].isAcked == 1 && numPackets > 0)
            {
                for (int j = 0; j < numPackets - 1; j++)
                {
                    wind[j] = wind[j + 1];
                }
                numPackets--;
            }
        }
    }

    //FIN tear-down step
    memset((char *)&packetSend, 0, sizeof(packetSend));
    struct packet ackedFinPacket;
    packetSend.seqNum = packetRecv.ackNum;
    packetSend.ackNum = 0;
    packetSend.ackFlag = 0;
    packetSend.synFlag = 0;
    packetSend.finFlag = 1;
    packetSend.dataSize = 0;
    currSeq = packetSend.seqNum;
    if (sendto(UDPsockFd, &packetSend, sizeof(packetSend), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        close(UDPsockFd);
        fprintf(stderr, "Error!!! in writing data to socket");
        exit(EXIT_FAILURE);
    }
    tv.tv_sec = 1;
    tv.tv_usec = 0; // 500ms is 5E5 us
    if (packetSend.finFlag == 1)
    {
        fprintf(stdout, "SEND %d %d FIN\n", packetSend.seqNum, packetSend.ackNum);
    }

    while (1)
    {
        FD_ZERO(&rfds);
        FD_SET(UDPsockFd, &rfds);
        tv.tv_usec = 500000;
        ntime = select(UDPsockFd + 1, &rfds, NULL, NULL, &tv);
        if (ntime == 0)
        {
            fprintf(stdout, "TIMEOUT %d\n", packetSend.seqNum);
            sendto(UDPsockFd, &packetSend, sizeof(packetSend), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
            printf("RESEND %d %d FIN\n", packetSend.seqNum, packetSend.ackNum);
        }
        else
        {
            do
            {
                memset((char *)&packetRecv, 0, sizeof(packetRecv));
                byteRecv = recvfrom(UDPsockFd, &packetRecv, sizeof(packetRecv), 0, (struct sockaddr *)&serverAddr, &addr_len);

                if (packetRecv.finFlag == 1)
                {
                    currSeq++;
                    if (currSeq == WINDOW_SIZE + 1)
                        currSeq = 0;
                    break;
                }
                else
                {
                    fprintf(stdout, "RECV %d %d ACK\n", packetRecv.seqNum, packetRecv.ackNum);
                    currSeq++;
                    if (currSeq == WINDOW_SIZE + 1)
                        currSeq = 0;
                }
            } while (packetRecv.ackFlag != 1 && packetRecv.ackNum != currSeq);

            break;
        }
    }

    while (1)
    {
        FD_ZERO(&rfds);
        FD_SET(UDPsockFd, &rfds);

        ntime = select(UDPsockFd + 1, &rfds, NULL, NULL, &tv);
        if (ntime == 0)
            break;
        else
        {
            memset((char *)&packetSend, 0, sizeof(packetSend));
            memset((char *)&packetRecv, 0, sizeof(packetRecv));
            byteRecv = recvfrom(UDPsockFd, &packetRecv, MAX_FILE_SIZE, 0, (struct sockaddr *)&serverAddr, &addr_len);
            fprintf(stdout, "RECV %d %d FIN\n", packetRecv.seqNum, packetRecv.ackNum);
            if (packetRecv.finFlag == 1)
            {
                packetSend.seqNum = currSeq;
                packetSend.ackNum = packetRecv.seqNum + 1;
                packetSend.ackFlag = 1;
                packetSend.finFlag = 0;
                packetSend.synFlag = 0;
                packetSend.dataSize = 0;

                if (packetSend.seqNum != ackedFinPacket.seqNum)
                {
                    ackedFinPacket = packetSend;
                    if (sendto(UDPsockFd, &packetSend, sizeof(packetSend), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
                    {
                        perror("ERROR:sending message");
                        exit(1);
                    }
                    fprintf(stdout, "SEND %d %d ACK\n", packetSend.seqNum, packetSend.ackNum);
                }

                else
                {
                    if (sendto(UDPsockFd, &packetSend, sizeof(packetSend), 0, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
                    {
                        perror("ERROR:sending message");
                        exit(1);
                    }
                    fprintf(stdout, "SEND %d %d DUP-ACK\n", packetSend.seqNum, packetSend.ackNum);
                }
            }
        }
    }
    fclose(fd);
    close(UDPsockFd);
    return 0;
}
