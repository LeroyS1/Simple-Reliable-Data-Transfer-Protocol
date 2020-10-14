//
// Created by Leroy Le on 5/9/20.
//
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>
#include <inttypes.h>

#include "packet.h"

#define PACKAGE_SIZE 524
#define PAYLOAD 512
#define MAX_FILE_SIZE 20000 // 10MB
#define MAX_SEQ_NUM 25600

static struct packet ackedSent[MAX_FILE_SIZE];
static struct packet pktBuffer[MAX_FILE_SIZE];

int main(int argc, char *argv[])
{
    int UDPSockFd;
    int bytesRecv;
    int byteSend;
    struct sockaddr_in clientAddr, serverAddr;
    socklen_t client_len;
    int opt = 1;
    int port;

    struct packet packetRecv;
    struct packet packetSend;
    struct packet lastOrderedPacket;
    int firstOrderedPacket = 1;
    int outOfOrderedPacket = 0;
    int numberAcked = 0;
    uint16_t nextSeq;
    uint16_t currSeq;
    int numberOfFile = 0;
    FILE *fd;
    struct timeval tv;
    int ntime = 0;
    fd_set rfds;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;

    // seed rand()
    srand(time(NULL));
    char *filename;

    if (argc != 2)
    {
        fprintf(stderr, "Error!!No server <port>\n");
        exit(EXIT_FAILURE);
    }

    UDPSockFd = socket(AF_INET, SOCK_DGRAM, 0); //create UDP socket
    if (UDPSockFd < 0)
    {
        fprintf(stderr, "Socket failed!!!");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(UDPSockFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        fprintf(stderr, "setsockopt failed!!!");
        exit(EXIT_FAILURE);
    }
    port = atoi(argv[1]);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);

    if (bind(UDPSockFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        fprintf(stderr, "Bind failed!!!");
        exit(EXIT_FAILURE);
    }
    int countFlag = 0;

    client_len = sizeof(clientAddr);
    memset((char *)&packetSend, 0, sizeof(packetSend));
    memset((char *)&packetRecv, 0, sizeof(packetRecv));
    printf("Waiting for requests from client...\n");
    while (1)
    {
        bytesRecv = recvfrom(UDPSockFd, &packetRecv, sizeof(packetRecv), 0, (struct sockaddr *)&clientAddr, &client_len);

        if (packetRecv.synFlag == 1)
        {
            countFlag++;
            fprintf(stdout, "RECV %d %d SYN\n", packetRecv.seqNum, packetRecv.ackNum);
            if (countFlag > 1)
            {
                packetSend.seqNum = currSeq;
                if (packetRecv.seqNum == MAX_SEQ_NUM)
                    packetSend.ackNum = 0;
                else
                    packetSend.ackNum = packetRecv.seqNum + 1;
                packetSend.ackFlag = 1;
                packetSend.synFlag = 1;
                packetSend.finFlag = 0;
                nextSeq = packetRecv.seqNum + 1;
                if (nextSeq > MAX_SEQ_NUM)
                    nextSeq -= MAX_SEQ_NUM;
                currSeq = packetSend.seqNum;
                byteSend = sendto(UDPSockFd, &packetSend, sizeof(packetSend), 0, (const struct sockaddr *)&clientAddr, client_len);
                if (byteSend < 0)
                {
                    fprintf(stderr, "Error!!!!!!! sending message failed\n");
                    close(UDPSockFd);
                    exit(EXIT_FAILURE);
                }
                fprintf(stdout, "SEND %d %d SYN DUP-ACK\n", packetSend.seqNum, packetSend.ackNum);
            }
            else
            {
                packetSend.seqNum = rand() % (MAX_SEQ_NUM - 1);
                if (packetRecv.seqNum == MAX_SEQ_NUM)
                    packetSend.ackNum = 0;
                else
                    packetSend.ackNum = packetRecv.seqNum + 1;

                //set flags & seq and ack num for receiver
                packetSend.ackFlag = 1;
                packetSend.synFlag = 1;
                packetSend.finFlag = 0;
                // currAck = packetSend.ackNum;
                nextSeq = packetRecv.seqNum + 1;
                if (nextSeq > MAX_SEQ_NUM)
                    nextSeq -= MAX_SEQ_NUM;
                currSeq = packetSend.seqNum;
                //send SYN ACK response
                byteSend = sendto(UDPSockFd, &packetSend, sizeof(packetSend), 0, (const struct sockaddr *)&clientAddr, client_len);
                if (byteSend < 0)
                {
                    fprintf(stderr, "Error!!!!!!! sending message failed\n");
                    close(UDPSockFd);
                    exit(EXIT_FAILURE);
                }
                fprintf(stdout, "SEND %d %d SYN ACK\n", packetSend.seqNum, packetSend.ackNum);
            }
            continue;
        }
        else if (packetRecv.finFlag == 1)
        {
            fprintf(stdout, "RECV %d %d FIN\n", packetRecv.seqNum, packetRecv.ackNum);
            break;
        }
        else
        {

            if ((firstOrderedPacket == 1 && packetRecv.seqNum == nextSeq) || (firstOrderedPacket == 0 && lastOrderedPacket.nextSeqNum == packetRecv.seqNum))
            {
                fprintf(stdout, "RECV %d %d ACK\n", packetRecv.seqNum, packetRecv.ackNum);
                numberOfFile++;
                int length = snprintf(NULL, 0, "%d.file", numberOfFile);
                filename = malloc(length + 1);

                snprintf(filename, length + 1, "%d.file", numberOfFile);
                fd = fopen(filename, "w");
                if (fd < 0)
                {
                    fclose(fd);
                    fprintf(stderr, "ERROR in opening file\n");
                    exit(EXIT_FAILURE);
                }

                fwrite(packetRecv.data, 1, packetRecv.dataSize, fd);

                lastOrderedPacket = packetRecv;

                if (packetRecv.seqNum == nextSeq)
                    firstOrderedPacket = 0;

                if (outOfOrderedPacket != 0)
                {
                    fprintf(stdout, "RECV %d %d ACK\n", packetRecv.seqNum, packetRecv.ackNum);
                    int i = 0;
                    while (i < outOfOrderedPacket)
                    {
                        if (lastOrderedPacket.nextSeqNum == pktBuffer[i].seqNum)
                        {
                            bytesRecv = fwrite(pktBuffer[i].data, 1, pktBuffer[i].dataSize, fd);

                            if (bytesRecv < 0)
                            {
                                close(UDPSockFd);
                                fclose(fd);
                                fprintf(stderr, "Error in writing file!!!\n");
                                exit(EXIT_FAILURE);
                            }
                            lastOrderedPacket = pktBuffer[i];

                            for (int q = i + 1; q < outOfOrderedPacket; q++)
                            {
                                pktBuffer[i] = pktBuffer[q];
                                i++;
                            }
                            i = 0;
                            outOfOrderedPacket--;
                        }
                        else
                            i++;
                    }
                }
            }
            else
            {
                pktBuffer[outOfOrderedPacket] = packetRecv;
                outOfOrderedPacket++;
                fprintf(stdout, "outOfOrderedPacket size: %d\n", outOfOrderedPacket);
            }

            packetSend.seqNum = packetRecv.ackNum;
            packetSend.ackNum = (packetRecv.seqNum + packetRecv.dataSize) % MAX_SEQ_NUM;
            packetSend.ackFlag = 1;
            packetSend.synFlag = 0;
            packetSend.finFlag = 0;
            currSeq = packetSend.seqNum;
            nextSeq = packetRecv.seqNum + packetRecv.dataSize;
            if (nextSeq > MAX_SEQ_NUM)
                nextSeq -= MAX_SEQ_NUM;
            if (sendto(UDPSockFd, &packetSend, sizeof(packetSend), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
            {
                close(UDPSockFd);
                fprintf(stderr, "ERROR:sending message\n");
                exit(EXIT_FAILURE);
            }

            ackedSent[numberAcked] = packetRecv;
            numberAcked++;
            fprintf(stdout, "SEND %d %d ACK\n", packetSend.seqNum, packetSend.ackNum);
            if (numberAcked == MAX_FILE_SIZE)
            {
                int q = MAX_FILE_SIZE / 2;
                for (int i = 0; i < MAX_FILE_SIZE / 2; i++)
                {
                    ackedSent[i] = ackedSent[q];
                    q++;
                }
                numberAcked = MAX_FILE_SIZE / 2;
            }
            fclose(fd);
        }
    }
    //FIN tear down
    //FIN
    memset((char *)&packetSend, 0, sizeof(packetSend));
    packetSend.seqNum = currSeq;
    packetSend.ackNum = (packetRecv.seqNum + 1) % 25600;
    packetSend.ackFlag = 1;
    packetSend.synFlag = 0;
    packetSend.finFlag = 0;

    if (sendto(UDPSockFd, &packetSend, sizeof(packetSend), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
    {
        fprintf(stderr, "ERROR!!! in sending message\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "SEND %d %d ACK\n", packetSend.seqNum, packetSend.ackNum);

    memset((char *)&packetSend, 0, sizeof(packetSend));
    packetSend.seqNum = currSeq;
    packetSend.ackNum = 0;
    packetSend.ackFlag = 0;
    packetSend.synFlag = 0;
    packetSend.finFlag = 1;
    currSeq = (currSeq + 1) % MAX_SEQ_NUM;
    if (sendto(UDPSockFd, &packetSend, sizeof(packetSend), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr)) < 0)
    {
        fprintf(stderr, "ERROR!!! in sending message\n");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "SEND %d %d FIN\n", packetSend.seqNum, packetSend.ackNum);
    while (1)
    {
        FD_ZERO(&rfds);
        FD_SET(UDPSockFd, &rfds);
        tv.tv_usec = 500000;
        ntime = select(UDPSockFd + 1, &rfds, NULL, NULL, &tv);
        if (ntime == -1)
        {
            close(UDPSockFd);
            fprintf(stderr, "Error with select\n");
        }
        if (ntime == 0)
        {
            fprintf(stdout, "TIMEOUT %d\n", packetSend.seqNum);
            sendto(UDPSockFd, &packetSend, sizeof(packetSend), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
            printf("RESEND %d %d FIN\n", packetSend.seqNum, packetSend.ackNum);
        }
        else
        { // ACK
            memset((char *)&packetRecv, 0, sizeof(packetRecv));
            bytesRecv = recvfrom(UDPSockFd, &packetRecv, sizeof(packetRecv), 0, (struct sockaddr *)&clientAddr, &client_len);
            if (bytesRecv < 0)
            {
                close(UDPSockFd);
                fprintf(stderr, "ERROR: message not received.\n");
            }
            if (packetRecv.ackFlag == 1)
            {
                fprintf(stdout, "RECV %d %d ACK\n", packetRecv.seqNum, packetRecv.ackNum);
                break;
            }
        }
    }
    close(UDPSockFd);
    return 0;
}
