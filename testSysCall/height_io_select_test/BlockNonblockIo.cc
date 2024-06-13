#pragma once

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <vector>
#include <functional>

// fcntl 函数设置文件描述符的状态
void setNonBlock(int fd)
{
  int fl = fcntl(fd, F_GETFL);
  if (fl < 0)
  {
    std::cerr << "fcntl : " << strerror(errno) << std::endl;
    return;
  }
  fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

void printLog()
{
  std::cout << "this is a log" << std::endl;
}

void download()
{
  std::cout << "this is a download" << std::endl;
}

void executeSql()
{
  std::cout << "this is a executeSql" << std::endl;
}

#define INIT(v)            \
  do                       \
  {                        \
    v.push_back(printLog); \
  } while (0)

#define EXEC_OTHER(cbs)        \
  do                           \
  {                            \
    for (auto const &cb : cbs) \
      cb();                    \
  } while (0)

using func_t = std::function<void()>;

int main()
{
  // 非阻塞io，非阻塞时间可以去做别的事情
  std::vector<func_t> cbs;
  INIT(cbs);

  setNonBlock(0); // 设置标准输出为非阻塞状态
  char readbuffer[1024];
  while (true)
  {
    // printf(">>> ");
    // fflush(stdout); // putchar(10);
    ssize_t size = read(0, readbuffer, sizeof(readbuffer) - 1);
    if (size > 0) // 返回表是读取到内容的长度
    {
      readbuffer[size - 1] = 0;
      std::cout << "echo# " << readbuffer << std::endl;
    }
    else if (size == 0) // 读取到文件的结尾了 EOF 或 linux 中输入 ctrl d 表示文件的末尾
    {
      std::cout << "read end" << std::endl;
      break;
    }
    else // 出错
    {
      // 1. 当我不输入的时候，底层没有数据，算错误吗？不算错误，只不过以错误的形式返回了
      // 2. 我又如何区分，真的错了，还是底层没有数据？单纯返回值，无法区分！
      // std::cout << "EAGAIN: " << EAGAIN << " EWOULDBLOCK: " << EWOULDBLOCK << std::endl;
      if (errno == EAGAIN) // 因为非阻塞，read必须返回，所以errno是EAGAIN，数据没有准备好
      {
        std::cout << "我没错, 只是没有数据" << std::endl;
        EXEC_OTHER(cbs); // 去做别的事情
      }
      else if (errno == EINTR) // 信号中断
        continue;
      else // 其它的原因
      {
        std::cout << "size : " << size << " errno: " << strerror(errno) << std::endl;
        break;
      }
    }

    sleep(1);
  }
}