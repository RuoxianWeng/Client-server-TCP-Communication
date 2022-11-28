#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include "msg.h"

/*
 *  * Project 3
 *  * By Ruoxian Weng. Completed on May 7, 2022.
 *    * This program implement the server side of client-server TCP communication
 *      * listening for incoming client request
 *      * accept a client and create a new thread to handle the request
 *      * never terminate to listen
 *  * Command line arguments: argv[1] = port
*/

void usage(char* s);
int listening(char* port);
void* handle(void* arg);
void put(int f_fd, struct msg *message);
void get(int f_fd, struct msg *message);
void delete(int f_fd, struct msg *message);

int recordIndex = 0; //keep track of the end of file

int main(int argc, char *argv[]) {
  char* port;
  int listen_fd;

  //check argument
  if (argc != 2) {
    usage(argv[0]);
  }

  port = argv[1];
  //get a listening socket
  listen_fd = listening(port);
  if (listen_fd == -1) {
    perror("couldn't bind to any addresses\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    struct sockaddr_storage client_addr;
    socklen_t client_addrlen = sizeof(client_addr);

    //accept a client connection
    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_addrlen);
    if (client_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK)) {
        continue;
      }
      perror("fail to accept a client connection");
      close(listen_fd);
      exit(EXIT_FAILURE);
    }

    //handle the client's request
    pthread_t handler;
    pthread_create(&handler, NULL, handle, &client_fd);
  }
}

void usage(char* s) {
  printf("usage: %s port\n", s);
  exit(EXIT_FAILURE);
}

int listening(char* port) {
  int retval, socket_fd = -1;
  struct addrinfo hints, *res;

  //zero out hints
  memset(&hints, 0, sizeof(struct addrinfo));
  //set constrints on address structure
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_flags |= AI_V4MAPPED;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  //call getaddrinfo()
  if ((retval = getaddrinfo(NULL, port, &hints, &res)) != 0) {
    printf("getaddrinfo failed: %s", gai_strerror(retval));
    return -1;
  }

  //search addresses until we are able to create and bind a socket
  struct addrinfo *i;
  for (i = res; i != NULL; i = i->ai_next) {
    //create socket
    socket_fd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
    if (socket_fd == -1) { //if socket creation failed
      perror("fail to create socket");
      //go to the next address
      continue;
    }

    //setting a socket option
    //make the port we bind available as soon as we exit
    int optionValue = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optionValue, sizeof(optionValue));

    //bind the socket to the address
    if (bind(socket_fd, i->ai_addr, i->ai_addrlen) == 0) { //bind success
      break;
    }
    //bind failed
    //close socket fd and go to the next address
    close(socket_fd);
    socket_fd = -1;
  }

  //free structure returned by getaddrinfo()
  freeaddrinfo(res);

  //if bind failed for all addresses
  if (socket_fd == -1) {
    return -1;
  }

  //tell OS that this socket is a listenig socket
  if (listen(socket_fd, SOMAXCONN) != 0) {
    perror("fail to listen a socket");
    return -1;
  }
  return socket_fd;
}

void* handle(void* arg) {
  int c_fd = *(int*)arg;

  //open database file
  int f_fd = open("database.txt", O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
  if (f_fd == -1) {
    perror("fail to open file");
    exit(EXIT_FAILURE);
  }

  //read data until client closes the connection
  while (1) {
    struct msg *message = (struct msg*) malloc(sizeof(struct msg));
    //recieve message from client
    ssize_t msgSizeRead = read(c_fd, message, sizeof(struct msg));
    if (msgSizeRead == -1) { //if read failed
      if ((errno == EAGAIN) || (errno == EINTR)) {
        continue;
      }
        perror("fail to read message from client");
        break;
    }
    if (msgSizeRead == 0) {
      perror("[The client disconnected.]\n");
      break;
    }

    //handle request
    switch (message->type) {
      case 1: //put 
        put(f_fd, message);
        break;
      case 2: //get
        get(f_fd, message);
        break;
      case 3: //delete
        delete(f_fd, message);
        break;
    }
    //send message back to client
    write(c_fd, message, sizeof(struct msg));
  }
  close(c_fd);
  close(f_fd);
  return NULL;
}

//write message to database
void put(int f_fd, struct msg *message) {
  int location, status;
  //search for location of first unused record
  for (location = 0, status = 0; location < recordIndex*512 && status == 0; location+=512) {
    int32_t id;
    lseek(f_fd, location+128, SEEK_SET);
    read(f_fd, &id, sizeof(int32_t));
    if (id == -1) { //if id = -1, record in that location is unused
      status = 1;
    }
  }

  //write to database
  struct record r = message->rd;
  lseek(f_fd, location, SEEK_SET);
  int byteWrite = write(f_fd, &r, 512);
  if (byteWrite > 0) {
    if (status == 0) { //increment index if writing to end of file
      recordIndex++;
    }
    message->type = 4;
  }
  else {
    message->type = 5;
  }
}

//search for corresponding id in the database
void get(int f_fd, struct msg *message) {
  int location, status;
  struct record r;
  lseek(f_fd, 0, SEEK_SET);
  //search for matching id in the database sequentially
  for (location = 0, status = 0; location < recordIndex*512 && status == 0; location+=512) {
    read(f_fd, &r, sizeof(struct record));
    if (r.id == message->rd.id) { //if id matches
      status = 1;
    }
  }
  if (status == 1) {
    message->type = 4;
    message->rd = r;
  }
  else {
    message->type = 5;
  }
}

//find corresponding id and delete the record
void delete(int f_fd, struct msg *message) {
  int location, status;
  struct record r;
  lseek(f_fd, 0, SEEK_SET);
  for (location = 0, status = 0; location < recordIndex*512 && status == 0; location+=512) {
    read(f_fd, &r, sizeof(struct record));
    if (r.id == message->rd.id) { //if id matches
      status = 1;
    }
  }
  if (status == 1) {
    //set id of that record to -1 in the database
    lseek(f_fd, location-384, SEEK_SET);
    int32_t id = -1;
    write(f_fd, &id, sizeof(int32_t));
    message->type = 4;
    message->rd = r;
  }
  else {
    message->type = 5;
  }
}
