#include <linux/ip.h>
#include <linux/icmp.h>
#include "router.h"

unsigned short in_cksum(unsigned short *ptr, int nbytes){
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

int create_raw_socket(char *ip, struct sockaddr_in routeraddr, socklen_t addrlen){
  int raw_socket;
  if ((raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
    perror("cannot create raw socket");
    exit(1);
  }
  memset(&routeraddr, 0, sizeof(routeraddr));
  routeraddr.sin_family = AF_INET;
  routeraddr.sin_port = htons(0);
  routeraddr.sin_addr.s_addr = inet_aton(ip, &routeraddr.sin_addr);
  if (bind(raw_socket, (struct sockaddr *)&routeraddr, sizeof(routeraddr)) < 0) {
    perror("bind failed");
    exit(1);
  } 
  if (getsockname(raw_socket, (struct sockaddr *)&routeraddr, &addrlen) < 0 ){
    perror("getsockname failed");
    exit(1);
  }
  return raw_socket;
}

void create_udp_socket(){

}

void run_router(int cur_router, char* interface, char *ip){
  struct sockaddr_in routeraddr, proxyaddr;
  int routersocket, raw_socket;
  char buffer[BUFSIZE];
  socklen_t addrlen = sizeof(struct sockaddr_in);

  //raw_socket = create_raw_socket(ip, routeraddr, addrlen);

  if ((routersocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("cannot create udp socket");
    exit(1);
  }

  memset(&routeraddr, 0, sizeof(routeraddr));
  routeraddr.sin_family = AF_INET;
  routeraddr.sin_port = htons(0);
  routeraddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(routersocket, (struct sockaddr *)&routeraddr, sizeof(routeraddr)) < 0) {
    perror("bind failed");
    exit(1);
  } 
  if (getsockname(routersocket, (struct sockaddr *)&routeraddr, &addrlen) < 0 ){
    perror("getsockname failed");
    exit(1);
  }
  int router_port_num = ntohs(routeraddr.sin_port);

  memset(&proxyaddr, 0, sizeof(proxyaddr));
  proxyaddr.sin_family = AF_INET;
  proxyaddr.sin_port = htons(PROXY_PORT_NUM);
  if (inet_aton("127.0.0.1", &proxyaddr.sin_addr)==0) {
    perror("inet_aton() failed");
    exit(1);
  } 

  FILE *output;
  char stage, router_num, filename[40];
  stage = STAGE + '0';
  router_num = cur_router + '0';
  sprintf(filename, "stage%c.router%c.out", stage, router_num);
  output = fopen(filename, "w");
  fprintf(output, "router: %d, pid: %d, port: %d\n", cur_router, getpid(), router_port_num);
  fclose(output);

  sprintf(buffer, "%d", getpid());
  if (sendto(routersocket, buffer, strlen(buffer), 0, (struct sockaddr *)&proxyaddr, addrlen)==-1) {
    perror("sendto failed");
    exit(1);
  }

  fd_set readset;
  int max = routersocket;

  FD_ZERO(&readset);
  FD_SET(routersocket, &readset);

  do{
    if (select(max+1, &readset, NULL, NULL, NULL) == SO_ERROR){
      perror("Select error!");
      exit(1);
    }
    if FD_ISSET(routersocket, &readset){
      int len = recvfrom(routersocket, buffer,BUFSIZE, 0, (struct sockaddr *)&proxyaddr,&addrlen);
      if (len > 0){
	buffer[len] = 0;
        struct iphdr *ip = (struct iphdr*)buffer;
	char buffer1[1000];
       
	memcpy(buffer1, buffer+sizeof(struct iphdr), sizeof(struct icmphdr));
	struct icmphdr *icmp = (struct icmphdr*) buffer1;

        output = fopen(filename, "a");
	fprintf(output,"ICMP from port:%d, src:%u.%u.%u.%u, dst:%u.%u.%u.%u, type:%d\n",PROXY_PORT_NUM, ip->saddr &0xff, ip->saddr>>8 &0xff, ip->saddr>>16 &0xff,ip->saddr>>24 &0xff, ip->daddr &0xff, ip->daddr>>8 &0xff, ip->daddr>>16 &0xff,ip->daddr >> 24 &0xff, icmp->type);
	fclose(output);
        
	__u32 addr = ip->daddr;
	ip->daddr = ip->saddr;
	ip->saddr = addr;

	//ip->ckeck = in_cksum((unsigned short *)ip, sizeof(struct iphdr));

	icmp->type = ICMP_ECHOREPLY;
	icmp->checksum = in_cksum((unsigned short *)icmp, sizeof(struct icmphdr));

	memcpy(buffer, ip, sizeof(struct iphdr));
	memcpy(buffer+sizeof(struct iphdr), icmp, sizeof(struct icmphdr));
	buffer[sizeof(struct iphdr) + sizeof(struct icmphdr)] = 0;
  	if (sendto(routersocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&proxyaddr, addrlen)==-1) {
    	  perror("sendto failed");
    	  exit(1);
	}
      }
    }
  } while(1);

  close(routersocket);
}
