#include <iostream>
#include <string>
#include <memory>

#include "selectServer.hpp"

static void usage(std::string proc)
{
  std::cerr << "Usage:\n\t" << proc << " port"
            << "\n\n";
}

std::string transaction(const std::string &request)
{
  return request;
}

// selectServer 8081
int main(int argc, char **argv)
{
  if (argc < 2)
  {
    usage(argv[0]);
    exit(ERROR);
  }

  // 运行 selectServer
  std::unique_ptr<select_nc::SelectServer> selectServer(new select_nc::SelectServer(transaction, atoi(argv[1])));
  selectServer->initServer();
  selectServer->start();

  // 打开终端，telnet 192.168.61.215 8001，即可连接上面的 selectServer

  return 0;
}