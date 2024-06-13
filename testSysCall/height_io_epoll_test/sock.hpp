#pragma once

#include <iostream>
#include <string>
#include <cstdarg>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>

#include "err.hpp"
#include "log.hpp"

class Sock
{
private:
  const static int backlog = 32;

public:
  // 1. 创建 socket 网络套接字对象
  static int Socket()
  {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
      perror("socket");
      exit(-1);
    }
    logMessage(NORMAL, "socket create sock success: %d", sock);

    // setsockopt设置套接字地址复用，tcp四次挥手，主动断开连接的一方需要维持一段时间的 TIME_WAIT 状态，再次绑定的时候会短暂的失败
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    return sock;
  }

  // 2. bind绑定自己的网络信息
  static void Bind(int sock, int port)
  {
    struct sockaddr_in local;
    bzero(&local, sizeof(local)); // 使用 memset(&local, 0, sizeof(local)); 也是可以的哦
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
      perror("bind");
      exit(-1);
    }
    logMessage(NORMAL, "bind sock success");
  }

  // 3. 设置套接字为监听状态
  static void Listen(int sock)
  {
    if (listen(sock, backlog) < 0)
    {
      perror("listen");
      exit(-1);
    }
    logMessage(NORMAL, "listen sock success");
  }

  // 4. accept从全连接队列获取sock网络套接字
  static int Accept(int listensock, std::string *clientip, uint16_t *clientport)
  {
    struct sockaddr_in peer;
    socklen_t len = sizeof(peer);
    int accept_sock = accept(listensock, (struct sockaddr *)&peer, &len);
    if (accept_sock < 0)
    {
      logMessage(ERROR, "accept sock error");
      perror("accept");
      exit(-1);
    }
    else
    {
      logMessage(NORMAL, "accept a new link success, get new sock: %d", accept_sock);
      // 外部需要使用到的变量
      *clientip = inet_ntoa(peer.sin_addr);
      *clientport = ntohs(peer.sin_port);
    }

    return accept_sock;
  }
};