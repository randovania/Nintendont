#ifndef _SOCKET_DEFINITIONS_H_
#define _SOCKET_DEFINITIONS_H_

//Defines taken from libogc's network.h and network_wii.c

#define INVALID_SOCKET  (~0)
#define SOCKET_ERROR  (-1)

#define INADDR_ANY 0
#define INADDR_BROADCAST 0xffffffff

#define IPPROTO_IP      0
#define IPPROTO_TCP      6
#define IPPROTO_UDP      17
#define  SOL_SOCKET      0xfff

#define  TCP_NODELAY     0x01
#define TCP_KEEPALIVE  0x02

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_RAW 3

#define AF_INET 2

/*
 * Option flags per-socket.
 */
#define  SO_DEBUG 0x0001    /* turn on debugging info recording */
#define  SO_ACCEPTCONN 0x0002 /* socket has had listen() */
#define  SO_REUSEADDR 0x0004 /* allow local address reuse */
#define  SO_KEEPALIVE 0x0008 /* keep connections alive */
#define  SO_DONTROUTE 0x0010 /* just use interface addresses */
#define  SO_BROADCAST 0x0020 /* permit sending of broadcast msgs */
#define  SO_USELOOPBACK 0x0040 /* bypass hardware when possible */
#define  SO_LINGER 0x0080 /* linger on close if data present */
#define  SO_OOBINLINE 0x0100 /* leave received OOB data in line */
#define  SO_REUSEPORT 0x0200 /* allow local address & port reuse */

#define SO_DONTLINGER    (int)(~SO_LINGER)

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF 0x1001 /* send buffer size */
#define SO_RCVBUF 0x1002 /* receive buffer size */
#define SO_SNDLOWAT 0x1003 /* send low-water mark */
#define SO_RCVLOWAT 0x1004 /* receive low-water mark */
#define SO_SNDTIMEO 0x1005 /* send timeout */
#define SO_RCVTIMEO 0x1006 /* receive timeout */
#define SO_ERROR 0x1007 /* get error status and clear */
#define SO_TYPE 0x1008 /* get socket type */

#define MSG_DONTWAIT    0x40

enum {
    IOCTL_SO_ACCEPT = 1,
    IOCTL_SO_BIND,
    IOCTL_SO_CLOSE,
    IOCTL_SO_CONNECT,
    IOCTL_SO_FCNTL,
    IOCTL_SO_GETPEERNAME, // todo
    IOCTL_SO_GETSOCKNAME, // todo
    IOCTL_SO_GETSOCKOPT,  // todo    8
    IOCTL_SO_SETSOCKOPT,
    IOCTL_SO_LISTEN,
    IOCTL_SO_POLL,        // todo    b
    IOCTLV_SO_RECVFROM,
    IOCTLV_SO_SENDTO,
    IOCTL_SO_SHUTDOWN,    // todo    e
    IOCTL_SO_SOCKET,
    IOCTL_SO_GETHOSTID,
    IOCTL_SO_GETHOSTBYNAME,
    IOCTL_SO_GETHOSTBYADDR,// todo
    IOCTLV_SO_GETNAMEINFO, // todo   13
    IOCTL_SO_UNK14,        // todo
    IOCTL_SO_INETATON,     // todo
    IOCTL_SO_INETPTON,     // todo
    IOCTL_SO_INETNTOP,     // todo
    IOCTLV_SO_GETADDRINFO, // todo
    IOCTL_SO_SOCKATMARK,   // todo
    IOCTLV_SO_UNK1A,       // todo
    IOCTLV_SO_UNK1B,       // todo
    IOCTLV_SO_GETINTERFACEOPT, // todo
    IOCTLV_SO_SETINTERFACEOPT, // todo
    IOCTL_SO_SETINTERFACE,     // todo
    IOCTL_SO_STARTUP,           // 0x1f
    IOCTL_SO_ICMPSOCKET = 0x30, // todo
    IOCTLV_SO_ICMPPING,         // todo
    IOCTL_SO_ICMPCANCEL,        // todo
    IOCTL_SO_ICMPCLOSE          // todo
};

struct address {
    unsigned char len;
    unsigned char family;
    unsigned short port;
    unsigned int name;
    unsigned char unused[20];
};

struct bind_params {
    unsigned int socket;
    unsigned int has_name;
    struct address addr;
};

struct sendto_params {
    unsigned int socket;
    unsigned int flags;
    unsigned int has_destaddr;
    struct address addr;
};

#endif // _SOCKET_DEFINITIONS_H_