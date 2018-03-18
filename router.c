#include <linux/ip.h>
#include <linux/icmp.h>
#include "router.h"
#include <errno.h>

int router_port_num;

/*
 *
 */
unsigned short cksum(unsigned short *ptr, int nbytes){
  register long sum;
  u_short oddbyte;
  register u_short answer;

  sum = 0;
  while(nbytes>1){
    sum += *ptr++;
    nbytes -= 2;
  }
  if(nbytes == 1){
    oddbyte = 0;
    *((u_char *)&oddbyte) = *(u_char *)ptr;
    sum += oddbyte;
  }
  sum = (sum >> 16)+(sum &0xffff);
  sum += (sum >>16);
  answer = ~sum;

  return answer;
}


/*
 * Create the raw_socket, binding it to a specific network interface and the 
 * ip of the interface. 
 */
int create_raw_socket(char* interface, char *ip){
  int raw_socket, rc;
  struct ifreq ifr;
  struct sockaddr_in routeraddr;
  socklen_t addrlen = sizeof(struct sockaddr_in);
  
  if ((raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
    perror("cannot create raw socket");
    exit(1);
  }

  memset(&ifr, 0, sizeof(ifr));
  snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), interface);
  if ((rc = setsockopt(raw_socket, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr))) < 0)
  {
      perror("Server-setsockopt() error for SO_BINDTODEVICE");
      close(raw_socket);
      exit(1);
  }

  memset(&routeraddr, 0, sizeof(routeraddr));
  routeraddr.sin_family = AF_INET;
  routeraddr.sin_port = htons(0);
  routeraddr.sin_addr.s_addr = inet_addr(ip);
  if (bind(raw_socket, (struct sockaddr *)&routeraddr, sizeof(routeraddr)) < 0) {
    perror("bind failed");
    close(raw_socket);
    exit(1);
  } 

  if (getsockname(raw_socket, (struct sockaddr *)&routeraddr, &addrlen) < 0 ){
    perror("getsockname failed");
    close(raw_socket);
    exit(1);
  }
  return raw_socket;
}

/*
 * Create the udp socket
 */
