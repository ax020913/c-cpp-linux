#pragma once

#include <string>
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "log.hpp"

class Sock
{
  const static int backlog = 32;

private:
  int _listensock;

public:
  Sock(int listensock = -1) : _listensock(listensock) {}
  ~Sock()
  {
    if (_listensock != -1)
      close(_listensock);
  }

  // 创建 _listensock
  void Create()
  {
    // 不是 int _listensock = socket(AF_INET, SOCK_STREAM, 0); 不然创建成功成员变量 _listensock 也是 -1
    _listensock = socket(AF_INET, SOCK_STREAM, 0);
    if (_listensock < 0)
    {
      logMessage(FATAL, "socket函数创建_listensock失败");
      exit(-1);
    }
    logMessage(NORMAL, "socket create sock success: %d", _listensock);

    // setsockopt设置套接字地址复用，tcp四次挥手，主动断开连接的一方需要维持一段时间的 TIME_WAIT 状态，再次绑定的时候会短暂的失败
    int opt = 1;
    setsockopt(_listensock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
  }

  // 绑定 _listensock
  void Bind(int port)
  {
    struct sockaddr_in local;
    bzero(&local, sizeof(local)); // 使用 memset(&local, 0, sizeof(local)); 也是可以的哦
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;

    if (bind(_listensock, (struct sockaddr *)&local, sizeof(local)) < 0)
    {
      logMessage(FATAL, "_listensock: %d bind port: %d error", _listensock, port);
      exit(-1);
    }
    logMessage(NORMAL, "bind sock success");
  }

  // 监听 _listensock
  void Listen()
  {
    if (listen(_listensock, backlog) < 0)
    {
      logMessage(FATAL, "listen函数监听失败");
      exit(-1);
    }
    logMessage(NORMAL, "listen sock success");
  }

  // 获取 new sock
  int Accept(std::string *clientip, uint16_t *clientport, int *err)
  {
    struct sockaddr_in peer;
    socklen_t len = sizeof(peer);
    int accept_sock = accept(_listensock, (struct sockaddr *)&peer, &len);
    *err = errno; // 如果需要使用 errno 的话，可以传递给外面去处理
    if (accept_sock > 0)
    {
      logMessage(NORMAL, "accept a new link success, get new sock: %d", accept_sock);
      // 外部需要使用到的变量
      *clientip = inet_ntoa(peer.sin_addr);
      *clientport = ntohs(peer.sin_port);
    }
    else
      logMessage(FATAL, "accept获取new _ioSock 失败");

    return accept_sock;
  }

  // 关闭 _listensock
  void Close()
  {
    if (_listensock != -1)
      close(_listensock);
  }

  // 返回 _listensock
  int Fd()
  {
    return _listensock;
  }
};