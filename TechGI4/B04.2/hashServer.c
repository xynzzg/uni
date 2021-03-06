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
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "time.h"
#include "common.h"
struct entry {
    uint16_t key;
    uint16_t val;
    uint16_t valid;
};
struct entry table[TABLESIZE];
/*****************/
struct node {
    uint32_t ip;
    uint16_t port;
    struct sockaddr_in addr;
    key id;
    key prev;
    int valid;
};
struct node nodes[FINGERTABLE_LEN+1];
/******************/

void print_nodes() {
    int i;
    printf("\ni | id | prev | port | valid\n");
    for(i = 0; i < FINGERTABLE_LEN; i++) {
        printf("%i | %i | %i| %i | %i\n",i, nodes[i].id, nodes[i].prev, nodes[i].port, nodes[i].valid);
    }
}
int getPos(uint16_t key) {
    int pos = key & LEN;
    while(1) {
        if(table[pos].valid == 1 && table[pos].key == key) {
            //printf("getPos return \n",pos);
            return pos;
        }
        pos = (pos + 1) % TABLESIZE;
        if(pos == (key & LEN)) break;
    }
    //printf("getPos not found\n");
    return -1;
}
struct entry* get(key key) {
    //printf("get %i\n", key);
    int pos = getPos(key);
    if(pos >= 0)  {
        return &table[pos];
    }
    return NULL;
}

void set(key key, uint16_t val) {
    //printf("set %i %i\n", key, val);
    int pos = key & LEN;
    while(1) {
        if(table[pos].valid == 0) {
            break;
        }
        pos = (pos + 1) % TABLESIZE;
        if(pos == (key & LEN)) return;

    }
    table[pos].valid = 1;
    table[pos].key = key;
    table[pos].val = val;
}

void del(uint16_t key) {
    //printf("del %i\n", key);
    int pos = getPos(key);
    if(pos >= 0)  {
        table[pos].valid = 0;
    }
}

int getNode(key key)
{
    printf("getNode(%i)", key);
    //print_nodes();
    int i;
    for(i = 0; i < FINGERTABLE_LEN; i++) {
        struct node n = nodes[i];
        if(n.valid == 0) continue;
        if(n.id > n.prev){
            if(n.id >= key && n.prev < key) {
                printf("[%i].id = %i, [%i].prev = %i; key = %i *\n",i,nodes[i].id, i, nodes[i].prev, key);
                return i;
            }
        } else {
            if(n.id >= key && n.prev > key) {
                printf("[%i].id = %i, [%i].prev = %i; key = %i **\n",i,nodes[i].id, i, nodes[i].prev, key);
                return i;
            }
        }
    }
    printf("getNode(%i) nothing found\n",key);
    return 0;
}
int createNode(int node, int ip, int port, key id, key prev)
{
    nodes[node].id = id;
    nodes[node].valid = 1;
    nodes[node].prev = prev;
    nodes[node].port = port;
    nodes[node].ip = ip;
    nodes[node].addr.sin_family = AF_INET;     
    nodes[node].addr.sin_port = htons(port);
    nodes[node].addr.sin_addr.s_addr = ip;
    
}
int createNode2(int node, char *ip, int port, key id, key prev)
{
    createNode(node, inet_addr(ip), port, id, prev);
    inet_aton(ip, &nodes[node].addr.sin_addr);
}
int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in serv_addr, cli_addr;
    int clilen;
    
    time_t start;
    time_t now;

    printf("UDP Server\n\n");
    int build_fingertable = 0;
    int i;
    for(i = 0; i< TABLESIZE; i++) {
        table[i].key = 0;
        table[i].val = 0;
        table[i].valid = 0;
    }
    for(i = 0; i< 4;i++) {
    	nodes[i].valid=0;
    }
    if (argc != 10) {
        fprintf(stderr,"Usage: hashServer serverIP serverPort serverID prevIP prevPort prevID nextIP nextPort nextID\n");
        exit(1);
    }
    createNode2(0, argv[1], atoi(argv[2]), atoi(argv[3]), 0);
    
    createNode2(2, argv[7], atoi(argv[8]), atoi(argv[9]), nodes[0].id);
    createNode2(1, argv[4], atoi(argv[5]), atoi(argv[6]), nodes[2].id);
    
    nodes[0].prev = nodes[1].id;//ring structure

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(atoi(argv[2]));

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("could not bind\n");
    }

    listen(sockfd, 10);
    time(&start);
    while(1) {
        unsigned char buffer[PACKLEN];
        if(build_fingertable == 0) {
            time(&now);
            double seconds;
            seconds = difftime(now, start);
            if(seconds > 10) {
                printf("[%i] builing fingertable\n", nodes[0].id);
                int i;
                for(i = 1; i< FINGERTABLE_LEN; i++) {
                    int key = calc_fingertable_key(i);
                    int n = getNode(key);
                    if(isMe(n)) continue;
                    printf("[%i] sending fingertable request to %i with key=%i\n",nodes[0].id,nodes[n].id,key);
                    packData(buffer, "GET", key, 0, nodes[0].ip, nodes[0].port);
                    sendto(sockfd, buffer, PACKLEN, 0, (struct sockaddr *)&(nodes[n].addr), sizeof(struct sockaddr_in));

                }
                build_fingertable = 1;
                continue;
            }
        }
        clilen = sizeof cli_addr;
        int size = recvfrom(sockfd, buffer, PACKLEN, 0,(struct sockaddr *) &cli_addr, &clilen);
        if(size > 0) {
            key key;
            value value;
            uint32_t ip;
            uint16_t port;
            char command[4];
            unpackData(buffer, command, &key, &value, &ip, &port);
            int n = getNode(key);
            if(!isMe(n)) {
                printf("[%i] send key=%i to node=%i\n",nodes[0].id, key, n);
                sendto(sockfd, buffer, PACKLEN, 0, (struct sockaddr *)&(nodes[n].addr), sizeof(nodes[n].addr));
                continue;
            }
            printf("[%i] i am serving key=%i \n",nodes[0].id, key);
            
            if(command[0] == 'S') {
                set(key, value);
                packData(buffer, "OK!", 0, 0,nodes[0].ip, nodes[0].port);
            } else if(command[0] == 'G') {
                struct entry *ret = get(key);
                if(ret == 0) {
                    packData(buffer, "NOF", key, 0, nodes[0].ip, nodes[0].port);
                } else {
                    packData(buffer, "VAL", ret->key, ret->val, nodes[0].ip, nodes[0].port);
                }
            } else if(command[0] == 'D') {
                del(key);
                packData(buffer, "OK!", 0, 0,nodes[0].ip, nodes[0].port);
            } else if(command[0] == 'N' || command[0] == 'V') {
                printf("[%i]got fingertable response key = %i, port = %i\n",nodes[0].id, key, port);
                //update fingertable
                int pos = calc_fingertable_pos(key);
                int next = getNode(pos);
                createNode(pos+3, ip, port, key, nodes[next].prev);
                nodes[next].prev = key;
                continue;
            }
            
            struct sockaddr_in cli;
            cli.sin_family = AF_INET;     
            cli.sin_port = htons(port);
            cli.sin_addr.s_addr = ip;
			sendto(sockfd, buffer, PACKLEN, 0, (struct sockaddr *) &cli, sizeof(struct sockaddr_in));
        }
    }
    close(sockfd);
    return 0;
}
int isMe(int pos)
{
    return nodes[pos].port == nodes[0].port;
}





