mybench
=======

#Http bench tool

##安装

./configure --prefix=$(WHERE_TO_INSTALL)

make

make install

##常用方法
- webbench www.baidu.com

启动1个工作线程，请求30秒

- webbench -c 20 -t 10 www.baidu.com

启动20个工作线程，请求10秒

- webbench -c 8 -t 10 -k www.baidu.com

启动100个工作线程，请求10秒，长连接

- webbench -c 10 -n 100 -t 10 www.baidu.com

启动10个工作线程，当请求到达10秒或请求次数到达100时，工作线程退出

- webbench http://127.0.0.1:8000/hello.txt

启动1个工作线程，使用8000端口，请求hello.txt 30秒

##注意

当未启用长连接时，若请求时间过长，客户端将出现大量TIMED_WAIT状态的TCP连接，严重时，会出现curl错误提示。
