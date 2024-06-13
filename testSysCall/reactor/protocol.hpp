#pragma once

#include <iostream>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <jsoncpp/json/json.h>

#define SEP " "
#define SEP_LEN strlen(SEP) // 不敢使用sizeof()
#define LINE_SEP "\r\n"
#define LINE_SEP_LEN strlen(LINE_SEP) // 不敢使用sizeof()

enum
{
  OK = 0,
  DIV_ZERO,
  MOD_ZERO,
  OP_ERROR
};

// "x op y" -> "content_len"\r\n"x op y"\r\n
// "exitcode result" -> "content_len"\r\n"exitcode result"\r\n
std::string enLength(const std::string &text)
{
  std::string send_string = std::to_string(text.size());
  send_string += LINE_SEP;
  send_string += text;
  send_string += LINE_SEP;

  return send_string;
}

// "content_len"\r\n"exitcode result"\r\n
bool deLength(const std::string &package, std::string *text)
{
  auto pos = package.find(LINE_SEP);
  if (pos == std::string::npos)
    return false;
  std::string text_len_string = package.substr(0, pos);
  int text_len = std::stoi(text_len_string);
  *text = package.substr(pos + LINE_SEP_LEN, text_len);
  return true;
}

// 没有人规定我们网络通信的时候，只能有一种协议！！
// 我们怎么让系统知道我们用的是哪一种协议呢？？
// "content_len"\r\n"协议编号"\r\n"x op y"\r\n

class Request
{
public:
  Request() : x(0), y(0), op(0)
  {
  }
  Request(int x_, int y_, char op_) : x(x_), y(y_), op(op_)
  {
  }
  // 1. 自己写
  // 2. 用现成的
  bool serialize(std::string *out)
  {
#ifdef MYSELF
    *out = "";
    // 结构化 -> "x op y";
    std::string x_string = std::to_string(x);
    std::string y_string = std::to_string(y);

    *out = x_string;
    *out += SEP;
    *out += op;
    *out += SEP;
    *out += y_string;
#else
    Json::Value root;
    root["first"] = x;
    root["second"] = y;
    root["oper"] = op;

    Json::FastWriter writer;
    // Json::StyledWriter writer;
    *out = writer.write(root);
#endif
    return true;
  }

  // "x op yyyy";
  bool deserialize(const std::string &in)
  {
#ifdef MYSELF
    // "x op y" -> 结构化
    auto left = in.find(SEP);
    auto right = in.rfind(SEP);
    if (left == std::string::npos || right == std::string::npos)
      return false;
    if (left == right)
      return false;
    if (right - (left + SEP_LEN) != 1)
      return false;

    std::string x_string = in.substr(0, left); // [0, 2) [start, end) , start, end - start
    std::string y_string = in.substr(right + SEP_LEN);

    if (x_string.empty())
      return false;
    if (y_string.empty())
      return false;
    x = std::stoi(x_string);
    y = std::stoi(y_string);
    op = in[left + SEP_LEN];
#else
    Json::Value root;
    Json::Reader reader;
    reader.parse(in, root);

    x = root["first"].asInt();
    y = root["second"].asInt();
    op = root["oper"].asInt();
#endif
    return true;
  }

public:
  // "x op y"
  int x;
  int y;
  char op;
};

class Response
{
public:
  Response() : exitcode(0), result(0)
  {
  }
  Response(int exitcode_, int result_) : exitcode(exitcode_), result(result_)
  {
  }
  bool serialize(std::string *out)
  {
#ifdef MYSELF
    *out = "";
    std::string ec_string = std::to_string(exitcode);
    std::string res_string = std::to_string(result);

    *out = ec_string;
    *out += SEP;
    *out += res_string;
#else
    Json::Value root;
    root["exitcode"] = exitcode;
    root["result"] = result;

    Json::FastWriter writer;
    *out = writer.write(root);
#endif
    return true;
  }
  bool deserialize(const std::string &in)
  {
#ifdef MYSELF
    // "exitcode result"
    auto mid = in.find(SEP);
    if (mid == std::string::npos)
      return false;
    std::string ec_string = in.substr(0, mid);
    std::string res_string = in.substr(mid + SEP_LEN);
    if (ec_string.empty() || res_string.empty())
      return false;

    exitcode = std::stoi(ec_string);
    result = std::stoi(res_string);
#else
    Json::Value root;
    Json::Reader reader;
    reader.parse(in, root);

    exitcode = root["exitcode"].asInt();
    result = root["result"].asInt();
#endif
    return true;
  }

public:
  int exitcode; // 0：计算成功，!0表示计算失败，具体是多少，定好标准
  int result;   // 计算结果
};

// "content_len"\r\n"x op y"\r\n     "content_len"\r\n"x op y"\r\n"content_len"\r\n"x op
bool ParseOnePackage(std::string &inbuffer, std::string *text)
{
  *text = "";
  // 分析处理
  auto pos = inbuffer.find(LINE_SEP);
  if (pos == std::string::npos)
    return false;

  std::string text_len_string = inbuffer.substr(0, pos);
  int text_len = std::stoi(text_len_string);
  int total_len = text_len_string.size() + 2 * LINE_SEP_LEN + text_len;

  if (inbuffer.size() < total_len)
    return false;

  // 至少有一个完整的报文
  *text = inbuffer.substr(0, total_len);
  inbuffer.erase(0, total_len);
  return true;
}

// "content_len"\r\n"x op y"\r\n"content_len"\r\n"x op y"\r\n"content_len"\r\n"x op
bool recvPackage(int sock, std::string &inbuffer, std::string *text)
{
  char buffer[1024];
  while (true)
  {
    ssize_t n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n > 0)
    {
      buffer[n] = 0;
      inbuffer += buffer;
      // 分析处理
      auto pos = inbuffer.find(LINE_SEP);
      if (pos == std::string::npos)
        continue;
      std::string text_len_string = inbuffer.substr(0, pos);
      int text_len = std::stoi(text_len_string);
      int total_len = text_len_string.size() + 2 * LINE_SEP_LEN + text_len;
      // text_len_string + "\r\n" + text + "\r\n" <= inbuffer.size();
      std::cout << "处理前#inbuffer: \n"
                << inbuffer << std::endl;

      if (inbuffer.size() < total_len)
      {
        std::cout << "你输入的消息，没有严格遵守我们的协议，正在等待后续的内容, continue" << std::endl;
        continue;
      }

      // 至少有一个完整的报文
      *text = inbuffer.substr(0, total_len);
      inbuffer.erase(0, total_len);

      std::cout << "处理后#inbuffer:\n " << inbuffer << std::endl;

      break;
    }
    else
      return false;
  }
  return true;
}