#include <iostream>
#include <string>
#include <memory>

#include "pollServer.hpp"

static void usage(std::string proc)
{
  std::cerr << "Usage:\n\t" << proc << " port"
            << "\n\n";
}

std::string transaction(const std::string &request)
{
  return request;
}

// pollServer 8081
int main(int argc, char **argv)
{
  if (argc < 2)
  {
    usage(argv[0]);
    exit(ERROR);
  }

  // 运行 pollServer
  std::unique_ptr<poll_nc::PollServer> pollServer(new poll_nc::PollServer(transaction, atoi(argv[1])));
  pollServer->initServer();
  pollServer->start();

  // 打开终端，telnet 192.168.61.215 8001，即可连接上面的 pollServer

  return 0;
}