# Lil webserv

## I/O Multiplexing

There will be one thread of control and it's going to go through a thing called an **event loop**. What this event loop will do is that, it's taking new events that comes in and usually these are going to be I/O events like *bytes came in of a file descriptor*, *a file descriptor is ready to be written to*. This event loop will then handle those specific bytes and then wait for more bytes to come in. This allows to have one thread that does CPU-based work but it's only going to do them when certain things happening.

> **CPU-based work**: works that requires CPU of the computer to actively spend time thinking and computing results, it requires a thread to handle it
> **I/O (Input/Output)**: works that involve waiting for something else to provide input or to send output. For example: waiting to read a file from a file system, making a network request to another server, or just waiting for time to pass.

I/O multiplexing is I/O with many sources. It is a problem of magnitude and we are talking in the context of network application where there's aserver and client. Server has to communicate with many clients, while client only just need to communicate with one server. Hence, as the scale of operations goes up, the number of clients that a server has to communicate with also increases. This is the problem of I/O multiplexing, the problem of server being able to communicate with many clients.

If we go deeper into the communication problem, we can conclude that talking (write operation) is easy, it's listening (read operation) that is difficult. For writing, all you need to do is write the data then send, and the work is done. Reading from a socket is difficule because a read or a `recv()` call blocks and completes only when data is available. And since we are blocked, waiting for data on one socket, we can't do a read on other sockets. Obviously, this is not going to work.

Here comes the I/O monitoring calls: `select()`, `poll()`, `epoll()` to help us with the problem.

### `select()` - Old school

`select()` doesn't get used very often in the modern world as there are better more efficient ways to do this. But, it still does a good job of showing us the core element that drives I/O multiplexing.

`select()` gives you a way to simultaneously check multiple sockets to see if they have data waiting to be `recv()`d or if you can `send()` data to them without blocking, or if some exception has occurred.

```cpp
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

int select(
    int numfds,               // highest-numbered socket descriptor plus one, max number of fd to check for activity
    fd_set *readfds,          // use this if you want to know when any of the sockets in the set is ready to recv() data
    fd_set *writefds,         // use this if you want to know when any of the sockets in the set is ready to send() data to
    fd_set *exceptfds,        // use this if you want to know when any of the sockets in the set occurs some error.
    struct timeval *timeout   // tell select() how long to check these sets for
);

// NOTE: readfds, writefds, exceptfds can be NULL, if you're not interested in those types of events.

// macros to populate sets of socket descriptors
FD_SET(int fd, fd_set *set);   // Add fd to the set
FD_CLR(int fd, ft_set *set);   // Remove fd from the set
FD_ISSET(int fd, fd_set *set); // Return true if fd is in the set
FD_ZERO(fd_set *set);          // Clear all entries from the set
```

Under the hood, `select()` puts your program to sleep until something happens and we tell the operating system what that something should be. You got these three `fd_set`s, let's say `readfds`. That's basically telling `select()` that, "Hey, I'm giving you a collection of fds and if any of them are ready for **reading**, wake me up". For `writefds`, it's let `select()` to monitor a collection of fds, and notify us is any of them are ready for **writing**. If any of them are in some exceptional state (`exceptfds`) like something went wrong, also wake me up so that we can deal with that.

### `poll()`

```cpp
#include <sys/poll.h>

int poll(struct pollfd *ufds, unsigned int nfds, int timeout);
```

Works similar to `select()` in that they both watch sets of fds for events, such as incoming data ready to `recv()`, socket ready to `send()` data to, out-of-band data ready to `recv()`, errors, etc.

The basic idea is that you pass a `nfds`, an array of `struct pollfd`s in `ufds`, along with a `timeout` in milliseconds. The `timeout` can be negative if you want to wait forever. If no event happens on any of the socket descriptors by the timeout, `poll()` will return.

```cpp
struct pollfd {
    int fd;        // the socket descriptor (the client)
    short events;  // bitmap of events we're interested in
    short revents; // when poll() returns, bitmap of events that occurred
}
```

Before calling `poll()`, load `fd` with the socket descriptor (if you set `fd` to a negative number, this `struct pollfd` is ignored and its `revents` field is set to zero) and then construct the `events` field by bitwise-ORing the following macros:

| Macro | Description |
| :---- | :---- |
| `POLLIN` | Alert me when data is ready to `recv()` on this socket. |
| `POLLOUT` | Alert me when I can `send()` data to this socket without blocking. |
| `POLLPRI` | Alert me when out-of-band data is ready to `recv()` on this socket. |

