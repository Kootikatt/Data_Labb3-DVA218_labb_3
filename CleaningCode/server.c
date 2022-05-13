#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "getChecksum.c"
#include "header.h"

#define PORT 5555
#define hostNameLength 50
#define messageLength 256
#define MAXMSG 512
#define NUMBEROFPACKAGES 10

int windowSize;
int packageArray[NUMBEROFPACKAGES] = {0};

int createSocket(struct sockaddr_in *clientName) {

  int socketfd = socket(AF_INET, SOCK_DGRAM, 0);

  if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0) {
    perror("setsockopt(SO_REUSEADDR) failed");
  }

  if (socketfd < 0) {
    printf("Could not open socket!\n");
  }

  memset(clientName, 0, sizeof(*clientName));

  clientName->sin_addr.s_addr = INADDR_ANY;
  clientName->sin_port = PORT;
  clientName->sin_family = AF_INET;

  return socketfd;
}

int bindSocket(int socketfd, struct sockaddr_in *clientName) {

  int bindReturn =
      bind(socketfd, (const struct sockaddr *)clientName, sizeof(*clientName));
  if (bindReturn < 0) {
    fprintf(stdout, "Errno: %d", errno);
    printf("Could not bind socket!\n");
  }

  return bindReturn;
}

int rcvMessage(int socketfd, struct sockaddr_in *clientName, rtp *buffer) {

  socklen_t socklen = sizeof(struct sockaddr);
  int recvResult = recvfrom(socketfd, buffer, MAXMSG, MSG_WAITALL,
                            (struct sockaddr *)clientName, &socklen);
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

/*Tanken är att isCorrupt() tar *buffer samt den checksumma som skickas med i
headern. Jämför dem och returnerar 0 eller 1 beroende på om de är samma eller
inte.*/
int isCorrupt(rtp *buffer) {
  if (getChecksum(buffer->data) == buffer->checksum)
    return 0;

  return 1;
}

int sendMessage(int flag, int socketfd, rtp *buffer,
                struct sockaddr_in *clientName) {

  buffer->windowsize = windowSize;
  buffer->flags = flag;
  while (1) {
    int result =
        sendto(socketfd, buffer, sizeof(*buffer), 0,
               (const struct sockaddr *)clientName, sizeof(*clientName));

    if (result < 0) {
      printf("Could not send message!\n");
      return 0;
    } else
      break;
  }
  return 1;
}

// void *timeThread(int *timeout) {

//   time_t stop, time_passed, start;
//   start = clock();
//   while (1) {
//     stop = clock();
//     time_passed = (double)(stop - start) / CLOCKS_PER_SEC;
//     if (time_passed >= TIMEOUT) {
//       *timeout = 1;
//     }
//   }
// }

int serverStart(int socketfd, rtp *buffer, struct sockaddr_in *clientName) {

  int starting = 1, state = SYN, recvResult, wait = 1, timeout = 0;
  time_t start, stop;
  double time_passed;
  pthread_t timeThread;

  while (starting) {

    // recvResult = rcvMessage(socketfd, clientName, buffer);
    // state = readFlag(buffer);

    switch (state) {
    case SYN:
      while (1) {
        rcvMessage(socketfd, clientName, buffer);
        windowSize = buffer->windowsize;
        int read = readMessage(buffer);
        if (!isCorrupt(buffer) && read == SYN) {
          sendMessage(SYNACK, socketfd, buffer, clientName);
          state = SYNACK;
          printf("Received SYN from client!\n");
          break;
        }
      }
      break;
    case SYNACK:
      start = clock();
      int status;
      while (wait != ACK) {
        ioctl(socketfd, FIONREAD, &status);
        if (status >= 0) {
          wait = rcvMessage(socketfd, clientName, buffer);
          if (wait > 0) {
            wait = readMessage(buffer);
          }
        }
        stop = clock();
        time_passed = (double)(stop - start) / CLOCKS_PER_SEC;
        if ((time_passed >= TIMEOUT) || wait == NACK) {
          sendMessage(SYNACK, socketfd, buffer, clientName);
          start = clock();
        }
      }
      printf("Received ACK From client!\n");
      starting = 0;
      break;
    default:
      break;
    }
  }
  printf("Connection established with client!\n");
  return 1;
}
void printMessage(rtp *buffer) {
  printf("Message received from client: %s\n", buffer->data);
}

int wasReceived(rtp *buffer, int expectedSeqNumber) { 
  
  if(packageArray[expectedSeqNumber] == 1){
    return 0;
  }
  return 1; }

int serverSlidingWindows(int socketfd, rtp *buffer,
                         struct sockaddr_in *clientName) {

  int expectedPackageNumber = 0;
  int seqNumber;

  while (1) {
    rcvMessage(socketfd, clientName, buffer);
    if (readMessage(buffer) == DR) {
        printf("Disconnect request received from client!\n Initiating "
               "shutdown!\n");
        break;
      }
    if ((wasReceived(buffer, expectedPackageNumber) == 1) &&
        (isCorrupt(buffer)) == 0) {
      
      if (readMessage(buffer) == 0) {
        printMessage(buffer);
      }
      
      seqNumber = buffer->seq;
      memset(buffer, 0, sizeof(*buffer));
      buffer->seq = seqNumber;
      sendMessage(ACK, socketfd, buffer, clientName);
      expectedPackageNumber++;
    } else {
      seqNumber = buffer->seq;
      memset(buffer, 0, sizeof(*buffer));
      buffer->seq = seqNumber;
      sendMessage(NACK, socketfd, buffer, clientName);
    }
  }
  return 1;
}

int main(int argc, char *argv[]) {
  int state = START, start = 0, opened = 0, close = 0, bindResult;
  rtp buffer;
  struct sockaddr_in clientName;

  int socketfd = createSocket(&clientName);

  if ((bindResult = bindSocket(socketfd, &clientName)) >= 0) {

    printf("Waiting for client to connect: \n");

    while (1) {

      switch (state) {
      case START:
        start = serverStart(socketfd, &buffer, &clientName);
        if (start == 1) {
          printf("Test\n");
          state = OPENED;
        }
        break;

      case OPENED:
        opened = serverSlidingWindows(socketfd, &buffer, &clientName);
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
  }
}
