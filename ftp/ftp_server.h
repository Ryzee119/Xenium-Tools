#ifndef __FTPSERVER_H
#define __FTPSERVER_H

#include <string>
#include <map>
#include "ftp_connection.h"
#ifdef NXDK
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#else
#include <netdb.h>
#endif

class ftpServer {
  fd_set master;
  fd_set readFds;
  int fdmax;

  std::map<int, ftpConnection*> clients;

  int listener;
  int newfd;
  int i;

  struct sockaddr_storage raddr;
  socklen_t addrlen;
  struct addrinfo hints, *ai, *p;

  void* getInAddr(struct sockaddr *sa);
public:
  ftpServer(void const* conf);
  int init();
  int run();
  void forgetConnection(int fd);
  int openConnection(std::string const& addr, std::string const& port);

  const void *conf;
};

int thread_runner(void* server);
#endif
