

### 1. strcpy 函数和内存重叠

注意：strcpy函数自己给自己拷贝(内存重叠的概念，可见官网 strcpy 函数的说明)

```cpp
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
  char s[15] = "hello world";
  strcpy(s, s + 1);
  puts(s);
  putchar(10);

  strcpy(s + 1, s);
  puts(s);

  return 0;
}
```

按理来说：`strcpy(s + 1, s);` 是会死循环的，因为前面的 `s[i]` 总是不会为 `'\0'` 的；但是测试并没有问题，可能使用 `s[i + 1]` 判断是否为 `'\0'` 的。

上面的情况也是可以看成 `UB` 问题的 ---- 未知问题。

