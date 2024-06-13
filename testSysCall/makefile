ma: test.cc
	g++ -o ma test.cc -g -rdynamic -lpthread -L/usr/lib64/mysql -lmysqlclient

# 上面的 -g -rdynamic 是为了 gdb 调试用的

clean:
	rm -r ma

