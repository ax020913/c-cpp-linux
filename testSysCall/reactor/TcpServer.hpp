#pragma once

#include <iostream>
#include <string>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <unistd.h>
#include <sys/epoll.h>

#include "sock.hpp"
#include "epoll.hpp"
#include "util.hpp"

namespace ax
{
  static const int defaultvalue = -1;
  int MAXLEN = 64;

  // 注意：_ioSock 描述符处理函数的参数用 Connection& 好像是有些问题的
  class Connection;
  using func_t = std::function<void(Connection *)>;

  class TcpServer;
  class Connection
  {
    // private:
  public:
    // 因为 _listensock 和 _epllfd 执行的函数主要分别是： Accepter 和 Wait 就不创建 Connection
    int _ioSock;
    // _ioSock 因为可能会一次性不能读写完，需要连续的读写，所以每一个描述符都配置一个输入输出缓冲区
    std::string _inBuffer;
    std::string _outBuffer;
    // _ioSock 需要执行的：读，写，异常事件
    func_t _reader;
    func_t _sender;
    func_t _execpter;
    // 回指指针（class Connection 和 class TcpServer 在同一个文件中的原因之一 & 后面Connection中的需要使用到TcpServer中的相关成员函数或变量）
    TcpServer *_tcpServerPtr;

  public:
    Connection(TcpServer *tcpServerPtr = nullptr, int ioSock = -1, func_t reader = nullptr, func_t sender = nullptr, func_t execpter = nullptr)
        : _ioSock(ioSock), _reader(reader), _sender(sender), _execpter(execpter), _tcpServerPtr(tcpServerPtr) {}
    ~Connection()
    {
      if (_ioSock == -1)
        close(_ioSock);
      if (_tcpServerPtr != nullptr)
        _tcpServerPtr = nullptr;
    }

    // 关闭管理的 _ioSock
    void Close()
    {
      if (_ioSock != -1)
        close(_ioSock);
    }
  };

  class TcpServer
  {
  private:
    // int _listensock; // 对上面的主线程的 _listensock 封装成一个 class Sock
    Sock _sock;
    int _port;

    // 获取就绪描述符相关信息的数组，以及数组的长度
    struct epoll_event *_epoll_events;
    int _maxlen;

    // 用户传递给ioSock的读写异常事件
    func_t reader;
    func_t sender;
    func_t execpter;

  public:
    // int _epollfd; // 对于 _epollfd 监听模型，我们封装成一个 class Epoll
    Epoll _epoll;

    // 主线程中存储所有的 _ioSock 和它对应的 Connection
    std::unordered_map<int, Connection *> _connections;

    // TcpServer 提供的服务
    func_t _service;

  public:
    TcpServer(func_t read = nullptr, func_t send = nullptr, func_t execpt = nullptr, func_t service = nullptr, int port = 8081, int maxlen = 64)
        : reader(read), sender(send), execpter(execpt), _service(service), _port(port), _maxlen(maxlen), _epoll_events(nullptr) {}
    ~TcpServer()
    {
      _sock.Close();
      _epoll.Close();
      if (_epoll_events != nullptr)
        delete[] _epoll_events;
    }

    // 初始化 TcpServer 成员变量
    void InitServer()
    {
      // 创建监听描述符
      _sock.Create();
      Util::SetNonBlock(_sock.Fd());
      _sock.Bind(_port);
      _sock.Listen();
      // 创建epoll模型
      _epoll.Create();
      // 把 _listensock 放入到 epoll 模型中监听
      _epoll.OpEpoll(_sock.Fd(), EPOLL_CTL_ADD, EPOLLIN | EPOLLET); // 主线程的 _listensock 就不创建一个 Connection 了，_listensock 的创建和销毁有Sock管理执行的只有一个 Accept 函数
      // 创建获取就绪描述符相关信息的数组
      _maxlen = MAXLEN;
      _epoll_events = new struct epoll_event[_maxlen];
    }

