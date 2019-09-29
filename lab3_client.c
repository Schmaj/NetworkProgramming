//#include	"unp.h"


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h> 
#include <sys/select.h>


#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#include <ctype.h> // isalnum()

#define TIMEOUT 10

int
main(int argc, char **argv)
{

  // index of next available socket
  int socknum = 0;

	int					sockfd1;
  int					sockfd2;
  int					sockfd3;
  int					sockfd4;
  int					sockfd5;

	struct sockaddr_in	servaddr;
    char* buffer[251];
    bzero(buffer, 251);

	//if (argc != 2)
  		//perror("usage: tcpcli <IPaddress>");

  // create 5 sockets, max number of connections
	sockfd1 = socket(AF_INET, SOCK_STREAM, 0);
  sockfd2 = socket(AF_INET, SOCK_STREAM, 0);
  sockfd3 = socket(AF_INET, SOCK_STREAM, 0);
  sockfd4 = socket(AF_INET, SOCK_STREAM, 0);
  sockfd5 = socket(AF_INET, SOCK_STREAM, 0);

  // array to refer to socket descriptor by index
  int socks[5];
  socks[0] = sockfd1;
  socks[1] = sockfd2;
  socks[2] = sockfd3;
  socks[3] = sockfd4;
  socks[4] = sockfd5;

  int ports[5];

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

 // scanf("%d\n", &sockfd);
	//Inet_pton(AF_INET, sockfd, &servaddr.sin_addr);

  while(1){

    fd_set rfds;

    FD_ZERO(&rfds);
    FD_SET(sockfd1, &rfds);
    FD_SET(sockfd2, &rfds);
    FD_SET(sockfd3, &rfds);
    FD_SET(sockfd4, &rfds);
    FD_SET(sockfd5, &rfds);
    FD_SET(0, &rfds);

    // structure to specify TIMEOUT second timeout on select() call
    struct timeval timeout;

    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    int num = select(6, &rfds, NULL, NULL, &timeout);

    // if there is data on stdin, bind next socket to given port
    if( FD_ISSET(0, &rfds)){
      // read port number from stdin
      int port;
      scanf("%d", &port);

      ports[socknum] = port;

      servaddr.sin_port = htons(port);

      // bind next tcp socket to given port
      int rc = bind(socks[socknum], (struct sockaddr*) &servaddr, sizeof(struct sockaddr));


      // set address to localhost to connect to server
      servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
      // connect to server
      connect(socks[socknum], (struct sockaddr*)&servaddr, sizeof(struct sockaddr_in));
      // restore to any ip for next bind
      servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

      // increment sock index
      socknum++;



    }

    for(int n = 0; n < 5; n++){
      if(FD_ISSET(socks[n], &rfds)){

        struct sockaddr_in client;
        int fromLen;

        // receive a message from server
        int bytesRead = recvfrom(socks[n], buffer, 250, 0, (struct sockaddr*)&client, &fromLen);

        if(bytesRead > 0){
          // print to stdout
          // null terminate because string will be printed
          buffer[bytesRead] = '\0';

          printf("%d %s", ports[n], buffer);
          bzero(buffer, 251);
        }
        if(bytesRead == 0){
          // connection closed
          printf("Server on %d closed\n", ports[n]);
          //close(socks[n]);
          socks[n] = 0;
        }
      }
    }
/*
    if ( FD_ISSET(sockfd, &readfds)){
        / read a datagram from the remote client side (BLOCKING) /
        n = recv( sockfd, buffer, 250, 0, (struct sockaddr *) &client,
                      (socklen_t *) &fromlen );

        if ( n == -1 ){
          perror( "recvfrom() failed" );
        }else{
          printf( "MAIN: Rcvd incoming UDP datagram from: %s\n",
                  inet_ntoa( client.sin_addr ));

          //printf( "RCVD %d bytes\n", n );
          buffer[n] = '\0';   /* assume that its printable char[] data /

        }
    }
    */
  }


	exit(0);
}
