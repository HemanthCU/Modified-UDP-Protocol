/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
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

/*void waitFor (unsigned int secs) {
    unsigned int retTime = time(0) + secs;   // Get finishing time.
    while (time(0) < retTime);               // Loop until it arrives.
}*/

int main(int argc, char **argv) {
  int sockfd, portno, n;
  int serverlen;
  struct sockaddr_in serveraddr;
  struct hostent *server;
  char *hostname;
  char buf[BUFSIZE];
  char buf1[BUFSIZE]; /* message buf1 */
  char msgtype[MSGTYPESIZE + 1];
  char msgtype1[MSGTYPESIZE + 1];
  char SN[SEQNOSIZE + 1];
  char SN1[SEQNOSIZE + 1];
  char msg[MSGSIZE + 1];
  char filename[20];
  int SeqNo; /* Sequence number of the received packet */
  int CSN = -1; /* Cumulative sequence number to track which packets arrived */
  int SeqNo1 = 20000; /* Sequence number of the packet */
  int msgsz;
  int comp;
  int testcount = 1;
  int retry = 0;
  FILE *fp;


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
  tv.tv_sec = 10;
  tv.tv_usec = 0;
  /*setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
             (struct timeval *)&tv,sizeof(struct timeval));*/

  /* build the server's Internet address */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
    (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno);

  /* get a message from the user */
  bzero(msgtype1, MSGTYPESIZE);
  printf("Please enter the command:\n");
  fgets(msgtype1, MSGTYPESIZE + 1, stdin);

  if (strcmp(msgtype1, "get") == 0) {
    bzero(filename, 20);
    printf("\nPlease enter the filename:\n");
    //fgets(filename, 20, stdin);

    scanf("%s", filename);
    bzero(buf1, BUFSIZE);
    memcpy(buf1, msgtype1, MSGTYPESIZE);
    sprintf(SN1, "%d", SeqNo1);
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
        printf("Client received %d/%d bytes from server with Seqno %d\n", (int) strlen(buf), n, SeqNo);
      }
      if (strcmp(msgtype, "ack") == 0) {
        if (SeqNo == SeqNo1) {
          SeqNo1++;
          fp = fopen(filename, "w+");
          fseek(fp, 0, SEEK_SET);
        } else {
          n = sendto(sockfd, buf1, strlen(buf1), 0,
                     (struct sockaddr *) &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in sendto");
        }
      } else if (strcmp(msgtype, "ftr") == 0) {
        if (CSN < 0 || SeqNo == CSN + 1) {
          CSN = SeqNo;
          
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "ack");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
          /*if (testcount == 1) {
            waitFor(12);
            testcount--;
          }*/
          if (0 && SeqNo % 500 == 2) {
            testcount++;
          } else {
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                       (struct sockaddr *) &serveraddr, serverlen);
          }
          if (n < 0) 
            error("ERROR in ack sendto");
          
          bzero(msg, MSGSIZE + 1);
          memcpy(msg, buf + HEADER, MSGSIZE);
          
          fwrite(msg, strlen(msg), 1, fp);
          //fseek(fp, MSGSIZE, SEEK_CUR);
        } else if (SeqNo > CSN + 1) {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "ack");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN, "%d", CSN);
          memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0,
                     (struct sockaddr *) &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in ack sendto");
        } else {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "ack");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0,
                     (struct sockaddr *) &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in ack sendto");
        }
      } else if (strcmp(msgtype, "fte") == 0) {
        if (CSN < 0 || SeqNo == CSN + 1) {
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
        } else if (SeqNo > CSN + 1) {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "ack");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          sprintf(SN, "%d", CSN);
          memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0,
                     (struct sockaddr *) &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in ack sendto");
        } else {
          bzero(buf1, BUFSIZE);
          strcpy(msgtype1, "ack");
          memcpy(buf1, msgtype1, MSGTYPESIZE);
          memcpy(buf1 + MSGTYPESIZE, SN, SEQNOSIZE);
          n = sendto(sockfd, buf1, strlen(buf1), 0,
                     (struct sockaddr *) &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in ack sendto");
        }
      }
    }
    
  } else if (strcmp(msgtype1, "put") == 0) {
    bzero(filename, 20);
    printf("\nPlease enter the filename:\n");
    //fgets(filename, 20, stdin);

    scanf("%s", filename);
    bzero(buf1, BUFSIZE);
    memcpy(buf1, msgtype1, MSGTYPESIZE);
    sprintf(SN1, "%d", SeqNo1);
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
      printf("Client received %d/%d bytes from server with Seqno %d\n", (int) strlen(buf), n, SeqNo);
      if (strcmp(msgtype, "ack") == 0) {
        if (SeqNo == SeqNo1) {
          SeqNo1++;
          bzero(msg, MSGSIZE + 1);
          msgsz = (int) fread(msg, MSGSIZE, 1, fp);

          if (msgsz == 0/* End of file */) {
            bzero(buf1, BUFSIZE);
            strcpy(msgtype1, "fte");
            memcpy(buf1, msgtype1, MSGTYPESIZE);
            sprintf(SN1, "%d", SeqNo1);
            memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
            memcpy(buf1 + HEADER, msg, MSGSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                      (struct sockaddr *) &serveraddr, serverlen);
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
            memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
            memcpy(buf1 + HEADER, msg, MSGSIZE);
            n = sendto(sockfd, buf1, strlen(buf1), 0,
                      (struct sockaddr *) &serveraddr, serverlen);
            printf("Client sent %s %d to server after getting %s %d\n", msgtype1, SeqNo1, msgtype, SeqNo);
            if (n < 0) 
              error("ERROR in ftr sendto");
          }
        } else {
          n = sendto(sockfd, buf1, strlen(buf1), 0,
                     (struct sockaddr *) &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in sendto");
        }
      } else {
        n = sendto(sockfd, buf1, strlen(buf1), 0,
                   (struct sockaddr *) &serveraddr, serverlen);
      }
    }   
  } else if (strcmp(msgtype1, "del") == 0) {

  } else if (strcmp(msgtype1, "lis") == 0) {

  } else if (strcmp(msgtype1, "exi") == 0) {

  } else {
    error("ERROR message sent");
  }
  /* send the message to the server */
  /*serverlen = sizeof(serveraddr);
  n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
  if (n < 0) 
    error("ERROR in sendto");*/

  /* print the server's reply */
  /*n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
  if (n < 0) 
    error("ERROR in recvfrom");
  printf("Echo from server: %s", buf);*/
  return 0;
}
