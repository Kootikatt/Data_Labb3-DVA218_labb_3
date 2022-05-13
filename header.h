#ifndef MY_GUARD
#define MY_GUARD 1

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>


// #define PORT 5555
// #define hostNameLength 50
#define messageLength 256
#define INIT 0
#define SYN 1
#define ACK 2
#define SYNACK 3
#define START 4
#define OPENED 5
#define CLOSE 6
#define CORRUPT 7
#define TIMEOUT 1
#define NACK 8
#define MAXMSG 512
#define DR 9
#define DATA 0

#define PACKETSTOSEND 10

typedef struct rtp_struct{
    int flags;
    int id;
    int seq;
    int windowsize;
    int checksum;
    char data[messageLength];
}rtp;

#endif