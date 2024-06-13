#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <pwd.h>
#include <semaphore.h>
#include <signal.h>
#include <assert.h>
#include <ulimit.h>
#include <sys/stat.h>
#include <fcntl.h>

// 注意：
// 下面的两个数组放在 main 函数内是有问题的哦：error: declaration of ‘char (* myargv)[32]’ shadows a parameter
char lineCommand[32]; // 获取命令行字符数组
char *myargv[32];     // 解析命令行字符数组

int lastCode = 0, lastSig = 0; // echo $? 输出上一次shell执行程序的退出码

int main(int argc, char **myargv)
{
  extern char **environ;

  while (true)
  {
    // 主机名  当前目录
    char hostname[32], pwd[128]; // 空间可以适量的给大一点的哦
    struct passwd *password = getpwuid(getuid());
    gethostname(hostname, sizeof(hostname) - 1);
    getcwd(pwd, sizeof(pwd) - 1);
    int len = strlen(pwd);
    char *p = pwd + len - 1;
    while (*p != '/')
      p--;
    p++;
    printf(R"(["%s"@'%s' %s]# )", password->pw_name, hostname, p);
    fflush(stdout);

    // 1. 获取命令行
    char *linecommand = fgets(lineCommand, sizeof(lineCommand) - 1, stdin);
    assert(linecommand != NULL);
    (void)linecommand;
    lineCommand[strlen(lineCommand) - 1] = '\0';
    printf("lineCommand: %s\n", lineCommand);

    // 2. 解析命令行
    int i = 0;
    myargv[i++] = strtok(lineCommand, " ");
    // 会影响后面的 ls > tt.t
    // if (myargv[0] != NULL && strcmp(myargv[0], "ls") == 0)
    //   myargv[i++] = (char *)"--color=auto"; // 打印的内容带上颜色
    while (myargv[i++] = strtok(NULL, ""))
      ;
    for (int i = 0; myargv[i] != NULL; i++)
      printf("%s ", myargv[i]);
    putchar(10);

    {
      // 内置命令定义：如果是cd命令，不需要创建子进程，让shell自己执行对应的命令，本质就是执行系统接口；像这种不需要我们的子进程来执行，而是让shell自己执行的命令就是内建命令
      // 查看：man help 可见什么命令是内建命令
      // cd [dirName]    注意使用 cd 命令的时候，获取主机名和目录名的代码得放到 while(true) 中的哦；如果不用的话，也是会每次获取的。有利有弊吧
      if (myargv[0] != NULL && strcmp(myargv[0], "cd") == 0)
      {
        if (myargv[1] != NULL)
          chdir(myargv[1]);
        continue;
      }
      // echo $? 或 echo hello world
      if (myargv[0] != NULL && strcmp(myargv[0], "echo") == 0)
      {
        if (myargv[1] != NULL && strcmp(myargv[1], "$?") == 0)
          printf("%d %d\n", lastCode, lastSig);
        else if (myargv[1] != NULL)
          printf("%s\n", myargv[1]);
        continue;
      }
      // ulimit [cmd] [newlimit]
      if (myargv[0] != NULL && strcmp(myargv[0], "ulimit") == 0)
      {
        if (myargv[1] == NULL) // ulimit (have bug: 之前设置好的 ulimit 也是会输出的 unlimited 的哦)
          printf("unlimited\n");
        else if (myargv[1] != NULL && myargv[2] == NULL) // ulimit 1024
          ulimit(atoi(myargv[1]));
        else if (myargv[1] != NULL && myargv[2] != NULL)
          printf("%d\n", ulimit(atoi(myargv[1]), atoi(myargv[2]))); // ulimit -u 1024
        continue;
      }
      // exit [exitCode]   退出后，shell可以通过 echo $? 来获取我们输入的 exitCode
      if (myargv[0] != NULL && strcmp(myargv[0], "exit") == 0)
      {
        exit(myargv[1] == NULL ? 0 : atoi(myargv[1]));
        continue;
      }
      // help (使用起来是不方便的)
      if (myargv[0] != NULL && strcmp(myargv[0], "help") == 0)
      {
        printf("%s\n", system(myargv[0]));
        continue;
      }
    }

    // 3.  非内置命令需要创建子进程执行处理结构输出到控制台上的哦
    pid_t child_pid = fork();
    if (child_pid == 0)
    {
      // 4. 子进程程序替换
      // putchar(10); // 不用手动换行了，父进程输入命令行会输入换行的

      // 查找是否有 < 、 > 、| (追加同理，难的是多个 <  >  |)，这里我们只处理一行命令中只有单个 <   >   |
      char LineCommand[32], filename[20];
      int flags = -1; // <: 0   >: 1   |: 2
      int i = 0, j = 0;
      for (; myargv[i] != NULL; i++)
      {
        // debug  >
        for (int k = 0; k < strlen(myargv[i]); k++)
          LineCommand[j++] = myargv[i][k];

        if (strcmp(myargv[i], "<") == 0) // filename < lineCommand
        {
          flags = 0;
          break;
        }
        else if (strcmp(myargv[i], ">") == 0) // lineCommand > filename
        {
          flags = 1;
          break;
        }
        else if (strcmp(myargv[i], "|") == 0) // lineCommand | filename
        {
          flags = 2;
          break;
        }
      }
      // debug >
      for (j = 0; j < strlen(myargv[i]); j++)
        filename[j++] = myargv[i + 1][j];

      printf("flags = %d\n", flags); // debug
      int ret = 0;
      if (flags == 0)
      {
      }
      else if (flags == 1)
      {
        printf("LineCommand: %s    filename: %s\n", LineCommand, filename); // debug
        return 0;

        FILE *fp = popen(LineCommand, "r");
        int filenameFD = open(filename, O_CREAT | O_WRONLY, 0777);

        char c;
        while (~(c = fgetc(fp)))
          write(filenameFD, &c, 1);
        return 0;
      }
      else if (flags == 2)
      {
      }
      else if (flags == -1)
        ret = execvp(myargv[0], myargv); // 刚刚好把 myargv 数组全传给它了（第一个参数不用，我也不用处理了，巧了，哈哈哈）

      if (ret == -1)
      {
        perror("execvp");
        exit(-1);
      }
      else
        printf("execvp success\n");
    }
    else
    {
      // 5. 获取子进程退出状态码: 防止资源泄露等问题
      int status;
      pid_t ret = waitpid(child_pid, &status, 0);
      assert(ret > 0);
      (void)ret;
      lastCode = ((status >> 8) & 0xFF);
      lastSig = (status & 0X7F);
    }
  }

  return 0;
}
