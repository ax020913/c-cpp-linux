#pragma once

/*

做为一个工具类：

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

namespace Util
{
  // 设置 sock 为非阻塞状态
  int SetNonBlock(int sock)
  {
    int ret = fcntl(sock, F_GETFL);
    if (ret == -1)
      return false;
    fcntl(sock, F_SETFL, ret | O_NONBLOCK);
    return true;
  }
}
