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
 * For stages 1 to 4
 * The function reads from the tunnel and contains the select loop implementation for the tunnel and proxysocket
 */
int tunnel_reader(char *filename, int proxysocket, struct sockaddr_in* routeraddr, socklen_t addrlen);

/*
 * For stages 5 and 6
 * The function reads from the tunnel and contains the select loop implementation for the tunnel and proxysocket
 * The proxysocket sectioon of the select loop in this function contains steps for handling the mantitor control packets.
 */
int tunnel_reader2(char *filename, int proxysocket, struct sockaddr_in* routeraddr, socklen_t addrlen);

#endif
