#include "TcpServer.hpp"

#include "protocol.hpp"

#include <memory>

// 程序使用说明
void Usage()
{
  std::cout << "Usage:"
            << "\n\t"
            << "./a.out serverPort"
            << "\n\n";
}

// _reader
void reader(ax::Connection *conn)
{
  // ET ===> 非阻塞的读取
  logMessage(DEBUG, "_reader......");
  char inBuffer[1024];
  while (true)
  {
    int readSize = recv(conn->_ioSock, inBuffer, sizeof(inBuffer) - 1, 0);
    logMessage(DEBUG, "readSize = %d", readSize);
    if (readSize > 0)
    {
      inBuffer[readSize] = 0; // '\0'
      conn->_inBuffer += inBuffer;
      logMessage(DEBUG, "recv: %s", conn->_inBuffer);
      conn->_tcpServerPtr->_service(conn); // 其实就是 transaction 函数
    }
    else if (readSize == 0)
    {
      // 全面的看待阻塞非阻塞 读取返回为0的情况：不一定是对方关闭了连接，可能代码逻辑有问题，哈哈哈
      // 1. 使用 telnet 127.0.0.1 8888 测试时可见，最后 quit 后，recv会返回0 ===> 双方都是正常的
      // 2. TCPServer recv 用一个空的字符数组接收，recv会返回0 ===> TcpServer recv方不正常
      if (conn->_execpter)
      {
        conn->_execpter(conn);
        return;
      }
    }
    else
    {
      logMessage(DEBUG, "reader errno = %d", errno);
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      else if (errno == EINTR)
        continue;
      else
      {
        if (conn->_execpter != nullptr)
        {
          conn->_execpter(conn);
          return;
        }
      }
    }
  }
}

// _sender
void sender(ax::Connection *conn)
{
  // ET ===> 非阻塞的写入
  logMessage(DEBUG, "_sender......");
  while (true)
  {
    int sendSize = send(conn->_ioSock, conn->_outBuffer.c_str(), conn->_outBuffer.size(), 0);
    logMessage(DEBUG, "sendSize = %d", sendSize);
    if (sendSize > 0)
    {
      // debug: 先移除，再判断是否(放下面的 else 中的话，是会有问题的 ===> 每次发送，最后一次发送，send会返回0，但是是正常的，因为会发送一次空字符串)
      conn->_outBuffer.erase(0, sendSize);
      if (conn->_outBuffer.empty() == true) // 发送完了的话直接退出发送
        break;
      // else
      // conn->_outBuffer.erase(0, sendSize); // 还没发送完的话，移除已经发送完的内容
    }
    else if (sendSize == 0)
    {
      // server非阻塞的发送：send返回值为0 ===>
      // 1. client 可能没有读取，造成 conn->_ioSock 的发送缓冲区满了，返回 0 ===> 正常
      // 2. client 关闭了，就没有读取，返回 0 ===> 正常
      // 所以非阻塞 send 返回 0 不一定是 client 关闭，可能 client 临时没有读取了。但是 client 正常的话，是会读取的，所以可以判定 client 关闭。
      // 3. 也有可能 send 的第三个参数为 0，也就是说，发送了一个空字符串。 ===> 下面加上 conn->_ourBuffer.size() 是否为空的判断 ===> TcpServer 不正常
      if (conn->_outBuffer.size() != 0 && conn->_execpter)
      {
        conn->_execpter(conn);
        return;
      }
      else if (conn->_outBuffer.size() == 0) // 发了一个空字符串，send返回0
        break;
    }
    else
    {
      logMessage(DEBUG, "sender errno = %d", errno);
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      else if (errno == EINTR)
        continue;
      else
      {
        if (conn->_execpter != nullptr)
        {
          conn->_execpter(conn);
          return;
        }
      }
    }
  }

  // 如果没有发送完毕，需要对对应的sock开启对写事件的关系， 如果发完了，我们要关闭对写事件的关心！
  if (conn->_outBuffer.empty() == false)
    conn->_tcpServerPtr->EnableReadWrite(conn, true, true);
  else
    conn->_tcpServerPtr->EnableReadWrite(conn, true, false);
}
// _execpter
void execpter(ax::Connection *conn)
{
  // epoll 中先移除，再 close
  conn->_tcpServerPtr->_epoll.OpEpoll(conn->_ioSock, EPOLL_CTL_DEL, 0);
  conn->Close();
  conn->_tcpServerPtr->_connections.erase(conn->_ioSock);

  logMessage(DEBUG, "_execpter...... _connectios.size = %d", conn->_tcpServerPtr->_connections.size());
  delete conn;
}

