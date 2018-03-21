#ifndef ROUTER_H
#define ROUTER_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <arpa/inet.h>

//globals
int STAGE;
int NUM_ROUTERS;
int MINITOR_HOPS;
int PROXY_PORT_NUM;

#define BUFSIZE 2048

/*
 * Starter function thats creates relevant sockets and starts the select loop.
 * Contains two separate select loops, one for stages 1 to 4, and the other
 * for stages 5 and 6.
 */
void run_router(int cur_router, char* interface, char *ip);

#endif