    void Start()
    {
      int timeout = 1000; // 阻塞-1    非阻塞0     定时返回，单位是毫秒
      while (true)
      {
        // 有 readyNum 个就绪的 _ioSock / _listensock 描述符获取到 _epoll_events 中了
        int readyNum = _epoll.Wait(_epoll_events, _maxlen, timeout);
        if (readyNum > 0)
          logMessage(NORMAL, "有描述符就绪");
        else
          logMessage(NORMAL, "_epoll.Wait......");

        for (int i = 0; i < readyNum; i++)
        {
          int sock = _epoll_events[i].data.fd;
          uint32_t events = _epoll_events[i].events;

          // 1. 将所有的异常问题都转化为，读写的问题
          if (events & EPOLLERR || events & EPOLLHUP)
            events |= (EPOLLIN | EPOLLOUT);

          // 2. _listensock 就绪
          if (sock == _sock.Fd() && (events & EPOLLIN)) // _listensock 获取新连接（也是可以创建 Connection，再有 EPOLLIN 时调用 Accepter）
          {
            Accepter();
            logMessage(DEBUG, "_sock.Fd() 获取新连接");
          }

          // 3. _ioSock 就绪
          if (sock != _sock.Fd())
          {
            logMessage(DEBUG, "conn->_ioSock 读写事件");
            if (events & EPOLLIN && IsExitConnections(sock) && _connections[sock]->_reader != nullptr)
              _connections[sock]->_reader(_connections[sock]);
            if (events & EPOLLOUT && IsExitConnections(sock) && _connections[sock]->_sender != nullptr)
              _connections[sock]->_sender(_connections[sock]);
          }
        }
      }
    }
    // 判断 sock 是否在 _connections 中
    bool IsExitConnections(int sock)
    {
      return _connections.find(sock) != _connections.end(); //  != 的话说明是存在的
    }

    // 后面可以把这个函数封装到 class Sock 类中
    // _listensock 的 Accepter 获取 new _ioSock
    void Accepter()
    {
      while (true)
      {
        std::string clientip;
        uint16_t clientport;
        int err = 0;
        int ioSock = _sock.Accept(&clientip, &clientport, &err);
        if (ioSock > 0)
        {
          // 为 ioSock 创建 Connection
          AddConnection(ioSock, EPOLLIN | EPOLLET, &clientip, &clientport);
          logMessage(NORMAL, "_listensock accept a new ioSock: %d", ioSock);
          // break; // 一直监听
        }
        else
        {
          logMessage(FATAL, "_listensock accept a new ioSock < 0");
          if (err == EAGAIN || err == EWOULDBLOCK) // 非阻塞导致的可再次读取
            break;
          else if (err == EINTR) // 被信号中断了
            continue;
          else
            break;
        }
      }
    }
    // 创建 new Connection
    void AddConnection(int ioSock, uint32_t events, std::string *clientip, uint16_t *clientport)
    {
      // ioSock 改为非阻塞
      if (events & EPOLLET)
        Util::SetNonBlock(ioSock);

      // ioSock 创建 Connection
      // reader, sender, execpter 使用的是：TcpServer 类的成员函数
      // Connection *conn = new Connection(this, ioSock,
      //                                   std::bind(&TcpServer::reader, this, std::placeholders::_1),
      //                                   std::bind(&TcpServer::sender, this, std::placeholders::_1),
      //                                   std::bind(&TcpServer::execpter, this, std::placeholders::_1));
      // reader, sender, execpter 使用的是：main 函数创建 TcpServer 对象时传递的函数
      Connection *conn = new Connection(this, ioSock,
                                        std::bind(reader, std::placeholders::_1),
                                        std::bind(sender, std::placeholders::_1),
                                        std::bind(execpter, std::placeholders::_1));
      // 防范一下 _connections 中的 ioSock 是否删除
      if (_connections.find(ioSock) != _connections.end())
      {
        logMessage(FATAL, "ioSock has");
        return;
      }
      _connections.insert(std::pair<int, Connection *>(ioSock, conn));
      // ioSock 放入到 epoll 中被监听
      _epoll.OpEpoll(ioSock, EPOLL_CTL_ADD, events); // EPOLLIN 是常设的， EPOLLOUT 是按需设置的
    }
    //
    void EnableReadWrite(Connection *conn, bool readable, bool writeable)
    {
      uint32_t event = (readable ? EPOLLIN : 0) | (writeable ? EPOLLOUT : 0) | EPOLLET;
      _epoll.OpEpoll(conn->_ioSock, EPOLL_CTL_MOD, event);
    }
  };
}