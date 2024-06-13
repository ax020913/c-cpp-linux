#include "sock.hpp"
#include <functional>
#include <sys/epoll.h>
#include <cassert>

namespace epoll_nc
{
  static const int defaultport = 8001;
  static const int num = 64;
  static const int defaultfd = -1;

  using func_t = std::function<std::string(const std::string &)>;

  class EpollServer
  {
  private:
    int _listensock;
    int _port;
    func_t _func;
    // struct pollfd *_rfds; // poll 的这个参数变成下面的三个参数了
    int _epfd;
    struct epoll_event *_rfds;
    int _maxlen;

  public:
    // 构造函数
    EpollServer(func_t f, int port = defaultport)
        : _listensock(defaultfd), _port(port), _epfd(defaultfd), _rfds(nullptr), _maxlen(num), _func(f)
    {
    }

    // 析构函数
    ~EpollServer()
    {
      if (_listensock == defaultport)
        close(_listensock);
      if (_epfd == defaultfd)
        close(_epfd);
      if (_rfds != nullptr)
        delete[] _rfds;
    }

    // 初始化成员变量
    void initServer()
    {
      // 1. 创建socket
      _listensock = Sock::Socket();
      Sock::Bind(_listensock, _port);
      Sock::Listen(_listensock);
      // 3. 创建epoll模型
      _epfd = epoll_create(_maxlen);
      if (_epfd == -1)
      {
        perror("epoll_create");
        exit(-1);
      }
      // 3. 添加 _listensock 到 epoll 中
      struct epoll_event ev;
      ev.events = EPOLLIN;
      ev.data.fd = _listensock;
      epoll_ctl(_epfd, EPOLL_CTL_ADD, _listensock, &ev);
      // 4. 申请就绪事件空间
      _rfds = new struct epoll_event[_maxlen];
      assert(_rfds);
    }

    // 启动服务器
    void start()
    {
      int timeout = -1; // 0：非阻塞   >0：定时返回   <0：阻塞
      for (;;)
      {
        int readyNum = epoll_wait(_epfd, _rfds, _maxlen, timeout);
        switch (readyNum) // _rfds 中前 readyNum 个事件描述符都是就绪的哦
        {
        case 0:
          logMessage(NORMAL, "timeout...");
          break;
        case -1:
          logMessage(WARNING, "epoll error, code: %d, err string: %s", errno, strerror(errno));
          break;
        default:
          logMessage(NORMAL, "have event ready!");
          HandlerEvent(readyNum);
          break;
        }
      }
    }

  private:
    void Print(int readyNum)
    {
      std::cout << "fd list: ";
      for (int i = 0; i < readyNum; i++)
        std::cout << _rfds[i].data.fd << ' ';
      std::cout << std::endl;
    }
    void HandlerEvent(int readyNum)
    {
      // _rfds 中前 readyNum 个事件描述符都是就绪的哦
      for (int i = 0; i < readyNum; i++)
      {
        uint32_t events = _rfds[i].events;
        int sock = _rfds[i].data.fd;

        // 1. _listensock 事件描述符就绪了 ===> accept
        if (sock == _listensock && (events & EPOLLIN))
        {
          // 走到这个 accept 不会阻塞 <=== epoll 通知了我们 _listensock 有事件就绪了
          std::string clientip;
          uint16_t clientport = 0;
          int newfd = Sock::Accept(_listensock, &clientip, &clientport);
          if (newfd < 0)
            continue;
          logMessage(NORMAL, "accept success [%s:%d]", clientip.c_str(), clientport);

          // 获取fd成功，可以直接读取吗？？不可以，放入epoll
          struct epoll_event ev;
          ev.events = EPOLLIN;
          ev.data.fd = newfd;
          epoll_ctl(_epfd, EPOLL_CTL_ADD, newfd, &ev);

          // 打印看一看 _rfds 数组的内容
          Print(readyNum);
        }

        // 2. io事件描述符就绪了 ===> recvform,sendto,read,write,recv,send
        else if (events & EPOLLIN)
        {
          logMessage(DEBUG, "io事件描述符就绪");

          // 1. 读取request
          // bug
          char buffer[1024];
          ssize_t size = read(sock, buffer, sizeof(buffer) - 1);
          if (size > 0) // 数据正常接收
          {
            buffer[size] = 0;
            logMessage(NORMAL, "client# %s", buffer);
            //
            // 2. 处理request
            std::string response = _func(buffer);

            // 3. 返回response
            // bug
            write(sock, response.c_str(), sizeof(response));
          }
          else if (size == 0) // 数据接收完毕
          {
            // 建议先从epoll移除，才close fd
            epoll_ctl(_epfd, EPOLL_CTL_DEL, sock, nullptr);
            close(sock);
            logMessage(NORMAL, "client quit");
            return;
          }
          else if (size < 0)
          {
            epoll_ctl(_epfd, EPOLL_CTL_DEL, sock, nullptr);
            close(sock);
            logMessage(ERROR, "client quit: %s", strerror(errno));
            return;
          }
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