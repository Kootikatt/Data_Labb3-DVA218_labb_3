#include "getChecksum.c"
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
#define WINDOWSIZE 3
#define NUMBEROFPACKAGES 10
// #define hostNameLength 50
// #define messageLength 256
// #define MAXMSG 512

int getChecksum();

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
  buffer->windowsize = WINDOWSIZE;
  buffer->checksum = getChecksum(buffer->data);
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

int wait_SYNACK(int socketfd, rtp *buffer, struct sockaddr_in *serverName) {

  int wait = 1, status;
  clock_t start, stop;
  double time_passed;

  start = clock();

  while (1) {

    ioctl(socketfd, FIONREAD, &status);
    if (status > 0) {
      wait = rcvMessage(socketfd, serverName, buffer);
      if (wait > 0) {
        wait = readMessage(buffer);
        if ((!isCorrupt(buffer)) && wait == SYNACK) {
          sendMessage(ACK, socketfd, buffer, serverName);
          break;
        };
      }
    }

    stop = clock();

    time_passed = (double)(stop - start) / CLOCKS_PER_SEC;
    if ((time_passed >= TIMEOUT) || wait == NACK) {
      sendMessage(NACK, socketfd, buffer, serverName);
      start = clock();
    }
  }
  printf("Client initiated!\n");
  return 0;
}

int clientStart(int socketfd, rtp *buffer, struct sockaddr_in *serverName) {

  int starting = 1, state = INIT, recvResult, wait = 1, timeout = 0, sendR;
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
      wait_SYNACK(socketfd, buffer, serverName);
      starting = 0;
      break;
    default:
      break;
    }
  }
  return 1;
}

void makePacket(rtp *buffer, int packetNumber, char data[], int checksum) {

  buffer->seq = packetNumber;
  strcpy(buffer->data, data);
  buffer->checksum = checksum;
}

int getAckNumber(rtp *buffer) {
  int ackNumber = buffer->seq;
  return ackNumber;
}

int packetInWindow(int ackNumber, int base) {
  if ((ackNumber <= (base + WINDOWSIZE - 1)) && (ackNumber >= base)) {
    return 1;
  }
  return 0;
}

int isTimeOut(clock_t start) {
  clock_t stop = clock();
  double timePassed = (double)(stop - start) / CLOCKS_PER_SEC;
  if (timePassed >= TIMEOUT) {
    return 1;
  }
  return 0;
}

int isNextInWindow(int nextPacket, int base) {
  if (nextPacket - base < WINDOWSIZE) {
    return 1;
  }
  return 0;
}

int clientSlidingWindows(int socketfd, rtp *buffer,
                         struct sockaddr_in *serverName) {

  rtp packets[WINDOWSIZE];
  int status, base = 0, nextPacket = 0, checksum, flag, ackNumber;
  clock_t start, stop;
  double timePassed;

  while (1) {
    if (isNextInWindow(nextPacket, base)) {
      if (nextPacket == base) {
        start = clock();
      }
      if (isNextInWindow(nextPacket, base) &&
          nextPacket <= NUMBEROFPACKAGES - 1) {
        makePacket(buffer, nextPacket, (packets[0]).data, checksum);
        sendMessage(0, socketfd, buffer, serverName);
        nextPacket++;
      }
    }
    ioctl(socketfd, FIONREAD, &status);
    if (status >= 0) {
      rcvMessage(socketfd, serverName, buffer);
      flag = readFlag(buffer);
      ackNumber = getAckNumber(buffer);
      if ((flag == ACK) && packetInWindow(ackNumber, base)) {
        base = ackNumber;
        if (base == nextPacket) {
          start = 0;
        } else {
          start = clock();
        }
      }
    }
    int timeOut = isTimeOut(start);
    if (timeOut == 1) {
      printf("Placeholder");
    }
    if ((base) == PACKETSTOSEND - 1) {
      break;
    }
  }
  printf("All packages sent and ACK'd!\n");
  return 1;
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
        printf("Opened!\n");
        state = OPENED;
      }
      break;

    case OPENED:

      clientSlidingWindows(socketfd, &buffer, &serverName);
      state = CLOSE;
      break;
    case CLOSE:
      break;
    default:
      state = START;
      break;
    }
  }

  // int result = sendto(socketfd, &testMessage, sizeof(testMessage), 0,
  //                     (const struct sockaddr *)&serverName,
  //                     sizeof(serverName));

  // if (result < 0) {
  //   printf("Could not send message!\n");
  // }

  return 0;
}