#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "msg.h"

/*
 *  * Project 3
 *  * By Ruoxian Weng. Completed on May 7, 2022.
 *    * This program implements the client side of client-server TCP communication
 *      * create a connection to a server
 *      * get request from the user
 *      * send the request to the server
 *      * request are handled sequentially for a single client
 *    * Command line arguments: argv[1] = hostname, argv[2] = port
*/

void usage(char *s);
int lookUpName(char *hostname, unsigned short port, struct sockaddr_storage *ret_addr, size_t *ret_addrlen);
int connection(struct sockaddr_storage *addr, size_t addrlen, int* socket_fd);
void put(struct msg *message);
void get(struct msg *message);
void delete(struct msg *message);
void communicate(int32_t socket_fd, struct msg *message);

int main(int argc, char *argv[]) {
  int32_t choice, socket_fd;
  unsigned short port;
  struct sockaddr_storage addr;
  size_t addrlen;

  //input checking
  if (argc != 3) {
    usage(argv[0]);
  }

  //convert string to unsigned short
  if (sscanf(argv[2], "%hu", &port) != 1) {
    usage(argv[0]);
  }

  //find IP address and its structure of given hostname
  if (!lookUpName(argv[1], port, &addr, &addrlen)) {
    usage(argv[0]);
  }

  //set up a connection with the server
  if (!connection(&addr, addrlen, &socket_fd)) {
    usage(argv[0]);
  }

  struct msg *message = (struct msg*) malloc(sizeof(struct msg));
  while (1) {
    //prompt user
    printf("Enter your choice (1 to put, 2 to get, 3 to delete, 0 to quit): ");
    scanf("%d", &choice);
    if (fgetc(stdin) != 10) { //if user enter something other than a number
      printf("invalid choice\n");
      exit(EXIT_FAILURE);
    }
    switch (choice) {
      case 1: //put
        //get user input
        put(message);
        //communicate with server
        communicate(socket_fd, message);
        if (message->type == 4) {
          printf("Put success.\n");
        }
        else if (message->type == 5) {
          printf("Put failed.\n");
        }
        break;
      case 2: //get
        //get user input
        get(message);
        //communicate with server
        communicate(socket_fd, message);
        if (message->type == 4) {
          printf("name: %s\n", message->rd.name);
          printf("id: %d\n", message->rd.id);
        }
        else if (message->type == 5) {
          printf("Get failed.\n");
        }
        break;
      case 3: //delete
        //get user input
        delete(message);
        //communicate with server
        communicate(socket_fd, message);
        if (message->type == 4) {
          printf("name: %s\n", message->rd.name);
          printf("id: %d\n", message->rd.id);
        }
        else if (message->type == 5) {
          printf("Delete failed.\n");
        }
        break;
      case 0: //quit
        close(socket_fd);
        free(message);
        exit(EXIT_SUCCESS);
      default: //any not valid numbers
        printf("invalid choice. Try again.\n");
        continue;
    }
  }
}

void usage(char *s) {
  fprintf(stderr, "Usage: %s hostname port\n", s);
  exit(EXIT_FAILURE);
}

//find specific IP address for a given hostname
int lookUpName(char *hostname, unsigned short port, struct sockaddr_storage *ret_addr, size_t *ret_addrlen) {
  int retval;
  struct addrinfo hints, *res;

  memset(&hints, 0, sizeof(hints)); //zero out hints for default
  //set specfic fields for hints
  hints.ai_family = AF_UNSPEC; //can be either IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM;

  //call getaddrinfo()
  if ((retval = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
    printf("getaddrinfo failed: %s", gai_strerror(retval));
    return 0; //fail to look up name
  }

  //set port of the address in the first result
  if (res->ai_family == AF_INET) { //IPv4
    struct sockaddr_in *v4addr = (struct sockaddr_in*) (res->ai_addr);
    v4addr->sin_port = htons(port);
  }
  else if (res->ai_family == AF_INET6) { //IPv6
    struct sockaddr_in6 *v6addr = (struct sockaddr_in6*) (res->ai_addr);
    v6addr->sin6_port = htons(port);
  }
  else { //not IPv4 or IPv6 address
    printf("getaddrinfo failed to provide an IPv4 or IPv6 address");
    return 0;
  }

  //return the first result
  if (res == NULL) {
    printf("structure of given hostname is null");
    return 0;
  }
  //get ip address & address length
  memcpy(ret_addr, res->ai_addr, res->ai_addrlen);
  *ret_addrlen = res->ai_addrlen;

  //clean up
  freeaddrinfo(res);
  return 1;
}
//create socket and connect to remote host
int connection(struct sockaddr_storage *addr, size_t addrlen, int* socket_fd) {
  //create a socket
  int sock_fd = socket(addr->ss_family, SOCK_STREAM, 0);
  if (sock_fd == -1) {
    printf("fail to create a socket");
    return 0;
  }

  //connect to the remote host
  int connectRes = connect(sock_fd, (const struct sockaddr*)addr, addrlen);
  if (connectRes == -1) {
    printf("fail to connect to remote host");
    return 0;
  }

  //get socket fd
  *socket_fd = sock_fd;
  return 1;
}

//prompt and get name and id from user to store in database
void put(struct msg *message) {
  message->type = 1;
  struct record r;
  //get name 
  printf("Enter the name: ");
  fgets(r.name, 128, stdin);
  //remove newline, if any
  if (r.name[strlen(r.name) - 1] == '\n') {
    r.name[strlen(r.name) - 1] = '\0';
  }
  //get id
  printf("Enter the id: ");
  scanf("%d", &(r.id));
  //check for invalid type of id
  if (fgetc(stdin) != 10) {
    printf("invalid id\n");
    exit(EXIT_FAILURE);
  }
  message->rd = r;
}

//prompt and get id from user to search in database
void get(struct msg *message) {
  message->type = 2;
  struct record r;
  //get id
  printf("Enter the id: ");
  scanf("%d", &r.id);
  //check for invalid type of id
  if (fgetc(stdin) != 10) {
    printf("invalid id\n");
    exit(EXIT_FAILURE);
  }
  message->rd = r;
}

//prompt and get id from user to delete in database
void delete(struct msg *message) {
  message->type = 3;
  struct record r;
  //get id
  printf("Enter the id: ");
  scanf("%d", &r.id);
  //check for invalid type of id
  if (fgetc(stdin) != 10) {
    printf("invalid id\n");
    exit(EXIT_FAILURE);
  }
  message->rd = r;
}

//send and receive message to/from server
void communicate(int32_t s_fd, struct msg *message) {
  //send message
  if (write(s_fd, message, sizeof(struct msg)) == -1) {
    perror("fail to send message to server");
  }
  //receive message
  read(s_fd, message, sizeof(struct msg));
}
