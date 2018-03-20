#ifndef TUNNEL_H
#define TUNNEL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

/*
 *
 */
int tunnel_reader(char *filename, int proxysocket, struct sockaddr_in* routeraddr, socklen_t addrlen);

#endif
