This project builds a simple **HTTP server** in C and socket, 
which can serve static html files under designated directory. 

The main topics explored include:

* socket programming
* multi-threaded programming
* HTTP protocol implementation
* memory, pointer, and string manipulation

### Run

In unix-like system, simply do...

```
make
cd build
./main
```

In other systems, use `CMakeLists.txt`.

### Reference

* [Beej Jorgensen || Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/#sendrecv)
* [Eduonix-Tech | Udemy || Learn Socket Programming in C from Scratch](https://www.udemy.com/course/learn-socket-programming-in-c-from-scratch)
* [Errata Security || TCP/IP, Sockets, and SIGPIPE](https://blog.erratasec.com/2018/10/tcpip-sockets-and-sigpipe.html#.XyRhOElS_ok)
* [Kuan-Yen Chou | Github || simple-httpd.c](https://github.com/kyechou)
* [MDN || HTTP](https://developer.mozilla.org/en-US/docs/Web/HTTP)
