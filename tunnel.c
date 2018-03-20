/*
 * The following code uses the framework provided in sample.c
 */

/*
* This code "USC CSci551 FA2012 Projects A and B" is
* Copyright (C) 2012 by Zi Hu.
* All rights reserved.
*
* This program is released ONLY for the purposes of Fall 2012 CSci551
* students who wish to use it as part of their project assignments.
* Use for another other purpose requires prior written approval by
* Zi Hu.
*
* Use in CSci551 is permitted only provided that ALL copyright notices
* are maintained and that this code is distinguished from new
* (student-added) code as much as possible.  We new services to be
* placed in separate (new) files as much as possible.  If you add
* significant code to existing files, identify your new code with
* comments.
*
* As per class assignments, use of any code OTHER than this provided
* code requires explicit approval, ahead of time, by the professor.
*
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/if_tun.h>
#include <linux/ip.h>
#include <linux/icmp.h>
#include <linux/kernel.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include "router.h"
/**************************************************************************
 * tun_alloc: allocates or reconnects to a tun/tap device. 
 * copy from simpletun.c
 * refer to http://backreference.org/2010/03/26/tuntap-interface-tutorial/ for more info 
 **************************************************************************/

int tun_alloc(char *dev, int flags) 
{
  struct ifreq ifr;
  int fd, err;
  char *clonedev = (char*)"/dev/net/tun";

  if( (fd = open(clonedev , O_RDWR)) < 0 ) {
    perror("Opening /dev/net/tun");
    return fd;
  }

  memset(&ifr, 0, sizeof(ifr));

  ifr.ifr_flags = flags;

  if (*dev) {
    strncpy(ifr.ifr_name, dev, IFNAMSIZ);
  }

  if( (err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0 ) {
    perror("ioctl(TUNSETIFF)");
    close(fd);
    return err;
  }

  strcpy(dev, ifr.ifr_name);
  return fd;
}


int tunnel_reader(char *filename, int proxysocket, struct sockaddr_in routeraddr[NUM_ROUTERS], socklen_t addrlen)
{
  char tun_name[IFNAMSIZ];
  char buffer[1000];
  fd_set readset, tempset;
  int max;

  /* Connect to the tunnel interface (make sure you create the tunnel interface first) */
  strcpy(tun_name, "tun1");
  int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI); 

  if(tun_fd < 0){
    perror("Open tunnel interface");
    exit(1);
  }

  FD_ZERO(&readset);
  FD_SET(proxysocket, &readset);
  FD_SET(tun_fd, &readset);
  if (tun_fd > proxysocket)
     max = tun_fd;
  else
     max = proxysocket;

  /*
   * Rewritten section with select loop that allows talking
   * to both the tun interface, AND to the router.
   */
  do {
    memcpy(&tempset, &readset, sizeof(tempset));
    if( select(max+1, &tempset, NULL, NULL, NULL)== SO_ERROR){
      perror("select error!\n");
      exit(1);
    }
    /* Now read data coming from the tunnel and router */
    if FD_ISSET(tun_fd, &tempset){
      int nread = read(tun_fd,buffer,sizeof(buffer));
      if(nread < 0) {
        perror("Reading from tunnel interface");
        close(tun_fd);
        exit(1);
      } else {
        buffer[nread] = 0;
        struct iphdr *ip = (struct iphdr*)buffer;
        

        int size = ntohs(ip->tot_len) - sizeof(struct iphdr); 
        char buffer1[size];
        memcpy(buffer1, buffer+sizeof(struct iphdr), size);
        struct icmphdr *icmp = (struct icmphdr*) buffer1;
        
        int router_num = 0;
      	if(NUM_ROUTERS == 1) router_num = 1;
      	else router_num = (__be32_to_cpu(ip->daddr) % NUM_ROUTERS)+1;
        if (ip->protocol == 1){
          if (sendto(proxysocket, buffer, sizeof(buffer),0,(struct sockaddr *)&routeraddr[router_num-1], addrlen)==-1){
            perror("sendto in tunnel.c\n");
            exit(1);
          }
          
          FILE *output = fopen(filename, "a");
          fprintf(output, "ICMP packet from tunnel, src:%u.%u.%u.%u, dst:%u.%u.%u.%u, type:%d\n",ip->saddr &0xff, ip->saddr>>8 &0xff, ip->saddr>>16 &0xff,ip->saddr>>24 &0xff, ip->daddr &0xff, ip->daddr>>8 &0xff, ip->daddr>>16 &0xff,ip->daddr >> 24 &0xff, icmp->type);
          fclose(output);
        }
      }
    }
    if FD_ISSET(proxysocket, &tempset){
      struct sockaddr_in theiraddr;
      int len = recvfrom(proxysocket, buffer,1000, 0, (struct sockaddr *)&theiraddr,&addrlen);
      if (len > 0){
        buffer[len] = 0;
        struct iphdr *ip = (struct iphdr*)buffer;
        
        int size = ntohs(ip->tot_len) - sizeof(struct iphdr); 
        char buffer1[size];
        memcpy(buffer1, buffer+sizeof(struct iphdr), size);
        struct icmphdr *icmp = (struct icmphdr*) buffer1;

        FILE *output = fopen(filename, "a");
        fprintf(output,"ICMP from port:%d, src:%u.%u.%u.%u, dst:%u.%u.%u.%u, type:%d\n",ntohs(theiraddr.sin_port), ip->saddr &0xff, ip->saddr>>8 &0xff, ip->saddr>>16 &0xff,ip->saddr>>24 &0xff, ip->daddr &0xff, ip->daddr>>8 &0xff, ip->daddr>>16 &0xff,ip->daddr >> 24 &0xff, icmp->type);
        fclose(output);

      	if(write(tun_fd, buffer, sizeof(struct iphdr)+sizeof(struct icmphdr))<0){
      	  perror("write failed");
        }
      }
    }
  }while(1);
}

