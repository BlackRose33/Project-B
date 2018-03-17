#include "router.h"
#include "tunnel.h"

void read_config_file(char* filename){
  char *line = NULL;
  size_t len = 0;
  ssize_t read;
  char* token;

  FILE *config_file;
  config_file = fopen(filename, "r");
 
  while ((read = getline(&line, &len, config_file)) != -1) {
    char *p = line;

    while (isspace(*p))    /* advance to first non-whitespace */
      p++;

    if (*p == '#' || !*p)     /* skip lines beginning with '#' or blank lines */
        continue;
    else {
      token = strtok(line, " ");
      if (!strcmp(token, "stage"))
        STAGE = atoi(strtok(NULL, " "));
      if (!strcmp(token, "num_routers"))
        NUM_ROUTERS = atoi(strtok(NULL, " "));
      if (!strcmp(token, "minitor_hops")):
	MINITOR_HOPS = atoi(strtok(NULL, " "));
    }
  }

  free(line);
  fclose(config_file);
}

void create_and_listen(){
  struct sockaddr_in proxyaddr, routeraddr;
  int proxysocket, recvlen;
  unsigned char buffer[BUFSIZE];
  socklen_t addrlen = sizeof(struct sockaddr_in);


  if ((proxysocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("cannot create socket\n");
    return;
  }

  memset(&proxyaddr, 0, sizeof(proxyaddr));
  proxyaddr.sin_family = AF_INET;
  proxyaddr.sin_port = htons(0);
  proxyaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(proxysocket, (struct sockaddr *)&proxyaddr, sizeof(proxyaddr)) < 0) {
    perror("bind failed");
    return;
  }

  if (getsockname(proxysocket, (struct sockaddr *)&proxyaddr, &addrlen) < 0){
    perror("getsockname failed");
    return;
  }
  PROXY_PORT_NUM = ntohs(proxyaddr.sin_port);

  FILE *output;
  char stage, filename[40];
  stage = STAGE + '0';
  sprintf(filename, "stage%c.proxy.out", stage);
  output = fopen(filename, "w");
  fprintf(output, "proxy port: %d\n", PROXY_PORT_NUM);
  fclose(output);

  pid_t pid;
  pid = fork();
  if (pid == 0)
    run_router();
  recvlen = recvfrom(proxysocket, buffer, BUFSIZE, 0, (struct sockaddr *)&routeraddr, &addrlen);
  if (recvlen > 0) {
    buffer[recvlen] = 0;
    output = fopen(filename, "a");
    fprintf(output, "router: 1, pid: %s, port: %d\n", buffer, ntohs(routeraddr.sin_port));
    fclose(output);
  }

  if (STAGE == 2){
    printf("On to stage 2\n");
    tunnel_reader(filename, proxysocket, routeraddr, addrlen);
  }

  if (STAGE == 3){
    printf("On to stage 3\n");
  }

  if (STAGE == 4){
    printf("On to stage 4\n");
  }

  if (STAGE == 5){
    printf("On to stage 5\n");
  }

  if (STAGE == 6){
    printf("On to stage 6\n");
  }

  close(proxysocket);
}

int main(int argc, char** argv){
  if (argc != 2) {
    perror("Error! Please enter the config file name as an argument.\n");
    return 0;
  } else {
    read_config_file(argv[1]);
    create_and_listen();
  }
  return 1;
}