Once the `poll()` call returns, the `revents` field will be constructed as a bitwise-OR of the above fields, telling you which descriptors actaully have had that event occur. Additionally, these other fields might be present:

| Macro | Description |
| :---- | :---- |
| `POLLERR` | An error has occurred on this socket. |
| `POLLHUP` | The remote side of the connection hung up. |
| `POLLNVAL` | Something was wrong with the socket descriptor `fd` — maybe it’s uninitialized? |

`poll()` retuns the number of elements in the `ufds` array that have had event occur on them; this can be `0` if the timeout occurred. Also returns `-1` on error and errno will be set accordingly.

## `struct`s

### `addrinfo`

```cpp
struct addrinfo {
    int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
    int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
    int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
    int              ai_protocol;  // use 0 for "any"
    size_t           ai_addrlen;   // size of ai_addr in bytes
    struct sockaddr *ai_addr;      // struct sockaddr_in or _in6
    char            *ai_canonname; // full canonical hostname

    struct addrinfo *ai_next;      // linked list, next node
};
```

## System Calls or Bust

### `getaddrinfo()` - Prepare to launch!

`getaddrinfo` helps set up the `struct`s you need later on.

A tiny bit of history: it used to be that you would use a function called `gethostbyname()` to do DNS lookups. Then you’d load that information by hand into a `struct sockaddr_in`, and use that in your calls.

But that is no longer neccessary. In these modern times, you now have function `getaddrinfo()` that does all kinds of good stuff for you, including DNS and service lookup, and fills out the `struct`s you need.

```cpp
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int getaddrinfo(const char *node,     // e.g. "www.example.com" or IP
                const char *service,  // e.g. "http" or port number
                const struct addrinfo *hints,
                struct addrinfo **res);
```

- `node`: host name to connect to, or an IP address
- `service`: port number, like "80". Noted that we are allowed to use port that is greater than 1024 and right up to 65535
- `hints`: points to a `struct addrinfo` that you've already filled out with relevant information.
- `res`: a pointer to a linked-list, `res`, of results

If there's an error, `getaddrinfo()` returns **non-zero**.

> Noted that the linked list of `struct addrinfo`s, each of which contains a struct `sockaddr` of some kind we can use later.

Finally, when we're eventually all done with the linked list that `getaddrinfo()` allocated for us, we must free it up with a call to `freeaddrinfo()`.

```cpp
// hints setup
struct addrinfo hints;

memset(&hints, 0, sizeof(hints)); // make sure the struct is empty
hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
hints.ai_flags = AI_PASSIVE; // signals that you intend to use the address information for a server socket, like one that listens for incoming connections
```

### `socket()` - Get the File Descriptor!

```cpp
#include <sys/types.h>
#include <sys/socket.h>

int socket(int domain, int type, int protocol);
```

- `domain`: `PF_INET` or `PF_INET6`
- `type`: `SOCK_STREAM` or `SOCK_DGRAM`
- `protocol`: can be set to `0` to choose the proper protocol for the given `type`

> `PF_INET` thing is a close relative of `AF_INET` that you can use when intializing the `sin_family` field in your `struct sockaddr_in`. In fact, they're so closely related that they actually have the same value. `AF` is address family, `PF` is protocol family. In short, the most correct way thing to do is to use `AF_INET` in your `struct sockaddr_in` and `PF_INET` in your call to `socket()`.

If you called `getaddrinfo` before this, you can actually just pass int the results from the call and feed them into `socket()` directly.


```cpp
int serverfd;
struct addrinfo hints, *res;

// pretend we already set up hints
getaddrinfo("www.xxx.com", "http", &hints, &res);

serverfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
```

On success, a `fd` for the new socket is returned. On error, `-1` is returned, and errno is set.

### `bind()` - What port am I on? (FOR SERVER)

Once you have a socket, you might have to associate that socket with a port on your local machine. This is commonly done if you're going to `listen()` for incoming connections on a specific port. Normally, it's not necessary to do bind on the client.

```cpp
#include <sys/types.h>
#include <sys/socket.h>

int bind(int sockfd, struct sockaddr *addr, int addrlen);
```

- `sockfd`: socket fd returned by `socket()`
- `addr`: a pointer to a struct `sockaddr` that contains information about your address, namely, port and IP address
- `addrlen`: length in bytes of that address

On error, `-1` is returned and errno is set. On success, `0` is returned.

### `connect()` - Hey, you! (FOR CLIENT)

