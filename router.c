#include <linux/ip.h>
#include <linux/icmp.h>
#include "router.h"
#include <errno.h>

int router_port_num;
int raw_socket_port_num;

struct record{
  uint16_t prev_hop;
  uint16_t iCircuit_ID;
  uint16_t oCircuit_ID;
  uint16_t next_hop;
};

int set = 0;

// Computing the internet checksum (RFC 1071).
// Note that the internet checksum does not preclude collisions.
uint16_t checksum (uint16_t *addr, int len)
{
  int count = len;
  register uint32_t sum = 0;
  uint16_t answer = 0;

  // Sum up 2-byte values until none or only one byte left.
  while (count > 1) {
    sum += *(addr++);
    count -= 2;
  }

  // Add left-over byte, if any.
  if (count > 0) {
    sum += *(uint8_t *) addr;
  }

  // Fold 32-bit sum into 16 bits; we lose information by doing this,
  // increasing the chances of a collision.
  // sum = (lower 16 bits) + (upper 16 bits shifted right 16 bits)
  while (sum >> 16) {
    sum = (sum & 0xffff) + (sum >> 16);
  }

  // Checksum is one's compliment of sum.
  answer = ~sum;

  return (answer);
}

void display(void *buf, int bytes)
{ int i;
  struct iphdr *ip = buf;
  struct icmphdr *icmp = buf+ip->ihl*4;

  fprintf(stderr, "----------------\n");
  for ( i = 0; i < bytes; i++ )
  {
    if ( !(i & 15) ) fprintf(stderr, "\n%X:  ", i);
    fprintf(stderr, "%X ", ((unsigned char*)buf)[i]);
  }
  fprintf(stderr,"\n");
  struct in_addr s, d;
  s.s_addr = ip->saddr;
  d.s_addr = ip->daddr;
  fprintf(stderr,"IPv%d: hdr-size=%d pkt-size=%d protocol=%d TTL=%d src=%s cksm: %d",
    ip->version, ip->ihl*4, ntohs(ip->tot_len), ip->protocol,
    ip->ttl, inet_ntoa(s), ntohs(ip->check));
  fprintf(stderr," dst=%s\n", inet_ntoa(d));
    fprintf(stderr,"ICMP: type[%d/%d] checksum[%d] id[%d] seq[%d]\n",
      icmp->type, icmp->code, ntohs(icmp->checksum),
      icmp->un.echo.id, icmp->un.echo.sequence);
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
  raw_socket_port_num = ntohs(routeraddr.sin_port);
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
  struct sockaddr_in proxyaddr, theiraddr, nexthopaddr;
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
  if (STAGE <= 4)
    fprintf(output, "router: %d, pid: %d, port: %d\n", cur_router, getpid(), router_port_num);
  if (STAGE > 4)
    fprintf(output, "router: %d, pid: %d, port: %d IP:%s\n", cur_router, getpid(), router_port_num, router_ip);
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
  fd_set readset, tempset;
  int max;
  if (routersocket > raw_socket)
     max = routersocket;
  else
     max = raw_socket;

  FD_ZERO(&readset);
  FD_SET(raw_socket, &readset);
  FD_SET(routersocket, &readset);

  // Select loop for listening and responding for stage 1 to 4
  if (STAGE <= 4){

    do{
      memcpy(&tempset, &readset, sizeof(tempset));
      if (select(max+1, &tempset, NULL, NULL, NULL) == SO_ERROR){
        perror("Select error!");
        close(routersocket);
        close(raw_socket);
        exit(1);
      }
      if FD_ISSET(routersocket, &tempset){
  	    bzero(buffer, BUFSIZE);
        int len = recvfrom(routersocket, buffer,BUFSIZE, 0, (struct sockaddr *)&theiraddr,&addrlen);
        if (len > 0){
        	buffer[len] = 0;

          // display(buffer, len);

          struct iphdr *ip = (struct iphdr*)buffer;
          
          int size = ntohs(ip->tot_len) - sizeof(struct iphdr);	
          char buffer1[size];
        	memcpy(buffer1, buffer+sizeof(struct iphdr), size);
        	struct icmphdr *icmp = (struct icmphdr*) buffer1;

          output = fopen(filename, "a");
        	fprintf(output,"ICMP from port:%d, src:%u.%u.%u.%u, dst:%u.%u.%u.%u, type:%d\n",PROXY_PORT_NUM, ip->saddr &0xff, ip->saddr>>8 &0xff, ip->saddr>>16 &0xff,ip->saddr>>24 &0xff, ip->daddr &0xff, ip->daddr>>8 &0xff, ip->daddr>>16 &0xff,ip->daddr >> 24 &0xff, icmp->type);
        	fclose(output);

          
        	if (!((inet_addr("10.5.51.2") & inet_addr("255.255.255.0")) == (ip->daddr & inet_addr("255.255.255.0")))) {
            const size_t icmp_size = sizeof(buffer1);
        	  struct iovec iov;
            iov.iov_base=icmp;
            iov.iov_len=icmp_size;
        	  struct sockaddr_in daddr;
        	  daddr.sin_family = AF_INET;
        	  daddr.sin_port = htons(raw_socket_port_num);
        	  daddr.sin_addr.s_addr = (uint32_t)ip->daddr;
        	

            struct msghdr message;
            message.msg_name=(struct sockaddr *)&daddr;
            message.msg_namelen=addrlen;
            message.msg_iov=&iov;
            message.msg_iovlen=1;
            message.msg_control=0;
            message.msg_controllen=0;

            if (sendmsg(raw_socket, &message, 0) < 0) {
              perror("sendto outside world failed");
              exit(1);
            }
            output = fopen(filename, "a");
          	fprintf(output,"ICMP from raw sock, src:%u.%u.%u.%u, dst:%s, type:0\n",ip->daddr &0xff, ip->daddr>>8 &0xff, ip->daddr>>16 &0xff,ip->daddr >> 24 &0xff, router_ip);
          	fclose(output);
          }   
        	
          __u32 addr = ip->saddr;
        	ip->saddr = ip->daddr;
        	ip->daddr = addr;

        	icmp->type = ICMP_ECHOREPLY;
        	icmp->checksum = 0x00;      
        	icmp->checksum = checksum((unsigned short *)icmp, sizeof(struct icmphdr));

        	ip->check = 0x00;
          ip->check = checksum((unsigned short *)ip, sizeof(struct iphdr));

        	memcpy(buffer, ip, sizeof(struct iphdr));
        	memcpy(buffer+sizeof(struct iphdr), buffer1, sizeof(buffer1));
        	buffer[sizeof(struct iphdr) + size] = 0;
          if (sendto(routersocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&proxyaddr, addrlen)==-1) {
        	  perror("sendto proxy failed");
        	  exit(1);
        	}
        }
      }
      if FD_ISSET(raw_socket, &tempset){

        /*struct icmphdr *icmp;
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
        if(recvmsg(raw_socket, &msg, 0)<0){
          perror("receiving failed");
  	exit(1);
        }

        output = fopen(filename, "a");
        fprintf(output,"ICMP from raw_socket, src:%s, dst:%s, type:%d\n",inet_ntoa(src.sin_addr), router_ip, icmp->type);
        fclose(output);
        */

        bzero(buffer, BUFSIZE);
        int len=recvfrom(raw_socket,buffer,BUFSIZE,0,(struct sockaddr *)&theiraddr,&addrlen);
        if (len > 0){
          buffer[len] = 0;
          struct iphdr *ip = (struct iphdr*)buffer;
          
          int size = ntohs(ip->tot_len) - sizeof(struct iphdr); 
          char buffer1[size];
          memcpy(buffer1, buffer+sizeof(struct iphdr), size);
          struct icmphdr *icmp = (struct icmphdr*) buffer1;
        
          struct in_addr src, dst;
          src.s_addr = ip->saddr;
          dst.s_addr = ip->daddr;

          output = fopen(filename, "a");
          fprintf(output,"ICMP from raw_socket, src:%s, dst:%s, type:%d\n",inet_ntoa(src), inet_ntoa(dst), icmp->type);
          fclose(output);
        }
      }
    } while(1);
  }

  // Select loop for listening and responding for stage 5 and 6
  if (STAGE > 4){
    int sequence = 1;
    struct record records;//Since for project B we would only be creating 1 circuit not multiple
    do{
      memcpy(&tempset, &readset, sizeof(tempset));
      if (select(max+1, &tempset, NULL, NULL, NULL) == SO_ERROR){
        perror("Select error!");
        close(routersocket);
        close(raw_socket);
        exit(1);
      }
      if FD_ISSET(routersocket, &tempset){
        bzero(buffer, BUFSIZE);
        int len = recvfrom(routersocket, buffer,BUFSIZE, 0, (struct sockaddr *)&theiraddr,&addrlen);
        if (len > 0){
          buffer[len] = '\0';
          uint8_t type = buffer[sizeof(struct iphdr)];

          if (type == 0x52){
            if (set == 0){
              set = 1;
              //Register received info
              records.prev_hop = theiraddr.sin_port;

              records.iCircuit_ID = buffer[sizeof(struct iphdr)+1];
              records.iCircuit_ID = records.iCircuit_ID << 8;
              records.iCircuit_ID |= buffer[sizeof(struct iphdr)+2];

              records.oCircuit_ID = (cur_router & 0xFFFF)<<8;
              records.oCircuit_ID |= 0x01;

              records.next_hop = buffer[sizeof(struct iphdr)+3];
              records.next_hop = records.next_hop << 8;
              records.next_hop |= buffer[sizeof(struct iphdr)+4];

              output = fopen(filename, "a");
              fprintf(output,"pkt from port: %d, length: 5, contents: 0x",ntohs(theiraddr.sin_port));
              for(int i = sizeof(struct iphdr); i<len; i++)
                fprintf(output,"%02x",((unsigned char*)buffer)[i]);
              fclose(output);
              fprintf(output,"new extend circuit: incoming: 0x%x, outgoing: 0x%x at %d",records.iCircuit_ID, records.oCircuit_ID, ntohs(records.next_hop));

              //Prepare extend done packet to forward
              uint8_t tosend[3];

              tosend[0] = 0x53;
              tosend[1] = buffer[sizeof(struct iphdr)+1];
              tosend[2] = buffer[sizeof(struct iphdr)+2];
           
              char datagram[sizeof(struct iphdr)+sizeof(tosend)];
              bzero(datagram, sizeof(datagram));
              struct iphdr *mant_ip = (struct iphdr*)datagram;
              mant_ip->saddr = inet_addr("127.0.0.1");
              mant_ip->daddr = inet_addr("127.0.0.1");
              mant_ip->protocol = 253;
              memcpy(datagram+sizeof(struct iphdr), (unsigned char*)tosend, sizeof(tosend));

              if (sendto(routersocket, datagram, sizeof(datagram),0,(struct sockaddr *)&theiraddr, addrlen)==-1){
                perror("sendto in router.c 0x52 1\n");
                exit(1);
              } 
            } else {
              buffer[sizeof(struct iphdr)+1] = cur_router & 0xFF;
              buffer[sizeof(struct iphdr)+2] = 0x01;

              fprintf(output,"pkt from port: %d, length: 5, contents: 0x",ntohs(theiraddr.sin_port));
              for(int i = sizeof(struct iphdr); i<len; i++)
                fprintf(output,"%02x",((unsigned char*)buffer)[i]);
              fprintf(output,"forwarding extend circuit: incoming: 0x%x, outgoing: 0x%x at %d",records.iCircuit_ID, records.oCircuit_ID, ntohs(records.next_hop));
              fclose(output);

              // Their information
              memset(&nexthopaddr, 0, sizeof(nexthopaddr));
              nexthopaddr.sin_family = AF_INET;
              nexthopaddr.sin_port = records.next_hop;
              if (inet_aton("127.0.0.1", &nexthopaddr.sin_addr)==0) {
                perror("inet_aton() failed");
                exit(1);
              } 
              if (sendto(routersocket, buffer, sizeof(buffer),0,(struct sockaddr *)&nexthopaddr, addrlen)==-1){
                perror("sendto in router.c 0x52 2\n");
                exit(1);
              } 

            }
          }
          if (type == 0x51){

          }
          if (type == 0x53){
            FILE *output = fopen(filename, "a");
            fprintf(output, "pkt from port: %d, length: 3, contents: 0x", ntohs(theiraddr.sin_port));
            for(int i = sizeof(struct iphdr); i<len; i++)
              fprintf(output,"%02x",((unsigned char*)buffer)[i]);
            fprintf(output, "\nforwarding extend-done circuit, incoming: 0x%x, outgoing: 0x%x at %d\n", records.oCircuit_ID, records.iCircuit_ID, ntohs(records.prev_hop));
            fclose(output);

            //sending extend circuit 
            buffer[sizeof(struct iphdr)+1] = (records.iCircuit_ID>>8)&0x00FF ;
            buffer[sizeof(struct iphdr)+2] = (records.iCircuit_ID)&0x00FF; 

            if (sendto(proxysocket, datagram, sizeof(datagram),0,(struct sockaddr *)&routeraddr[router_num[cur_hop-1]-1], addrlen)==-1){
              perror("sendto in tunnel.c\n");
              exit(1);
            }  
          }
          if (type == 0x54){

          }

          for ( int i = 0; i < len; i++ )
          {
            if ( !(i & 15) ) fprintf(stderr, "\n%X:  ", i);
            fprintf(stderr, "%X ", ((unsigned char*)buffer)[i]);
          }
          fprintf(stderr,"\n");
        }
      }
      if FD_ISSET(raw_socket, &tempset){
        bzero(buffer, BUFSIZE);
      }
    } while(1);
  }

  close(raw_socket);
  close(routersocket);
}
