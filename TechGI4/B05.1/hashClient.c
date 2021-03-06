/*
############################################################################
#                                                                          #
# Copyright TU-Berlin, 2011-2014                                           #
# Die Weitergabe, Veröffentlichung etc. auch in Teilen ist nicht gestattet #
#                                                                          #
############################################################################
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "common.h"


int main(int argc, char *argv[])
{
    struct hostent *he;
    int sockfd; 
    struct sockaddr_in their_addr;
    int numbytes;
    int serverPort;
     sec s2,s3;
    nsec n2,n3;
    printf("Hash client\n\n");
    
    if (argc != 3) {
        fprintf(stderr,"Usage: hashClient serverName serverPort\n");
        exit(1);
    }
    
    serverPort = atoi(argv[2]);

    if ((he=gethostbyname(argv[1])) == NULL) {  // get the host info
        herror("gethostbyname");
        exit(1);
    }
    
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    their_addr.sin_family = AF_INET;     
    their_addr.sin_port = htons(serverPort);
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);
    char buffer[PACKLEN];
    char command[4];
    packData(buffer, "REQ", 0,0,0,0);
    sendto(sockfd, buffer, PACKLEN, 0, (struct sockaddr *)&their_addr, sizeof(struct sockaddr_in));
    recvfrom(sockfd, buffer, PACKLEN, 0, NULL, NULL);
    unpackData(buffer, command, &s2,&n2,&s3,&n3);
    return 0;
}

