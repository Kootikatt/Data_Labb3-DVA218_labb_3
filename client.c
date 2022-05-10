#include "header.h"
#include <asm-generic/ioctls.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT 5555
// #define hostNameLength 50
// #define messageLength 256
// #define MAXMSG 512

int createSocket(struct sockaddr_in *serverName, char *argv) {

  int *dstHost = "127.0.0.1";

  int socketfd;
  struct hostent *hostinfo;

  if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    printf("Can't create UDP socket\n");

  memset(serverName, 0, sizeof(*serverName));

  serverName->sin_addr.s_addr = inet_addr(dstHost);
  serverName->sin_family = AF_INET;
  serverName->sin_port = PORT;

  return socketfd;
}
int isCorrupt(rtp *buffer) {
  printf("PLACEHOLDER");
  return 0;
}

int rcvMessage(int socketfd, struct sockaddr_in *serverName, rtp *buffer) {

  socklen_t socklen = sizeof(struct sockaddr);
  int recvResult = recvfrom(socketfd, buffer, MAXMSG, MSG_WAITALL,
                            (struct sockaddr *)serverName, &socklen);
  if (recvResult < 0) {
    printf("Could not recieve message!\n");
  }
  return recvResult;
}

int readFlag(rtp *buffer) {

  if (buffer->flags == ACK) {
    return ACK;
  } else if (buffer->flags == SYNACK) {
    return SYNACK;
  } else if (buffer->flags == SYN) {
    return SYN;
  }
  return 0;
}

int readMessage(rtp *buffer) {

  int flag = readFlag(buffer);
  if (flag == 0) {
    // printMessage
  }
  return flag;
}

int sendMessage(int flag, int socketfd, rtp *buffer,
                struct sockaddr_in *serverName) {

  buffer->flags = flag;
  while (1) {
    int result =
        sendto(socketfd, buffer, sizeof(*buffer), 0,
               (const struct sockaddr *)serverName, sizeof(*serverName));

    if (result < 0) {
      printf("Could not send message!\n");
      return 0;
    } else
      break;
  }
  return 1;
}

int wait_SYNACK(int socketfd, rtp * buffer, struct sockaddr_in *serverName) {

  int wait = 1, status;
  timer_t start, stop, time_passed;

  start = clock();

  while (wait != SYNACK){
  
    ioctl(socketfd, FIONBIO, &status);
        if(status > 0){
          wait = rcvMessage(socketfd, serverName, buffer);
        }
        stop = clock();
        time_passed = (double)(stop-start)/CLOCKS_PER_SEC;
        if(time_passed >= TIMEOUT){
          sendMessage(NACK, socketfd, buffer, serverName);
          start = clock(); 
        }
  }
}

int clientStart(int socketfd, rtp *buffer, struct sockaddr_in *serverName) {

  int starting = 1, state = SYN, recvResult, wait = 1, timeout = 0, sendR;
  time_t start, stop;
  double time_passed;
  pthread_t timeThread;

  while (starting) {
    switch (state) {
    case INIT:
      sendR = sendMessage(SYN, socketfd, buffer, serverName);
      if (sendR == 1) {
        state = SYNACK;
      }
      break;
    case SYNACK:
      wait_SYNACK();
      starting = 0;
      break;
    default:
      break;
    }
  }
}

int main(int argc, char *argv[]) {

  int state = START, start = 0, opened = 0, close = 0, bindResult;
  rtp buffer;
  struct sockaddr_in serverName;
  argv[1] = "localhost";

  int socketfd = createSocket(&serverName, argv[1]);

  while (1) {

    switch (state) {
    case START:
      start = clientStart(socketfd, &buffer, &serverName);
      if (start == 1) {
        state = OPENED;
      }
      break;

    case OPENED:

      if (opened == 1) {
        state = CLOSE;
      }
      break;
    case CLOSE:
      break;
    default:
      state = START;
      break;
    }
  }

  int result = sendto(socketfd, &testMessage, sizeof(testMessage), 0,
                      (const struct sockaddr *)&serverName, sizeof(serverName));

  if (result < 0) {
    printf("Could not send message!\n");
  }

  return 0;
}