int create_udp_socket(char* ip){
  int routersocket;
  struct sockaddr_in routeraddr;
  socklen_t addrlen = sizeof(struct sockaddr_in);

  if ((routersocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("cannot create udp socket");
    exit(1);
  }

  memset(&routeraddr, 0, sizeof(routeraddr));
  routeraddr.sin_family = AF_INET;
  routeraddr.sin_port = htons(0);
  routeraddr.sin_addr.s_addr = inet_addr(ip);

  if (bind(routersocket, (struct sockaddr *)&routeraddr, sizeof(routeraddr)) < 0) {
    perror("bind failed");
    close(routersocket);
    exit(1);
  } 
  if (getsockname(routersocket, (struct sockaddr *)&routeraddr, &addrlen) < 0 ){
    perror("getsockname failed");
    close(routersocket);
    exit(1);
  }
  router_port_num = ntohs(routeraddr.sin_port);
  return routersocket;
}

void run_router(int cur_router, char* interface, char* router_ip){
  struct sockaddr_in proxyaddr;
  int routersocket, raw_socket;
  char buffer[BUFSIZE];
  socklen_t addrlen = sizeof(struct sockaddr_in);

  // Create sockets
  raw_socket = create_raw_socket(interface, router_ip);
  routersocket = create_udp_socket(router_ip);

  // Proxy information
  memset(&proxyaddr, 0, sizeof(proxyaddr));
  proxyaddr.sin_family = AF_INET;
  proxyaddr.sin_port = htons(PROXY_PORT_NUM);
  if (inet_aton("127.0.0.1", &proxyaddr.sin_addr)==0) {
    perror("inet_aton() failed");
    close(routersocket);
    close(raw_socket);
    exit(1);
  } 

  // Initial setup of router output file
  FILE *output;
  char stage, router_num, filename[40];
  stage = STAGE + '0';
  router_num = cur_router + '0';
  sprintf(filename, "stage%c.router%c.out", stage, router_num);
  output = fopen(filename, "w");
  fprintf(output, "router: %d, pid: %d, port: %d\n", cur_router, getpid(), router_port_num);
  fclose(output);

  // Send pid to proxy (I'm alive msg)
  sprintf(buffer, "%d", getpid());
  if (sendto(routersocket, buffer, strlen(buffer), 0, (struct sockaddr *)&proxyaddr, addrlen)==-1) {
    perror("sendto failed");
    close(routersocket);
    close(raw_socket);
    exit(1);
  }

  // Setup for select loop
  fd_set readset;
  int max;
  if (routersocket > raw_socket)
     max = routersocket;
  else
     max = raw_socket;

  FD_ZERO(&readset);
  FD_SET(routersocket, &readset);
  FD_SET(raw_socket, &readset);

  // Select loop for listening and responding
  do{
    if (select(max+1, &readset, NULL, NULL, NULL) == SO_ERROR){
      perror("Select error!");
      close(routersocket);
      close(raw_socket);
      exit(1);
    }
    if FD_ISSET(routersocket, &readset){
      int len = recvfrom(routersocket, buffer,BUFSIZE, 0, (struct sockaddr *)&proxyaddr,&addrlen);
      if (len > 0){
	      buffer[len] = 0;
        struct iphdr *ip = (struct iphdr*)buffer;
      	char buffer1[1000];
       
      	memcpy(buffer1, buffer+sizeof(struct iphdr), sizeof(struct icmphdr));
        buffer1[sizeof(struct icmphdr)] = 0;
      	struct icmphdr *icmp = (struct icmphdr*) buffer1;

        output = fopen(filename, "a");
      	fprintf(output,"ICMP from port:%d, src:%u.%u.%u.%u, dst:%u.%u.%u.%u, type:%d\n",PROXY_PORT_NUM, ip->saddr &0xff, ip->saddr>>8 &0xff, ip->saddr>>16 &0xff,ip->saddr>>24 &0xff, ip->daddr &0xff, ip->daddr>>8 &0xff, ip->daddr>>16 &0xff,ip->daddr >> 24 &0xff, icmp->type);
      	fclose(output);

        __u32 addr = ip->saddr;

        if (!((inet_addr("10.5.51.2") & inet_addr("255.255.255.0")) == (ip->daddr & inet_addr("255.255.255.0")))) {
          struct iovec iov[1];
          iov[0].iov_base=buffer1;
          iov[0].iov_len=sizeof(buffer1);

          struct msghdr message;
          message.msg_name=ip->daddr;
          message.msg_namelen=sizeof(ip->daddr);
          message.msg_iov=iov;
          message.msg_iovlen=1;
          message.msg_control=0;
          message.msg_controllen=0;

          if (sendmsg(raw_socket, &message, 0) < 0) {
              perror("sendto outside world failed");
              exit(1);
          }
        }   
      	
      	ip->saddr = ip->daddr;
      	ip->daddr = addr;

      	icmp->type = ICMP_ECHOREPLY;
      	icmp->checksum = cksum((unsigned short *)icmp, sizeof(struct icmphdr));

        ip->check = cksum((unsigned short *)ip, sizeof(struct iphdr));

      	memcpy(buffer, ip, sizeof(struct iphdr));
      	memcpy(buffer+sizeof(struct iphdr), icmp, sizeof(struct icmphdr));
      	buffer[sizeof(struct iphdr) + sizeof(struct icmphdr)] = 0;
        if (sendto(routersocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&proxyaddr, addrlen)==-1) {
          	  perror("sendto proxy failed");
          	  exit(1);
      	}
      }
    }
    if FD_ISSET(raw_socket, &readset){
      struct icmphdr *icmp;
      struct iovec iov;
      struct msghdr msg;
      struct sockaddr_in src; 

      iov.iov_base = &icmp;
      iov.iov_len = sizeof(icmp);
      msg.msg_name = (void*)&src;
      msg.msg_namelen = sizeof(src);
      msg.msg_iov = &iov;
      msg.msg_iovlen = 1;
      msg.msg_flags = 0;
      msg.msg_control = 0;
      msg.msg_controllen = 0;
      /* Receiving errors flog is set */
      return_status = recvmsg(socket, &msg, 0);
      printf("ICMP msgtype=%d", icmp->type);
      /*int n=recvfrom(raw_socket,buffer,BUFSIZE,0,(struct sockaddr *)&proxyaddr,&addrlen);
      printf(" rec'd %d bytes\n",n);

      struct iphdr *ip_hdr = (struct iphdr *)buffer;

      printf("IP header is %d bytes.\n", ip_hdr->ihl*4);

      for (int i = 0; i < n; i++) {
        printf("%02X%s", (uint8_t)buffer[i], (i + 1)%16 ? " " : "\n");
      }
      printf("\n");

      struct icmphdr *icmp_hdr = (struct icmphdr *)((char *)ip_hdr + (4 * ip_hdr->ihl));

      printf("ICMP msgtype=%d, code=%d", icmp_hdr->type, icmp_hdr->code);*/
    }
  } while(1);

  close(raw_socket);
  close(routersocket);
}