// 1. 回响服务 //////////////////////////////////////////////////////////////////////////////////////////////
// TcpServer 提供的服务: echoServer 回响 ---> 直接返回输入的 _inBuffer
void echoServer(ax::Connection *conn)
{
  logMessage(DEBUG, "echoServer...");
  conn->_outBuffer += conn->_inBuffer; // 直接获取刚刚用户输入的内容 _inBuffer
  conn->_inBuffer = "";                // 再把 _inBuffer 清空，不然后面输入的都在 _inBuffer 中
  if (conn->_sender != nullptr)
    conn->_sender(conn);
}

// 2. 计算服务 //////////////////////////////////////////////////////////////////////////////////////////////
// TcpServer 提供的服务: calculate 计算服务
bool cal(const Request &req, Response &resp)
{
  // req已经有结构化完成的数据啦，你可以直接使用
  resp.exitcode = OK;
  resp.result = OK;

  switch (req.op)
  {
  case '+':
    resp.result = req.x + req.y;
    break;
  case '-':
    resp.result = req.x - req.y;
    break;
  case '*':
    resp.result = req.x * req.y;
    break;
  case '/':
  {
    if (req.y == 0)
      resp.exitcode = DIV_ZERO;
    else
      resp.result = req.x / req.y;
  }
  break;
  case '%':
  {
    if (req.y == 0)
      resp.exitcode = MOD_ZERO;
    else
      resp.result = req.x % req.y;
  }
  break;
  default:
    resp.exitcode = OP_ERROR;
    break;
  }

  return true;
}

void calculate(ax::Connection *conn)
{
  std::string onePackage;
  while (ParseOnePackage(conn->_inBuffer, &onePackage))
  {
    std::string reqStr;
    if (!deLength(onePackage, &reqStr))
      return;
    std::cout << "去掉报头的正文：\n"
              << reqStr << std::endl;

    // 2. 对请求Request，反序列化
    // 2.1 得到一个结构化的请求对象
    Request req;
    if (!req.deserialize(reqStr))
      return;

    Response resp;
    cal(req, resp);

    std::string respStr;
    resp.serialize(&respStr);
    // 5. 然后我们在发送响应
    // 5.1 构建成为一个完整的报文
    conn->_outBuffer += enLength(respStr);

    std::cout << "--------------result: " << conn->_outBuffer << std::endl;
  }
  // 直接发
  if (conn->_sender)
    conn->_sender(conn);

  // // 如果没有发送完毕，需要对对应的sock开启对写事件的关系， 如果发完了，我们要关闭对写事件的关心！
  // if (!conn->_outBuffer.empty())
  //     conn->_tcpServerPtr->EnableReadWrite(conn, true, true);
  // else
  //     conn->_tcpServerPtr->EnableReadWrite(conn, true, false);
}

// ./ma serverport
int main(int argc, char **argv)
{
  if (argc < 2)
  {
    Usage();
    exit(-1);
    // return -1;
  }

  // 1. TcpServer提供回响服务：echoServer
  // 2. TcpServer提供计算服务：calculate

  std::unique_ptr<ax::TcpServer> tcpServer(new ax::TcpServer(reader, sender, execpter, echoServer, atoi(argv[1]), 64));
  tcpServer->InitServer();
  tcpServer->Start();

  return 0;
}
