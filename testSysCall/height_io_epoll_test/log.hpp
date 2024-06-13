#pragma once

#include <iostream>
#include <string>
#include <cstdarg>
#include <ctime>
#include <unistd.h>
#include <time.h>

#define DEBUG 0
#define NORMAL 1
#define WARNING 2
#define ERROR 3
#define FATAL 4

//
const char *to_levelstr(int level)
{
  switch (level)
  {
  case DEBUG:
    return "DEBUG";
  case NORMAL:
    return "NORMAL";
  case WARNING:
    return "WARNING";
  case ERROR:
    return "ERROR";
  case FATAL:
    return "FATAL";
  default:
    return nullptr;
  }
}

// 使用：logMessage(2, "%s %s", "hello world", "exe");
//      [WARNING][2024-5-2 19-27-53][pid: 92552]hello world exe
void logMessage(int level, const char *format, ...)
{
  // 1. 日志头部信息
#define NUM 1024
  time_t now;
  time(&now);
  char logprefix[NUM];
  snprintf(logprefix, sizeof(logprefix), "[%s][%d-%d-%d %d-%d-%d][pid: %d]",
           to_levelstr(level),
           // 注意 tm_year + 1900，tm_mon + 1
           localtime(&now)->tm_year + 1900, localtime(&now)->tm_mon + 1, localtime(&now)->tm_mday,
           localtime(&now)->tm_hour, localtime(&now)->tm_min, localtime(&now)->tm_sec,
           getpid());

  // 2. 日志内容部分
  char logcontent[NUM];
  va_list arg;
  va_start(arg, format);
  vsnprintf(logcontent, sizeof(logcontent), format, arg);

  std::cout << logprefix << logcontent << std::endl;
}