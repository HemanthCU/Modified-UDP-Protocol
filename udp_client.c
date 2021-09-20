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
  char filename[1000];
  int SeqNo; /* Sequence number of the received packet */
  int SeqNo1 = 20000; /* Sequence number of the packet */
  int msgsz;
  int comp;
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

  /* build the server's Internet address */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
    (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno);

  /* get a message from the user */
  bzero(msgtype1, MSGTYPESIZE + 1);
  printf("Please enter the command:\n");
  fgets(msgtype1, MSGTYPESIZE, stdin);

  if (strcmp(msgtype, "get") == 0) {
    bzero(filename, 1000);
    printf("\nPlease enter the filename:\n");
    fgets(filename, 1000, stdin);
    
    bzero(buf1, BUFSIZE);
    memcpy(buf1, msgtype1, MSGTYPESIZE);
    sprintf(SN1, "%d", SeqNo1);
    memcpy(buf1 + MSGTYPESIZE, SN1, SEQNOSIZE);
    memcpy(buf1 + HEADER, filename, MSGSIZE);
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf1, strlen(buf1), 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    
    comp = 0;
    
    while (comp == 0) {
      n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
      if (n < 0) 
        error("ERROR in recvfrom");
      bzero(buf, BUFSIZE);
      memcpy(msgtype, buf, MSGTYPESIZE);

      bzero(SN, SEQNOSIZE + 1);
      memcpy(SN, buf + MSGTYPESIZE, SEQNOSIZE);
      SeqNo = atoi(SN);
      if (strcmp(msgtype, "ack") == 0) {
        if (SeqNo == SeqNo1)
          SeqNo1++;
        else {
          n = sendto(sockfd, buf1, strlen(buf1), 0, &serveraddr, serverlen);
          if (n < 0) 
            error("ERROR in sendto");
        }
      } else if (strcmp(msgtype, "ftr") == 0) {

      } else if (strcmp(msgtype, "fte") == 0) {
        comp = 1;
      }
    }
    
    
  } else if (strcmp(msgtype, "put") == 0) {
    
  } else if (strcmp(msgtype, "del") == 0) {

  } else if (strcmp(msgtype, "lis") == 0) {

  } else if (strcmp(msgtype, "exi") == 0) {

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
