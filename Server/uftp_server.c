/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h> 

#define BUFSIZE 1024
#define MSGTYPESIZE 3
#define SEQNOSIZE 5
#define HEADER 8
#define MSGSIZE 1016

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char buf1[BUFSIZE]; /* message buf1 */
  char msgtype[MSGTYPESIZE + 1];
  char msgtype1[MSGTYPESIZE + 1];
  char SN[SEQNOSIZE + 1];
  char SN1[SEQNOSIZE + 1];
  char msg[MSGSIZE + 1];
  char filename[20];
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  int SeqNo; /* Sequence number of the received packet */
  int CSN = -1; /* Cumulative sequence number to track which packets arrived */
  int SeqNo1 = 10000; /* Sequence number of the packet */
  int msgsz;
  int len;
  FILE *fp;
  DIR *d;
  struct dirent *dir;
  int comp;
  int comp1;
  int retry;
  int testcount = 1;
  int badhost = 0;
  int running = 1;
  

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  
  struct timeval tv;
  tv.tv_sec = 3;
  tv.tv_usec = 0;
  
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
             (const void *)&optval , sizeof(int));
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
             (struct timeval *)&tv,sizeof(struct timeval));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (running) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    retry = 0;
    badhost = 0;
    bzero(buf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0) {
      retry = 1;
    }

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
              sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      badhost = 1;
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      badhost = 1;
    printf("Server received %d/%d bytes from client\n", (int) strlen(buf), n);
    if (retry != 1) {
      bzero(msgtype, MSGTYPESIZE + 1);
      memcpy(msgtype, buf, MSGTYPESIZE);

      bzero(SN, SEQNOSIZE + 1);
      memcpy(SN, buf + MSGTYPESIZE, SEQNOSIZE);
      SeqNo = atoi(SN);
    }

    /* 
     * Structure of message: Command(3B) SeqNo(5B) Message(1016B)
     */

    if (strcmp(msgtype, "ack") != 0 && strcmp(msgtype, "acl") != 0 && strcmp(msgtype, "") != 0 && !badhost) {
      bzero(buf1, BUFSIZE);
      strcpy(msgtype1, "ack");
      memcpy(buf1, msgtype1, MSGTYPESIZE);
      memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
      n = sendto(sockfd, buf1, strlen(buf1), 0, 
        (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
        error("ERROR in ack sendto");
      if (retry != 1)
        printf("Server sent %s %d to client after getting %s %d\n", msgtype1, SeqNo, msgtype, SeqNo);
      else
        printf("Server resent %s %d to client as it didn't receive anything after %s %d\n", msgtype1, SeqNo, msgtype, SeqNo);
    }

    /*
     * compare which action is being performed based on 3 char message
     */
    if (badhost) {
      // Do nothing and retry
      printf("Retry Recv due to bad host/first try\n");
    } else if (strcmp(msgtype, "get") == 0) {
      // Initial get message. This will initiate a send from the server back to the client. If this is also
      // the ending of the file, send fte message instead.
      comp1 = 0;
      if (retry != 1) {
        bzero(filename, 20);
        bzero(msg, MSGSIZE + 1);
        memcpy(filename, buf + HEADER, 20);
        fp = fopen(filename, "r");
        fseek(fp, 0, SEEK_SET);
        msgsz = fread(msg, MSGSIZE, 1, fp);
        if (msgsz == 0) {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "fte");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN1, "%d", SeqNo1);
          if (strlen(SN1) < SEQNOSIZE) {
            len = strlen(SN1);
            memcpy(SN1 + SEQNOSIZE - len, SN1, len);
            memset(SN1, '0', SEQNOSIZE - len);
          }
          memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
          memcpy(buf1 + HEADER, msg, MSGSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0, 
            (struct sockaddr *) &clientaddr, clientlen);
          printf("Server sent %s %d to client after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
          if (n < 0) 
            error("ERROR in fte sendto");
          SeqNo1 = (SeqNo1 + 1) % 100000;
          fclose(fp);
          comp1 = 1;
        } else {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "ftr");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN1, "%d", SeqNo1);
          if (strlen(SN1) < SEQNOSIZE) {
            len = strlen(SN1);
            memcpy(SN1 + SEQNOSIZE - len, SN1, len);
            memset(SN1, '0', SEQNOSIZE - len);
          }
          memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
          memcpy(buf1 + HEADER, msg, MSGSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0, 
            (struct sockaddr *) &clientaddr, clientlen);
          printf("Server sent %s %d to client after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
          if (n < 0) 
            error("ERROR in ftr sendto");
          SeqNo1 = (SeqNo1 + 1) % 100000;
        }
      } else {
        n = sendto(sockfd, buf1, strlen(buf1), 0, 
          (struct sockaddr *) &clientaddr, clientlen);
        printf("Server resent %s %d to client as it didn't receive anything after %s %d\n", msgtype1, SeqNo1 - 1, msgtype, SeqNo);
        if (n < 0) 
          error("ERROR in sendto"); 
      }
    } else if (strcmp(msgtype, "ack") == 0) {
      // Acknowledgement message from client when receiving file after get command.
      if ((SeqNo + 1) % 100000 == SeqNo1 && comp1 == 0) {
        bzero(msg, MSGSIZE + 1);
        msgsz = (int) fread(msg, MSGSIZE, 1, fp);

        if (msgsz == 0/* End of file */) {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "fte");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN1, "%d", SeqNo1);
          if (strlen(SN1) < SEQNOSIZE) {
            len = strlen(SN1);
            memcpy(SN1 + SEQNOSIZE - len, SN1, len);
            memset(SN1, '0', SEQNOSIZE - len);
          }
          memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
          memcpy(buf1 + HEADER, msg, MSGSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0, 
            (struct sockaddr *) &clientaddr, clientlen);
          printf("Server sent %s %d to client after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
          if (n < 0) 
            error("ERROR in fte sendto");
          SeqNo1 = (SeqNo1 + 1) % 100000;
          fclose(fp);
          comp1 = 1;
        } else {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "ftr");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN1, "%d", SeqNo1);
          if (strlen(SN1) < SEQNOSIZE) {
            len = strlen(SN1);
            memcpy(SN1 + SEQNOSIZE - len, SN1, len);
            memset(SN1, '0', SEQNOSIZE - len);
          }
          memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
          memcpy(buf1 + HEADER, msg, MSGSIZE);
          if (0 && testcount % 10000 == 2) {
            // Nothing
          } else {
            n = sendto(sockfd, buf1, strlen(buf1), 0, 
              (struct sockaddr *) &clientaddr, clientlen);
          }
          printf("Server sent %s %d to client after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
          if (n < 0) 
            error("ERROR in ftr sendto");
          SeqNo1 = (SeqNo1 + 1) % 100000;
          //testcount++;
        }
      } else if (comp1 == 1) {
        printf("Server completed get operation after getting %s %d\n", msgtype, SeqNo);
        bzero(msgtype, MSGTYPESIZE + 1);
      } else {
        n = sendto(sockfd, buf1, strlen(buf1), 0, 
          (struct sockaddr *) &clientaddr, clientlen);
        printf("Server resent %s %d to client as it didn't receive anything after %s %d\n", msgtype1, SeqNo1 - 1, msgtype, SeqNo);
        if (n < 0) 
          error("ERROR in fte sendto");        
      }

    } else if (strcmp(msgtype, "put") == 0) {
      // Initial put message. Server will send acknowledgement, create file with the filename
      // in the file system and expect ftr messages from the client.
      if (retry != 1 && (CSN < 0 || (CSN + 1) % 100000 == SeqNo)) {
        comp = 0;
        CSN = SeqNo;
        bzero(filename, 20);
        memcpy(filename, buf + HEADER, 20);
        printf("new file = %s", filename);
        fp = fopen(filename, "w+");
        fseek(fp, 0, SEEK_SET);
      }
    } else if (strcmp(msgtype, "ftr") == 0) {
      // File transfer message. This is part of the file being sent and will be combined with
      // the other parts previously received to form the file.
      if (comp == 1) {
        printf("Server completed put operation after getting %s %d\n", msgtype, SeqNo);
        bzero(msgtype, MSGTYPESIZE + 1);
      } else if (retry != 1 && (CSN < 0 || (CSN + 1) % 100000 == SeqNo)) {
        CSN = SeqNo;
        bzero(msg, MSGSIZE + 1);
        memcpy(msg, buf + HEADER, MSGSIZE);
        fwrite(msg, strlen(msg), 1, fp);
      }
    } else if (strcmp(msgtype, "fte") == 0) {
      // File transfer end message. This is the last part of the file being sent and will be
      // combined with the other parts previously received to form the completed file.
      if (comp == 1) {
        printf("Server completed put operation after getting %s %d\n", msgtype, SeqNo);
        bzero(msgtype, MSGTYPESIZE + 1);
      } else if (retry != 1 && (CSN < 0 || (CSN + 1) % 100000 == SeqNo)) {
        CSN = SeqNo;
        bzero(msg, MSGSIZE + 1);
        memcpy(msg, buf + HEADER, MSGSIZE);
        fwrite(msg, strlen(msg), 1, fp);
        fclose(fp);
        comp = 1;
        CSN = -1;
      }
    } else if (strcmp(msgtype, "del") == 0) {
      // Initial delete message. Server will delete the requested file and acknowledge.
      if (retry != 1) {
        bzero(filename, 20);
        bzero(msg, MSGSIZE + 1);
        memcpy(filename, buf + HEADER, 20);
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0
            || strcmp(filename, "server") == 0 || strcmp(filename, "uftp_server.c") == 0) {
          printf("File deletion not allowed\n");
        } else if (remove(filename) != 0) {
          printf("File deletion failed\n");
        } else {
          printf("File deletion successful\n");
        }
      }
    } else if (strcmp(msgtype, "lis") == 0) {
      // Initial list message. Server will respond with the files in the server's system.
      comp1 = 0;
      if (retry != 1) {
        bzero(filename, 20);
        bzero(msg, MSGSIZE + 1);
        memcpy(filename, buf + HEADER, 20);
        d = opendir(".");
        dir = readdir(d);
        if (dir == NULL) {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "lie");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN1, "%d", SeqNo1);
          if (strlen(SN1) < SEQNOSIZE) {
            len = strlen(SN1);
            memcpy(SN1 + SEQNOSIZE - len, SN1, len);
            memset(SN1, '0', SEQNOSIZE - len);
          }
          memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0, 
            (struct sockaddr *) &clientaddr, clientlen);
          printf("Server sent %s %d to client after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
          if (n < 0) 
            error("ERROR in fte sendto");
          SeqNo1 = (SeqNo1 + 1) % 100000;
          comp1 = 1;
        } else {
          bzero(msg, MSGSIZE + 1);
          strcpy(msg, dir->d_name);
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "lit");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN1, "%d", SeqNo1);
          if (strlen(SN1) < SEQNOSIZE) {
            len = strlen(SN1);
            memcpy(SN1 + SEQNOSIZE - len, SN1, len);
            memset(SN1, '0', SEQNOSIZE - len);
          }
          memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
          memcpy(buf1 + HEADER, msg, MSGSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0, 
            (struct sockaddr *) &clientaddr, clientlen);
          printf("Server sent %s %d to client after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
          if (n < 0) 
            error("ERROR in ftr sendto");
          SeqNo1 = (SeqNo1 + 1) % 100000;
        }
      } else {
        n = sendto(sockfd, buf1, strlen(buf1), 0, 
          (struct sockaddr *) &clientaddr, clientlen);
        printf("Server resent %s %d to client as it didn't receive anything after %s %d\n", msgtype1, SeqNo1 - 1, msgtype, SeqNo);
        if (n < 0) 
          error("ERROR in sendto"); 
      }
    } else if (strcmp(msgtype, "acl") == 0) {
      // Acknowledgement message from client when receiving file after get command.
      if ((SeqNo + 1) % 100000 == SeqNo1 && comp1 == 0) {
        dir = readdir(d);

        if (dir == NULL) {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "lie");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN1, "%d", SeqNo1);
          if (strlen(SN1) < SEQNOSIZE) {
            len = strlen(SN1);
            memcpy(SN1 + SEQNOSIZE - len, SN1, len);
            memset(SN1, '0', SEQNOSIZE - len);
          }
          memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0, 
            (struct sockaddr *) &clientaddr, clientlen);
          printf("Server sent %s %d to client after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
          if (n < 0) 
            error("ERROR in fte sendto");
          SeqNo1 = (SeqNo1 + 1) % 100000;
          comp1 = 1;
        } else {
          bzero(msg, MSGSIZE + 1);
          strcpy(msg, dir->d_name);
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "lit");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN1, "%d", SeqNo1);
          if (strlen(SN1) < SEQNOSIZE) {
            len = strlen(SN1);
            memcpy(SN1 + SEQNOSIZE - len, SN1, len);
            memset(SN1, '0', SEQNOSIZE - len);
          }
          memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
          memcpy(buf1 + HEADER, msg, MSGSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0, 
            (struct sockaddr *) &clientaddr, clientlen);
          printf("Server sent %s %d to client after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
          if (n < 0) 
            error("ERROR in ftr sendto");
          SeqNo1 = (SeqNo1 + 1) % 100000;
        }
      } else if (comp1 == 1) {
        printf("Server completed get operation after getting %s %d\n", msgtype, SeqNo);
        bzero(msgtype, MSGTYPESIZE + 1);
      } else {
        n = sendto(sockfd, buf1, strlen(buf1), 0, 
          (struct sockaddr *) &clientaddr, clientlen);
        printf("Server resent %s %d to client as it didn't receive anything after %s %d\n", msgtype1, SeqNo1 - 1, msgtype, SeqNo);
        if (n < 0) 
          error("ERROR in fte sendto");        
      }
    } else if (strcmp(msgtype, "exi") == 0) {
      // Exit command. Will exit while loop and close server.
      running = 0;
    } else {
      printf("Retry Recv\n");
      //error("ERROR message sent");
    }
  }
  printf("The Server program will terminate now\n");
}
