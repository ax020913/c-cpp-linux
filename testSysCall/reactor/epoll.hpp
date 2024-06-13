#pragma once

#include <sys/epoll.h>
#include <stdint.h>

/*

用来封装 epoll 的三个相关的函数：

  int epoll_create(int size);

  int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);

  int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

*/

class Epoll
{
private:
  int _epollfd;
  int _maxlen;

public:
  Epoll(int epollfd = -1, int maxlen = 64) : _epollfd(epollfd), _maxlen(maxlen) {}
  ~Epoll()
  {
    if (_epollfd != -1)
      close(_epollfd);
  }

  // 创建一个 epoll 模型
  // 封装 int epoll_create(int size);
  void Create()
  {
    _epollfd = epoll_create(_maxlen);
    if (_epollfd == -1)
    {
      perror("epoll_create");
      exit(-1);
    }
  };

  // 对于 sock 进行增删改操作
  // 封装 int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
  int OpEpoll(int sock, int op, uint32_t events)
  {
    int ret = 0;
    if (op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD)
    {
      struct epoll_event ev;
      ev.events = events;
      ev.data.fd = sock;
      ret = epoll_ctl(_epollfd, op, sock, &ev);
    }
    else if (op == EPOLL_CTL_DEL)
      ret = epoll_ctl(_epollfd, op, sock, nullptr);
    else
      ret = -1;

    return ret == 0;
  };

  // 参数 events 是一个输入输出型参数，用来获取epoll就绪队列中的就绪描述符，传递出去
  // 封装 int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
  int Wait(struct epoll_event *events, int maxevents, int timeout)
  {
    int readyNum = epoll_wait(_epollfd, events, maxevents, timeout);
    return readyNum;
  }

  // 关闭 _epollfd
  void Close()
  {
    if (_epollfd != -1)
      close(_epollfd);
  };
};