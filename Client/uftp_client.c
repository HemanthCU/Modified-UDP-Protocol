/* 
 * uftpclient.c - A simple UDP FTP client with reliability
 * usage: ./client <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 

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
  exit(0);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to send to */
  int n; /* message byte size */
  int serverlen; /* byte size of server's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct hostent *server; /* server host info */
  char *hostname; /* server host name to send to */
  char buf[BUFSIZE]; /* received message buf */
  char buf1[BUFSIZE]; /* sent message buf1 */
  char msgtype[MSGTYPESIZE + 1]; /* received message command */
  char msgtype1[MSGTYPESIZE + 1]; /* sent message command */
  char scancommand[10]; /* command entered by user */
  char SN[SEQNOSIZE + 1]; /* received message Seq No as string */
  char SN1[SEQNOSIZE + 1]; /* sent message Seq No as string */
  char msg[MSGSIZE + 1]; /* payload of the packet */
  char filename[20]; /* filename of the file */
  int SeqNo; /* Sequence number of the received packet */
  int CSN = -1; /* Cumulative sequence number to track which packets arrived */
  int SeqNo1 = 20000; /* Sequence number of the sent packet */
  int msgsz; /* file read byte size */
  int len; /* used for adjusting SeqNo < 10000 into char[5] */
  int comp; /* completion of current action */
  int retry = 0; /* 0 = first attempt/1 = resending attempt */
  int running = 1; /* while loop should be running */
  FILE *fp; /* FILE object to access files */

  const int DEBUG = 0; /* Enabling log statements */

  /* check command line arguments */
  if (argc != 3) {
     fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
     exit(0);
  }
  hostname = argv[1];
  portno = atoi(argv[2]);

  /* socket: create the socket */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
      error("ERROR opening socket");

  /* gethostbyname: get the server's DNS entry */
  server = gethostbyname(hostname);
  if (server == NULL) {
      fprintf(stderr,"ERROR, no such host as %s\n", hostname);
      exit(0);
  }

  struct timeval tv;
  tv.tv_sec = 4;
  tv.tv_usec = 0;

  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
             (struct timeval *)&tv,sizeof(struct timeval));

  /* build the server's Internet address */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
    (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno);

  /* get a command from the user */
  while (running) {
    bzero(msgtype1, MSGTYPESIZE);
    bzero(scancommand, 10);
    printf("Please enter the command:\n");
    scanf("%s", scancommand);
    memcpy(msgtype1, scancommand, MSGTYPESIZE);
    if (strcmp(msgtype1, "get") == 0) {
      // Initial get message. This will create a new file and initiate a send from the server back
      // to the client.
      bzero(filename, 20);
      printf("\nPlease enter the filename:\n");
      scanf("%s", filename);
      bzero(buf1, BUFSIZE);
      memcpy(buf1, msgtype1, MSGTYPESIZE);
      sprintf(SN1, "%d", SeqNo1);
      if (strlen(SN1) < SEQNOSIZE) {
        len = strlen(SN1);
        memcpy(SN1 + SEQNOSIZE - len, SN1, len);
        memset(SN1, '0', SEQNOSIZE - len);
      }
      memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
      memcpy(buf1 + HEADER, filename, strlen(filename));
      serverlen = sizeof(serveraddr);
      n = sendto(sockfd, buf1, strlen(buf1), 0,
                 (struct sockaddr *) &serveraddr, serverlen);
      if (n < 0) 
        error("ERROR in sendto");
      
      comp = 0;
      
      while (comp == 0) {
        bzero(buf, BUFSIZE);
        retry = 0;
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *) &serveraddr, &serverlen);
        if (n < 0) {
          retry = 1;
        }
        if (retry != 1) {
          bzero(msgtype, MSGTYPESIZE + 1);
          memcpy(msgtype, buf, MSGTYPESIZE);

          bzero(SN, SEQNOSIZE + 1);
          memcpy(SN, buf + MSGTYPESIZE, SEQNOSIZE);
          SeqNo = atoi(SN);
          if (DEBUG)
            printf("Client received %d/%d bytes from server with %s Seqno %d\n", (int) strlen(buf), n, msgtype, SeqNo);
        }
        if (strcmp(msgtype, "ack") == 0) {
          // Received acknowledgement for initial get message
          if (SeqNo == SeqNo1) {
            SeqNo1 = (SeqNo1 + 1) % 100000;
            fp = fopen(filename, "w+");
            fseek(fp, 0, SEEK_SET);
          } else {
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in sendto");
          }
        } else if (strcmp(msgtype, "ftr") == 0) {
          // Received part of the file. Send acknowledgement.
          if (CSN < 0 || SeqNo == (CSN + 1) % 100000) {
            CSN = SeqNo;
            
            bzero(buf1, BUFSIZE);
            strcpy(msgtype1, "ack");
            memcpy(buf1, msgtype1, MSGTYPESIZE);
            memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in ack sendto");
            
            bzero(msg, MSGSIZE + 1);
            memcpy(msg, buf + HEADER, MSGSIZE);
            
            fwrite(msg, strlen(msg), 1, fp);
          } else {
            // Resend acknowledgement
            bzero(buf1, BUFSIZE);
            strcpy(msgtype1, "ack");
            memcpy(buf1, msgtype1, MSGTYPESIZE);
            sprintf(SN, "%d", CSN);
            memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in ack sendto");
          }
        } else if (strcmp(msgtype, "fte") == 0) {
          // Received end of the file. Send acknowledgement and close the file.
          if (CSN < 0 || SeqNo == (CSN + 1) % 100000) {
            CSN = SeqNo;
            
            bzero(buf1, BUFSIZE);
            strcpy(msgtype1, "ack");
            memcpy(buf1, msgtype1, MSGTYPESIZE);
            memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in ack sendto");
            
            bzero(msg, MSGSIZE + 1);
            memcpy(msg, buf + HEADER, MSGSIZE);
            
            fwrite(msg, strlen(msg), 1, fp);
            fclose(fp);
            comp = 1;
            CSN = -1;
          } else {
            // Resend acknowledgement
            bzero(buf1, BUFSIZE);
            strcpy(msgtype1, "ack");
            memcpy(buf1, msgtype1, MSGTYPESIZE);
            sprintf(SN, "%d", CSN);
            memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in ack sendto");
          }
        }
      }
      
    } else if (strcmp(msgtype1, "put") == 0) {
      // Initial put message. This will read from the file and initiate a send from the client
      // to the server after the server acknowledgement.
      bzero(filename, 20);
      printf("\nPlease enter the filename:\n");
      scanf("%s", filename);
      bzero(buf1, BUFSIZE);
      memcpy(buf1, msgtype1, MSGTYPESIZE);
      sprintf(SN1, "%d", SeqNo1);
      if (strlen(SN1) < SEQNOSIZE) {
        len = strlen(SN1);
        memcpy(SN1 + SEQNOSIZE - len, SN1, len);
        memset(SN1, '0', SEQNOSIZE - len);
      }
      memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
      memcpy(buf1 + HEADER, filename, strlen(filename));
      serverlen = sizeof(serveraddr);
      n = sendto(sockfd, buf1, strlen(buf1), 0,
                 (struct sockaddr *) &serveraddr, serverlen);
      if (n < 0) 
        error("ERROR in sendto");
      
      fp = fopen(filename, "r");
      fseek(fp, 0, SEEK_SET);
      comp = 0;
      
      while (comp == 0) {
        bzero(buf, BUFSIZE);
        retry = 0;
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *) &serveraddr, &serverlen);
        if (n < 0) 
          retry = 1;
        bzero(msgtype, MSGTYPESIZE + 1);
        memcpy(msgtype, buf, MSGTYPESIZE);

        bzero(SN, SEQNOSIZE + 1);
        memcpy(SN, buf + MSGTYPESIZE, SEQNOSIZE);
        SeqNo = atoi(SN);
        if (DEBUG)
          printf("Client received %d/%d bytes from server with %s Seqno %d\n", (int) strlen(buf), n, msgtype, SeqNo);
        if (strcmp(msgtype, "ack") == 0) {
          // Received acknowledgement
          if (SeqNo == SeqNo1) {
            SeqNo1 = (SeqNo1 + 1) % 100000;
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
                        (struct sockaddr *) &serveraddr, serverlen);
              if (DEBUG)
                printf("Client sent %s %d to server after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
              if (n < 0) 
                error("ERROR in fte sendto");
              fclose(fp);
              comp = 1;
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
                        (struct sockaddr *) &serveraddr, serverlen);
              if (DEBUG)
                printf("Client sent %s %d to server after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
              if (n < 0) 
                error("ERROR in ftr sendto");
            }
          } else {
            // Resend packet as received Seqno does not match sent Seqno
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (DEBUG)
              printf("Client resent buf1 to server SeqNo = %d SeqNo1 = %d\n", SeqNo, SeqNo1);
            if (n < 0) 
              error("ERROR in sendto");
          }
        } else {
          // Resend packet as nothing was received
          n = sendto(sockfd, buf1, strlen(buf1), 0,
                     (struct sockaddr *) &serveraddr, serverlen);
        }
      }   
    } else if (strcmp(msgtype1, "del") == 0) {
      // Initial delete message. This will send the filename of the file to be deleted
      // and wait for acknowledgement.
      bzero(filename, 20);
      printf("\nPlease enter the filename:\n");
      scanf("%s", filename);
      bzero(buf1, BUFSIZE);
      memcpy(buf1, msgtype1, MSGTYPESIZE);
      sprintf(SN1, "%d", SeqNo1);
      if (strlen(SN1) < SEQNOSIZE) {
        len = strlen(SN1);
        memcpy(SN1 + SEQNOSIZE - len, SN1, len);
        memset(SN1, '0', SEQNOSIZE - len);
      }
      memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
      memcpy(buf1 + HEADER, filename, strlen(filename));
      serverlen = sizeof(serveraddr);
      n = sendto(sockfd, buf1, strlen(buf1), 0,
                 (struct sockaddr *) &serveraddr, serverlen);
      if (n < 0) 
        error("ERROR in sendto");
      comp = 0;
      while (comp == 0) {
        bzero(buf, BUFSIZE);
        retry = 0;
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *) &serveraddr, &serverlen);

        if (n < 0) {
          retry = 1;
        }
        if (!retry) {
          bzero(msgtype, MSGTYPESIZE + 1);
          memcpy(msgtype, buf, MSGTYPESIZE);

          bzero(SN, SEQNOSIZE + 1);
          memcpy(SN, buf + MSGTYPESIZE, SEQNOSIZE);
          SeqNo = atoi(SN);
          if (DEBUG)
            printf("Client received %d/%d bytes from server with %s Seqno %d\n", (int) strlen(buf), n, msgtype, SeqNo);
          if (SeqNo == SeqNo1) {
            SeqNo1 = (SeqNo1 + 1) % 100000;
            comp = 1;
          }
        }
      }
    } else if (strcmp(msgtype1, "lis") == 0) {
      // Initial list message. This will initiate a send from the server back
      // to the client with the list of files at the server.
      bzero(buf1, BUFSIZE);
      memcpy(buf1, msgtype1, MSGTYPESIZE);
      sprintf(SN1, "%d", SeqNo1);
      if (strlen(SN1) < SEQNOSIZE) {
        len = strlen(SN1);
        memcpy(SN1 + SEQNOSIZE - len, SN1, len);
        memset(SN1, '0', SEQNOSIZE - len);
      }
      memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
      serverlen = sizeof(serveraddr);
      n = sendto(sockfd, buf1, strlen(buf1), 0,
                 (struct sockaddr *) &serveraddr, serverlen);
      if (n < 0) 
        error("ERROR in sendto");
      comp = 0;
      while (comp == 0) {
        bzero(buf, BUFSIZE);
        retry = 0;
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *) &serveraddr, &serverlen);
        if (n < 0) {
          retry = 1;
        }
        if (retry != 1) {
          bzero(msgtype, MSGTYPESIZE + 1);
          memcpy(msgtype, buf, MSGTYPESIZE);

          bzero(SN, SEQNOSIZE + 1);
          memcpy(SN, buf + MSGTYPESIZE, SEQNOSIZE);
          SeqNo = atoi(SN);
          if (DEBUG)
            printf("Client received %d/%d bytes from server with %s Seqno %d\n", (int) strlen(buf), n, msgtype, SeqNo);
        }
        if (strcmp(msgtype, "ack") == 0) {
          // Received acknowledgement for initial list message
          if (SeqNo == SeqNo1) {
            SeqNo1 = (SeqNo1 + 1) % 100000;
            printf("The List of files on the server are:\n");
          } else {
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in sendto");
          }
        } else if (strcmp(msgtype, "lit") == 0) {
          // Received one filename, waiting for more
          if (CSN < 0 || SeqNo == (CSN + 1) % 100000) {
            CSN = SeqNo;
            
            bzero(buf1, BUFSIZE);
            strcpy(msgtype1, "acl");
            memcpy(buf1, msgtype1, MSGTYPESIZE);
            memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in acl sendto");
            
            bzero(msg, MSGSIZE + 1);
            memcpy(msg, buf + HEADER, MSGSIZE);
            if (strcmp(msg, ".") != 0 && strcmp(msg, "..") != 0 && strcmp(msg, "server") != 0
                && strcmp(msg, "uftp_server.c") != 0)
              printf("%s\n", msg);
          } else {
            // Resend acknowledgement
            bzero(buf1, BUFSIZE);
            strcpy(msgtype1, "acl");
            memcpy(buf1, msgtype1, MSGTYPESIZE);
            sprintf(SN, "%d", CSN);
            memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in acl sendto");
          }
        } else if (strcmp(msgtype, "lie") == 0) {
          // Received end of the list. End list operation.
          if (CSN < 0 || SeqNo == (CSN + 1) % 100000) {
            CSN = SeqNo;
            
            bzero(buf1, BUFSIZE);
            strcpy(msgtype1, "acl");
            memcpy(buf1, msgtype1, MSGTYPESIZE);
            memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in acl sendto");
            
            bzero(msg, MSGSIZE + 1);
            memcpy(msg, buf + HEADER, MSGSIZE);
            comp = 1;
            CSN = -1;
          } else {
            // Resend acknowledgement
            bzero(buf1, BUFSIZE);
            strcpy(msgtype1, "acl");
            memcpy(buf1, msgtype1, MSGTYPESIZE);
            sprintf(SN, "%d", CSN);
            memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
            if (n < 0) 
              error("ERROR in acl sendto");
          }
        }
      }
    } else if (strcmp(msgtype1, "exi") == 0) {
      // Initial exit message. This will send the exit message
      // and wait for acknowledgement before exiting.
      bzero(buf1, BUFSIZE);
      memcpy(buf1, msgtype1, MSGTYPESIZE);
      sprintf(SN1, "%d", SeqNo1);
      if (strlen(SN1) < SEQNOSIZE) {
        len = strlen(SN1);
        memcpy(SN1 + SEQNOSIZE - len, SN1, len);
        memset(SN1, '0', SEQNOSIZE - len);
      }
      memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
      serverlen = sizeof(serveraddr);
      n = sendto(sockfd, buf1, strlen(buf1), 0,
                 (struct sockaddr *) &serveraddr, serverlen);
      if (n < 0) 
        error("ERROR in sendto");
      comp = 0;
      while (comp == 0) {
        bzero(buf, BUFSIZE);
        retry = 0;
        n = recvfrom(sockfd, buf, BUFSIZE, 0,
                     (struct sockaddr *) &serveraddr, &serverlen);

        if (n < 0) {
          retry = 1;
        }
        if (!retry) {
          bzero(msgtype, MSGTYPESIZE + 1);
          memcpy(msgtype, buf, MSGTYPESIZE);

          bzero(SN, SEQNOSIZE + 1);
          memcpy(SN, buf + MSGTYPESIZE, SEQNOSIZE);
          SeqNo = atoi(SN);
          if (DEBUG)
            printf("Client received %d/%d bytes from server with %s Seqno %d\n", (int) strlen(buf), n, msgtype, SeqNo);
          if (SeqNo == SeqNo1) {
            SeqNo1 = (SeqNo1 + 1) % 100000;
            comp = 1;
          }
        }
      }
      running = 0;
    } else {
      printf("Inavlid Command\n");
    }
  }
  return 0;
}