int tunnel_reader2(char *filename, int proxysocket, struct sockaddr_in routeraddr[NUM_ROUTERS], socklen_t addrlen)
{
  char tun_name[IFNAMSIZ];
  char buffer[1000];
  fd_set readset, tempset;
  int max;

  /* Connect to the tunnel interface (make sure you create the tunnel interface first) */
  strcpy(tun_name, "tun1");
  int tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI); 

  if(tun_fd < 0){
    perror("Open tunnel interface");
    exit(1);
  }

  FD_ZERO(&readset);
  FD_SET(proxysocket, &readset);
  FD_SET(tun_fd, &readset);
  if (tun_fd > proxysocket)
     max = tun_fd;
  else
     max = proxysocket;

  int cur_hop = 0, sequence_num = 1;
  int router_num[MINITOR_HOPS];
  char store_packet_while_creating_circuit[1000];
  /*
   * Rewritten section with select loop that allows talking
   * to both the tun interface, AND to the routers.
   */
  do {
    memcpy(&tempset, &readset, sizeof(tempset));
    if( select(max+1, &tempset, NULL, NULL, NULL)== SO_ERROR){
      perror("select error!\n");
      exit(1);
    }
    /* Now read data coming from the tunnel and router */
    if FD_ISSET(tun_fd, &tempset){
      int nread = read(tun_fd,buffer,sizeof(buffer));
      if(nread < 0) {
        perror("Reading from tunnel interface");
        close(tun_fd);
        exit(1);
      } else {
        //setup to extract info
        buffer[nread] = '\0';
        struct iphdr *ip = (struct iphdr*)buffer;
        
        if (ip->protocol == 1){
          memcpy(store_packet_while_creating_circuit, buffer, sizeof(buffer));
          //find first hop
          if(NUM_ROUTERS == 1) router_num[cur_hop] = 1;
          else router_num[cur_hop] = (__be32_to_cpu(ip->daddr) % NUM_ROUTERS)+1;

          // write hops to file before resetting to build circuit
          FILE *output = fopen(filename, "a");
          int cur_router = router_num[cur_hop];
          while (cur_hop < MINITOR_HOPS){
            fprintf(output, "hop: %d, router: %d\n", cur_hop+1, cur_router);
            cur_router = (cur_router % NUM_ROUTERS) + 1;
            cur_hop += 1;
            router_num[cur_hop] = cur_router;
          }
          cur_hop = 0;
          fclose(output);

          //send first circuit extend control message
	  uint16_t id = (0*256) + sequence_num;

	  uint8_t tosend[5];

	  tosend[0] = 0x52;
	  tosend[1] = (id>>8)&0x00FF;
	  tosend[2] = (id)&0x00FF;
	  tosend[3] = routeraddr[router_num[cur_hop]-1].sin_port >> 8; 
	  tosend[4] = routeraddr[router_num[cur_hop]-1].sin_port & 0xFF; 
       
          char datagram[sizeof(struct iphdr)+sizeof(tosend)];
          bzero(datagram, sizeof(datagram));
          struct iphdr *mant_ip = (struct iphdr*)datagram;
          mant_ip->saddr = inet_addr("127.0.0.1");
          mant_ip->daddr = inet_addr("127.0.0.1");
          mant_ip->protocol = 253;
          memcpy(datagram+sizeof(struct iphdr), (unsigned char*)tosend, sizeof(tosend));

          if (sendto(proxysocket, datagram, sizeof(datagram),0,(struct sockaddr *)&routeraddr[router_num[cur_hop]-1], addrlen)==-1){
            perror("sendto in tunnel.c\n");
            exit(1);
          }       
        }
      }
    }
    if FD_ISSET(proxysocket, &tempset){
      // struct sockaddr_in theiraddr;
      // int len = recvfrom(proxysocket, buffer,1000, 0, (struct sockaddr *)&theiraddr,&addrlen);
      // if (len > 0){
      //   buffer[len] = 0;
      //   struct iphdr *ip = (struct iphdr*)buffer;
        
      //   int size = ntohs(ip->tot_len) - sizeof(struct iphdr); 
      //   char buffer1[size+1];
      //   memcpy(buffer1, buffer+sizeof(struct iphdr), size);
      //   struct icmphdr *icmp = (struct icmphdr*) buffer1;

      //   FILE *output = fopen(filename, "a");
      //   fprintf(output,"ICMP from port:%d, src:%u.%u.%u.%u, dst:%u.%u.%u.%u, type:%d\n",ntohs(theiraddr.sin_port), ip->saddr &0xff, ip->saddr>>8 &0xff, ip->saddr>>16 &0xff,ip->saddr>>24 &0xff, ip->daddr &0xff, ip->daddr>>8 &0xff, ip->daddr>>16 &0xff,ip->daddr >> 24 &0xff, icmp->type);
      //   fclose(output);

      //   if(write(tun_fd, buffer, sizeof(struct iphdr)+sizeof(struct icmphdr))<0){
      //     perror("write failed");
      //   }
      // }
    }
  }while(1);
}
