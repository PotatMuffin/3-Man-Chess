#include "common.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>

struct Socket {
    SOCKET fd;
};

Socket *JoinGame(char *ip, short port)
{
    int res;
    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == INVALID_SOCKET) return NULL;

    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip);
    if(serverAddr.sin_addr.s_addr == -1) return NULL;

    res = connect(fd, (SOCKADDR *)&serverAddr, sizeof(serverAddr));

    Socket *sock = malloc(sizeof(Socket));
    sock->fd = fd;
    return sock;
}

Socket *CreateSocket()
{
    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd == INVALID_SOCKET) return NULL;

    u_long yes = 1;
    ioctlsocket(fd, FIONBIO, &yes);

    Socket *sock = malloc(sizeof(Socket));
    sock->fd = fd;
    return sock;
}

bool Bind(Socket *sock, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    char yes = 1;
    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(true));
    if(bind(sock->fd, (SOCKADDR *)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        printf("failed binding socket\n");
        return false;
    }
    return true;
}

bool Listen(Socket *sock, int backlog)
{
    int res = listen(sock->fd, backlog);
    if(res != 0)
    {
        printf("failed to listen");
        return false;
    }
    return true;
}

Socket *Accept(Socket *sock)
{
    SOCKET clientFd = accept(sock->fd, NULL, NULL);
    if(clientFd == INVALID_SOCKET) 
    {
        printf("couldn't accept connection");
        return NULL;
    }

    u_long yes = 1;
    ioctlsocket(clientFd, FIONBIO, &yes);

    Socket *clientSock = malloc(sizeof(Socket));
    clientSock->fd = clientFd;

    return clientSock;
}

int Poll(PollFd *Ppolls, int count, int timeout)
{
    WSAPOLLFD polls[count];

    for(int i = 0; i < count; i++)
    {
        polls[i].fd = Ppolls[i].sock->fd;
        polls[i].events = Ppolls[i].events;
        polls[i].revents = 0;
    }

    int pollCount = WSAPoll(&polls[0], count, timeout);

    for(int i = 0; i < count; i++)
    {
        Ppolls[i].revents = polls[i].revents;
    }

    return pollCount;
}

int Read(Socket *sock, void *dest, int count)
{
    return recv(sock->fd, dest, count, 0);
}

int Write(Socket *sock, void *src, int count)
{
    return send(sock->fd, src, count, 0);
}

bool IsValidConnection(Socket *sock)
{
    struct sockaddr addr;
    int size = sizeof(addr);

    int res = getpeername(sock->fd, &addr, &size);
    if(res != 0) 
    {
        return false;
    }
    return true;
}

int SocketFd(Socket *sock)
{
    return (int)sock->fd;
}

int Shutdown(Socket *sock)
{
    return shutdown(sock->fd, SD_BOTH);
}

void Close(Socket *sock)
{
    closesocket(sock->fd);
}

int InitSockets()
{
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2,2), &wsaData);
    if(res != NO_ERROR) 
    {
        wprintf(L"WSASartup failed with error: %d\n", res);
        return 1;
    }
    return 0;
}

void CleanupSockets()
{
    WSACleanup();
}

#elif __GNUC__
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <errno.h>

struct Socket {
    int fd;
};

int SocketFd(Socket *sock)
{
    return sock->fd;
}

Socket *CreateSocket()
{
    int fd = socket(AF_INET, SOCK_STREAM | O_NONBLOCK, 0);
    if(fd <= 0) return NULL;
    Socket *sock = malloc(sizeof(Socket));
    sock->fd = fd;
    return sock;
}

Socket *JoinGame(char *ip, short port)
{
    int res;
    int fd = socket(AF_INET, SOCK_STREAM | O_NONBLOCK, 0);
    if(fd <= 0) return NULL;
    printf("socket fd = %d\n", fd);

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip);
    if(serverAddr.sin_addr.s_addr == -1) return NULL;

    res = connect(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if(res != 0) 
    {   
        if(errno != EINPROGRESS)
        {
            perror("failed to connect");
            return NULL;
        }
    }

    struct Socket *sock = malloc(sizeof(Socket));
    sock->fd = fd;
    return sock;
}

int Poll(PollFd *Ppolls, int count, int timeout)
{
    struct pollfd polls[count];

    for(int i = 0; i < count; i++)
    {
        polls[i].fd = Ppolls[i].sock->fd;
        polls[i].events = Ppolls[i].events; 
        polls[i].revents = 0;
    }

    int pollCount = poll(&polls[0], count, timeout);

    for(int i = 0; i < count; i++)
    {
        Ppolls[i].revents = polls[i].revents;
    }

    return pollCount;
}

int Read(Socket *sock, void *dest, int count)
{
    return read(sock->fd, dest, count);
}

int Write(Socket *sock, void *src, int count)
{
    return write(sock->fd, src, count);
}

bool Bind(Socket *sock, int port)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int yes = 1;
    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(true));
    if(bind(sock->fd, (struct sockaddr  *)&addr, sizeof(addr)) < 0) 
    {
        perror("failed binding socket\n");
        return false;
    }
    return true;
}

Socket *Accept(Socket *sock)
{
    struct sockaddr client = {0};
    int clientSize = 0;

    int clientFd = accept(sock->fd, &client, &clientSize);
    if(clientFd < 0) 
    {
        perror("couldn't accept connection");
        return NULL;
    }

    fcntl(clientFd, F_SETFL, O_NONBLOCK);

    Socket *clientSock = malloc(sizeof(Socket));
    clientSock->fd = clientFd;

    return clientSock;
}

bool Listen(Socket *sock, int backlog)
{
    int res = listen(sock->fd, backlog); 
    if(res != 0)
    {
        perror("failed to listen");
        return false;
    }
    return true;
}

bool IsValidConnection(Socket *sock)
{
    struct sockaddr addr;
    int size = sizeof(addr);

    int res = getpeername(sock->fd, &addr, &size);
    if(res != 0) 
    {
        perror("failed to connect");
        return false;
    }
    return true;
}

int Shutdown(Socket *sock)
{
    return shutdown(sock->fd, SHUT_RDWR);
}

void Close(Socket *sock)
{
    close(sock->fd);
    free(sock);
}

int InitSockets()
{
    return 0;
}

void CleanupSockets()
{
    return;
}

#endif