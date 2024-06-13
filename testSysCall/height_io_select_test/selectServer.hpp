#pragma once

#include "sock.hpp"
#include <functional>

namespace select_nc
{
  static const int defaultport = 8001;
  static const int fdnum = sizeof(fd_set) * 8;
  static const int defaultfd = -1;

  using func_t = std::function<std::string(const std::string &)>;

  class SelectServer
  {
  private:
    // 声明的顺序才是初始化的顺序
    int _listensock;
    int _port;
    int *_fdarray; // 使用 select 的话，需要自己维护一个保存所有合法 fd 的数组
    func_t _func;

  public:
    // 构造函数
    SelectServer(func_t f, int port = defaultport) // 一般默认参数都是从右边放起的
        : _listensock(-1), _port(port), _fdarray(nullptr), _func(f)
    {
    }

    // 析构函数
    ~SelectServer()
    {
      if (_listensock == defaultport)
        close(_listensock);
      if (_fdarray != nullptr)
        delete[] _fdarray;
    }

    // 初始化成员变量
    void initServer()
    {
      _listensock = Sock::Socket();
      Sock::Bind(_listensock, _port);
      Sock::Listen(_listensock);
      _fdarray = new int[fdnum];
      for (int i = 0; i < fdnum; i++)
        _fdarray[i] = defaultport;
      _fdarray[0] = _listensock; // _listensock 一致占据 _fdarray 的第一个位置
    }

    // 启动服务器
    void start()
    {
      for (;;)
      {
        // select 需要使用到的内核级位图
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = _fdarray[0];

        // 遍历 int* _fdarray 数组
        for (int i = 0; i < fdnum; i++)
        {
          if (_fdarray[i] == defaultport)
            continue;

          // 合法的 fd 全部添加到读文件描述符集中
          FD_SET(_fdarray[i], &rfds);

          // 更新所有 fd 中最大的 fd
          if (maxfd < _fdarray[i])
            maxfd = _fdarray[i];
        }

        // struct timeval timeout = {0, 0};
        // int n = select(_listensock + 1, &rfds, nullptr, nullptr, &timeout);
        // _fdarray: 一般而言，要是用 select 的话，需要自己维护一个保存所有合法 fd 的数组 ！
        int n = select(maxfd + 1, &rfds, nullptr, nullptr, nullptr);
        switch (n)
        {
        case 0:
          logMessage(NORMAL, "timeout...");
          break;
        case -1:
          logMessage(WARNING, "select error, code: %d, err string: %s", errno, strerror(errno));
          break;
        default:
          // 说明有事件就绪了，目前只有一个监听事件_listensock
          logMessage(NORMAL, "get a new link...");
          HandlerEvent(rfds);
          break;
        }
      }
    }

  private:
    // 打印 int* _fdarray;
    void Print()
    {
      std::cout << "fd list: ";
      for (int i = 0; i < fdnum; i++)
        if (_fdarray[i] != defaultport)
          std::cout << _fdarray[i] << ' ';
      std::cout << std::endl;
    }
    // _listensock 监听描述符的获取 new link 逻辑
    void HandlerEvent(fd_set &rfds)
    {
      for (int i = 0; i < fdnum; i++)
      {
        if (_fdarray[i] == defaultport)
          continue;

        // 1. _listensock 事件描述符就绪了 ===> accept
        if (_fdarray[i] == _listensock && FD_ISSET(_fdarray[i], &rfds))
        {
          // 走到这个 accept 不会阻塞 <=== select 通知了我们 _listensock 有事件就绪了
          std::string clientip;
          uint16_t clientport = 0;
          int sock = Sock::Accept(_listensock, &clientip, &clientport);
          if (sock < 0)
            return;
          logMessage(NORMAL, "accept success [%s:%d]", clientip.c_str(), clientport);

          // sock 我们能直接 recv / read 吗？
          // 不整个代码只有 select 有资格检测事件是否就绪
          // ===> 将 new link sock 托管给 select ===> 将 sock 添加到 _fdarray 数组中即可
          int i = 0;
          for (; i < fdnum; i++)
          {
            if (_fdarray[i] != defaultport)
              continue;
            else
              break;
          }
          if (i < fdnum)
            _fdarray[i] = sock;
          else
          {
            logMessage(WARNING, "server if full, please wait");
            close(sock); // 服务器断开了 new link ===> 我们看到的连接已重置，需要重新连接
          }

          // 打印看一看 _fdarray 数组的内容
          Print();
        }

        // 2. io事件描述符就绪了 ===> recvform,sendto,read,write,recv,send
        else if (FD_ISSET(_fdarray[i], &rfds))
        {
          logMessage(DEBUG, "io事件描述符就绪");

          // 1. 读取request
          // bug
          char buffer[1024];
          ssize_t size = read(_fdarray[i], buffer, sizeof(buffer) - 1);
          if (size > 0) // 数据正常接收
          {
            buffer[size] = 0;
            logMessage(NORMAL, "client# %s", buffer);
          }
          else if (size == 0) // 数据接收完毕
          {
            close(_fdarray[i]);
            _fdarray[i] = defaultfd;
            logMessage(NORMAL, "client quit");
            return;
          }
          else if (size < 0)
          {
            close(_fdarray[i]);
            _fdarray[i] = defaultfd;
            logMessage(ERROR, "client quit: %s", strerror(errno));
            return;
          }

          // 2. 处理request
          std::string response = _func(buffer);

          // 3. 返回response
          // bug
          write(_fdarray[i], response.c_str(), sizeof(response));
        }

        // 3. 未知的事件描述符就绪了
        else
        {
          logMessage(NORMAL, "未知的事件描述符就绪了");
        }
      }
    }
  };
}