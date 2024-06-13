#include <iostream>
#include <string>
#include <memory>

#include "epollServer.hpp"

static void usage(std::string proc)
{
  std::cerr << "Usage:\n\t" << proc << " port"
            << "\n\n";
}

std::string transaction(const std::string &request)
{
  return request;
}

// epollServer 8081
int main(int argc, char **argv)
{
  if (argc < 2)
  {
    usage(argv[0]);
    exit(ERROR);
  }

  // 运行 epollServer
  std::unique_ptr<epoll_nc::EpollServer> epollServer(new epoll_nc::EpollServer(transaction, atoi(argv[1])));
  epollServer->initServer();
  epollServer->start();

  // 打开终端，telnet 192.168.61.215 8001，即可连接上面的 epollServer

  return 0;
}