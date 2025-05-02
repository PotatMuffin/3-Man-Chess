#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "./common/common.h"

double target = 0;

void Wait(double t);
double GetTime();

#define PLAYERS 3
struct pollfd polls[PLAYERS+1];

MoveList legalMoves = {0};

void Broadcast(Server *server, Message *msg)
{
    for(int i = 0; i < server->players; i++)
    {
        // gcc is annoying and complains when I ignore the return value of write
        int fd = server->clientFd[i];
        if(fd == -1) continue;
        int _ = write(fd, msg, sizeof(Message));
    }
}

void Send(Server *server, int client, Message *msg)
{
    int fd = server->clientFd[client];
    if(fd == -1) return;
    int _ = write(fd, msg, sizeof(Message));
}

void HandleMessage(Server *server, int client, Message *msg)
{
    Message response = {0};
    int colour = server->colour[client];

    if(msg->flag == PLAYMOVE)
    {
        if(colour != server->board.colourToMove) 
        {
            printf("someone whose turn it isn't tried to play a move\n");
            printf("colour of player: %d, colour to move: %d, client: %d\n", colour, server->board.colourToMove, client);
            return;
        }

        Move playedMove = msg->playMove.move;
        bool isLegal = false;
        for(int i = 0; i < legalMoves.count; i++)
        {
            Move move = legalMoves.moves[i];
            if(playedMove.start  != move.start)  continue;
            if(playedMove.target != move.target) continue;
            if(playedMove.flag   != move.flag)   continue;
            isLegal = true;
            break;
        }

        if(isLegal)
        {
            MakeMove(&server->board, playedMove);
        }

        response.flag = PLAYMOVE;
        response.playMove.move = playedMove;
        Broadcast(server, &response);
    }
}

int InitServer(Server *server)
{
    server->serverFd = socket(AF_INET, SOCK_STREAM | O_NONBLOCK, 0);

    printf("created socket with fd: %d\n", server->serverFd);
    if(server->serverFd < 0) 
    {
        printf("failed creating socket\n");
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(42069);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int yes = 1;
    setsockopt(server->serverFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(true));
    if(bind(server->serverFd, (struct sockaddr  *)&addr, sizeof(addr)) < 0) 
    {
        printf("failed binding socket\n");
        return 1;
    }

    return 0;
}

int AwaitPlayers(Server *server)
{
    Message msg = {0};
    polls[0] = (struct pollfd) { .fd = server->serverFd, .events = POLLIN | POLLPRI };
    if(listen(server->serverFd, 3) < 0) 
    {
        printf("failed to listen\n");
        return 1;
    }

    while(server->players != 3)
    {
        int res = poll(&polls[0], PLAYERS+1, -1);
        if(res <= 0) continue;

        for(int i = 0; i <= server->players; i++)
        {
            if((polls[i].revents & polls[i].events) == 0) continue;
            if(i == 0)
            {
                struct sockaddr client = {0};
                int clientSize = 0;

                int clientFd = accept(server->serverFd, &client, &clientSize);
                fcntl(clientFd, F_SETFL, O_NONBLOCK);
                if(clientFd < 0) 
                {
                    printf("couldn't accept connection\n");
                    return 1;
                }
                printf("client connected with fd: %d\n", clientFd);

                server->clientFd[server->players++] = clientFd;
                polls[server->players] = (struct pollfd) { .fd = clientFd, .events = POLLIN };
            }
            else
            {
                int fd = polls[i].fd;
                int rc = read(fd, &msg, sizeof(msg));
                if(rc <= 0)
                {
                    printf("client with fd %d has disconnected\n", fd);
                    close(fd);
                    server->players--;
                    server->clientFd[i] = server->clientFd[server->players];
                    polls[i].fd         = server->clientFd[server->players];
                }
            }
        }
    }
    return 0;
}

void StartGame(Server *server, char *FEN)
{
    Message msg = {0};
    msg.gameStart.flag = GAMESTART;
    strcpy(msg.gameStart.FEN, FEN);

    for(int i = 0; i < 3; i++)
    {
        int colour = (i+1)*8;
        msg.gameStart.colour = colour;
        server->colour[i] = colour;
        Send(server, i, &msg);
    }

    InitBoard(&server->board, FEN);

    GenerateMoves(&server->board, &legalMoves);
    uint8_t movingColour = server->board.colourToMove;
    while(true)
    {
        double start = GetTime();

        if(movingColour != server->board.colourToMove)
        {
            GenerateMoves(&server->board, &legalMoves);
            if(legalMoves.count == 0)
            {
                printf("server->board.eliminatedColour = %d\n", server->board.eliminatedColour);
                if(server->board.eliminatedColour == 0) 
                {
                    EliminateColour(&server->board, server->board.colourToMove);
                    msg.eliminated.colour = server->board.colourToMove;
                    msg.flag = ELIMINATED;
                    Broadcast(server, &msg);
                    NextMove(&server->board);
                    GenerateMoves(&server->board, &legalMoves);
                }
                else if(InCheck()) return;
            }
            movingColour = server->board.colourToMove;
        }

        int res = poll(polls, PLAYERS+1, 0);

        for(int i = 1; res > 0 && i <= server->players; i++)
        {
            int client = i-1;
            Message msg = {0};
            if((polls[i].revents & POLLIN) == 0) continue;
            int fd = polls[i].fd;

            int rc = read(fd, &msg, sizeof(msg));
            if(rc <= 0) 
            {
                printf("client with fd %d disconnected\n", fd);
                close(server->clientFd[client]);

                uint8_t colour = server->colour[client];
                EliminateColour(&server->board, colour);
                msg.eliminated.colour = colour;
                msg.flag = ELIMINATED;
                Broadcast(server, &msg);

                server->players--;
                server->clientFd[client] = server->clientFd[server->players];
                polls[i].fd              = server->clientFd[server->players];
                server->colour[client]   = server->colour[server->players];
                if(server->board.colourToMove == colour) NextMove(&server->board);
                continue;
            }
            HandleMessage(server, client, &msg);
        }

        double end = GetTime();
        double diff = end-start;
        Wait(target-diff);
    }
}

void CloseServer(Server *server)
{
    for(int i = 0; i < server->players; i++)
    {
        shutdown(server->clientFd[i], SHUT_RDWR);
        close(server->clientFd[i]);
    }
    close(server->serverFd);
}

// the functions Wait and GetTime were "inspired" by raylib
void Wait(double t)
{
    if(t <= 0) return;
    struct timespec time = {0};
    time.tv_sec = (time_t)t;
    time.tv_nsec = (t*1000000000ULL - time.tv_sec);
    while(nanosleep(&time, &time) == -1);
}

double GetTime()
{
    struct timespec ts = {0};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double t = ts.tv_sec;
    t += (double)ts.tv_nsec / (double)1000000000.0;
    return t;
}

int main()
{
    target = 1.0/60.0;
    Server server = {0};
    if(InitServer(&server) != 0) return 1;
    if(AwaitPlayers(&server) != 0) return 1;
    StartGame(&server, DEFAULT_FEN);
    CloseServer(&server);
    return 0;
}