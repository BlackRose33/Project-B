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
int ROUTER_PORT_NUM;

#define BUFSIZE 2048

void run_router();

#endif