```cpp
#include <sys/types.h>
#include <sys/socket.h>

int connect(int sockfd, struct sockaddr *serv_addr, int addrlen);
```

- `sockfd`: socket file descriptor, as returned by the socket
- `serv_addr`: containing the destination port and IP address
- `addrlen`: length in bytes of the server address structure

Again, all these info can be gathered from the result of `getaddrinfo()` call, which rocks.

```cpp
struct addrinfo hints, *res;
int sockfd;

// first, load up address structs with getaddrinfo():
memset(&hints, 0, sizeof hints);
hints.ai_family = AF_UNSPEC;
hints.ai_socktype = SOCK_STREAM;
getaddrinfo("www.example.com", "3490", &hints, &res);

// make a socket:
sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

// connect!
connect(sockfd, res->ai_addr, res->ai_addrlen);
```

On error, it will return `-1` and set errno. Else, `0` is returned.

### `listen()` - Will somebody please call me?

Wait for incoming connections and handle them in some way. The process is two step: first you `listen()`, then you `accept()`.

```cpp
int listen(int sockfd, int backlog);
```

- `sockfd`: socket fd from the `socket()` system call
- `backlog`: number of connections allowed on the incoming queue. incoming connections are going to wait in this queue until you `accept()` them and this is the limit on how many can queue up. Most systems silently limit this number to about 20; you can probably get away with setting it to `5` or `10`.

As usual, `listen()` returns `-1` and sets errno on error.

`bind()` needs to be called before we call `listen()` so that the server is running on a specific port. So if you're going to be listening for incoming connections, the sequence of system calls you'll make is:

```cpp
getaddrinfo();
socket();
bind();
listen();
/* accept() goes here */
```

### `accept()` - Thank you for calling port 3490

Someone far far away will try to `connect()` (on client side) to your machine on a port that you are `listen()`ing on. Their connection will be queued up waiting to be `accept()`ed. You can `accept()` and you tell it to get the pending connection. It'll return to you a *brand new socket file descriptor* to use for this single connection. The original one that you `listen()`ing on is still listening for more new connections, and the newly created on is finally ready to `send()` and `recv()`.

```cpp
#include <sys/types.h>
#include <sys/socket.h>

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
```

- `sockfd`: the `listen()`ing socket fd.
- `addr`: a pointer to a local `struct sockaddr_storage`. This is where the information about the incoming connection will go, and with it you can determine which host is calling you from which port.
- `addrlen`: a local integer variable that should be set to `sizeof(struct sockaddr_storage)` before its address is passed to `accept()`.

Again, `accept()` returns `-1` and sets `errno` if an error occurs.

### `send()` and `recv()` - Talk to me, baby!

These two functions are for communicating over stream sockets or connected datagram sockets.

The `send()` call (**client**):

```cpp
int send(int sockfd, const void *msg, int len, int flags);
```

- `sockfd`: socket fd you want to send data to
- `msg`: a pointer to the data you want to send
- `len`: length of that data in bytes
- `flags`: just set to 0

```cpp
// Example
char *msg = "Ricky was here!";
int len, bytes_sent;
...
len = strlen(msg);
bytes_sent = send(sockfd, msg, len, 0);
```

Fear not, `send()` returns the number of bytes actually sent out and this *might be less than the number you told it to send!* Sometimes, you tell it to send a whole gob of data and it just can't handle it. It'll fire off as much of data as it can, and trust you to send the rest later.

Remember, if the value returned by `send()` doesn't match the value in `len`, it's up to you to send the rest of the string. The good new is this: if the packet is small, it will probably manage to send the whole thing all in one go.

Again, `-1` is returned on error and errno is set.


The `recv()` call (**server**), very similar to `send()`:

```cpp
int recv(int sockfd, void *buf, int len, int flags);
```

- `sockfd`: socket fd you want to read from
- `buf`: the buffer to read the information into
- `len`: max length of the buffer
- `flags`: just set to 0

`recv()` returns the number of bytes actually read into the buffer, or `-1` on error. Warning! `recv()` can return 0! This means the client side has closed the connection on you.

### `close()` - Get outta my face!

Close socket fd. It's the same way to close a fd file in unix:

```cpp
close(sockfd);
```

This will prevent anymore reads and writes to the socket. Anyone attempting to read or write the socket on the remote end will receive an error.

## I/O Multiplexing

/Users/wricky-t/.docker/bin:/Users/wricky-t/goinfre/.brew/bin:/Library/Frameworks/Python.framework/Versions/3.10/bin:/Library/Frameworks/Python.framework/Versions/3.7/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/munki
