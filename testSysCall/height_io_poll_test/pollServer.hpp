#include "sock.hpp"
#include <functional>
#include <poll.h>
#include <cassert>

namespace poll_nc
{
  static const int defaultport = 8001;
  static const int num = 2048;
  static const int defaultfd = -1;

  using func_t = std::function<std::string(const std::string &)>;

  class PollServer
  {
  private:
    int _listensock;
    int _port;
    struct pollfd *_rfds;
    func_t _func;

  public:
    // 构造函数
    PollServer(func_t f, int port = defaultport)
        : _listensock(-1), _port(port), _rfds(nullptr), _func(f)
    {
    }

    // 析构函数
    ~PollServer()
    {
      if (_listensock == defaultport)
        close(_listensock);
      if (_rfds != nullptr)
        delete[] _rfds;
    }

    // 初始化成员变量
    void initServer()
    {
      _listensock = Sock::Socket();
      Sock::Bind(_listensock, _port);
      Sock::Listen(_listensock);
      _rfds = new struct pollfd[num];
      assert(_rfds);             // 不能创建失败的哦
      _rfds[0].fd = _listensock; // _listensock 一致占据 _rfds[0] 的第一个位置
      _rfds[0].events = POLLIN;  // _listensock 只需要关系 POLLIN 事件是否就绪
      _rfds[0].revents = 0;
      for (int i = 1; i < num; i++)
      {
        _rfds[i].fd = defaultfd; // _listensock 一致占据 _rfds[0] 的第一个位置
        _rfds[i].events = 0;
        _rfds[i].revents = 0;
      }
    }

    // 启动服务器
    void start()
    {
      int timeout = -1; // 0：非阻塞   >0：定时返回   <0：阻塞
      for (;;)
      {
        int n = poll(_rfds, num, timeout);
        switch (n)
        {
        case 0:
          logMessage(NORMAL, "timeout...");
          break;
        case -1:
          logMessage(WARNING, "poll error, code: %d, err string: %s", errno, strerror(errno));
          break;
        default:
          logMessage(NORMAL, "have event ready!");
          HandlerEvent();
          break;
        }
      }
    }

  private:
    void Print()
    {
      std::cout << "fd list: ";
      for (int i = 0; i < num; i++)
        if (_rfds[i].fd != defaultfd)
          std::cout << _rfds[i].fd << ' ';
      std::cout << std::endl;
    }
    void HandlerEvent()
    {
      for (int i = 0; i < num; i++)
      {
        if (_rfds[i].fd == defaultfd)
          continue;

        // 1. _listensock 事件描述符就绪了 ===> accept
        if (_rfds[i].fd == _listensock && (_rfds[i].revents & POLLIN))
        {
          // 走到这个 accept 不会阻塞 <=== poll 通知了我们 _listensock 有事件就绪了
          std::string clientip;
          uint16_t clientport = 0;
          int sock = Sock::Accept(_listensock, &clientip, &clientport);
          if (sock < 0)
            return;
          logMessage(NORMAL, "accept success [%s:%d]", clientip.c_str(), clientport);

          // sock 我们能直接 recv / read 吗？
          // 不整个代码只有 poll 有资格检测事件是否就绪
          // ===> 将 new link sock 托管给 poll ===> 将 sock 添加到 _fdarray 数组中即可
          int i = 0;
          for (; i < num; i++)
          {
            if (_rfds[i].fd != defaultfd)
              continue;
            else
              break;
          }
          if (i < num)
          {
            _rfds[i].fd = sock;
            _rfds[i].events = POLLIN;
            _rfds[i].revents = 0;
          }
          else
          {
            logMessage(WARNING, "server if full, please wait");
            close(sock); // 服务器断开了 new link ===> 我们看到的连接已重置，需要重新连接
          }

          // 打印看一看 _rfds 数组的内容
          Print();
        }

        // 2. io事件描述符就绪了 ===> recvform,sendto,read,write,recv,send
        else if (_rfds[i].revents & POLLIN)
        {
          logMessage(DEBUG, "io事件描述符就绪");

          // 1. 读取request
          // bug
          char buffer[1024];
          ssize_t size = read(_rfds[i].fd, buffer, sizeof(buffer) - 1);
          if (size > 0) // 数据正常接收
          {
            buffer[size] = 0;
            logMessage(NORMAL, "client# %s", buffer);
          }
          else if (size == 0) // 数据接收完毕
          {
            close(_rfds[i].fd);
            _rfds[i].fd = defaultfd;
            logMessage(NORMAL, "client quit");
            return;
          }
          else if (size < 0)
          {
            close(_rfds[i].fd);
            _rfds[i].fd = defaultfd;
            logMessage(ERROR, "client quit: %s", strerror(errno));
            return;
          }

          // 2. 处理request
          std::string response = _func(buffer);

          // 3. 返回response
          // bug
          write(_rfds[i].fd, response.c_str(), sizeof(response));
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