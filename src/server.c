#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include "./common/common.h"

#ifdef _WIN32
    #include <sysinfoapi.h>
    void __stdcall Sleep(unsigned long msTimeout);
    BOOL WINAPI AllocConsole(void);
#endif

const double target = 1.0/60.0;;

void Wait(double t);
double GetTime();

#define PLAYERS 3

PollFd polls[PLAYERS+1] = {0};

typedef struct {
    Board board;
    Socket *serverSock;
    Socket *clients[3];
    int colour[3];
    int playerCount;
} Server;

MoveList legalMoves = {0};

int InitServer(Server *server);
int AwaitPlayers(Server *server);
void CloseServer(Server *server);

void Broadcast(Server *server, Message *msg)
{
    for(int i = 0; i < server->playerCount; i++)
    {
        Socket *sock = server->clients[i];
        if(sock == NULL) continue;
        Write(sock, msg, sizeof(Message));
    }
}

void Send(Server *server, int client, Message *msg)
{
    Socket *sock = server->clients[client];
    if(sock == NULL) return;
    Write(sock, msg, sizeof(Message));
}

void EliminatePlayer(Server *server, uint8_t colour)
{
    Message msg = {0};
    EliminateColour(&server->board, colour);
    int index = (colour>>3)-1;
    msg.eliminated.colour = colour;
    msg.eliminated.clockTime = server->board.clock.seconds[index];
    msg.flag = ELIMINATED;
    Broadcast(server, &msg);
    if(server->board.colourToMove == colour) NextMove(&server->board);
}

void HandleMessage(Server *server, int playerIndex, Message *msg)
{
    Message response = {0};
    int colour = server->colour[playerIndex];
    int index = (colour >> 3)-1;

    if(msg->flag == PLAYMOVE)
    {
        if(colour != server->board.colourToMove) 
        {
            printf("someone whose turn it isn't tried to play a move\n");
            printf("colour of player: %d, colour to move: %d, player: %d\n", colour, server->board.colourToMove, playerIndex);
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
            IncrementClock(&server->board);
            MakeMove(&server->board, playedMove);
            response.flag = MOVEPLAYED;
            response.movePlayed.move = playedMove;
            response.movePlayed.clockTime = server->board.clock.seconds[index];
            Broadcast(server, &response);
        }
    }
}

int InitServer(Server *server)
{
    server->serverSock = CreateSocket();

    if(server->serverSock == NULL) 
    {
        printf("failed creating socket\n");
        return 1;
    }
    printf("created socket with fd: %d\n", SocketFd(server->serverSock));

    if(!Bind(server->serverSock, PORT)) return 1;

    return 0;
}

int AwaitPlayers(Server *server)
{
    Message msg = {0};
    if(!Listen(server->serverSock, 3)) return 1;
    polls[0] = (PollFd) { .sock = server->serverSock, .events = PollRead | PollUrgent, .revents = 0 };

    while(server->playerCount != 3)
    {
        int count = Poll(&polls[0], server->playerCount+1, -1);
        if(count == 0) continue;

        for(int i = 0; i <= server->playerCount; i++)
        {
            if((polls[i].revents & polls[i].events) == 0) continue;
            if(i == 0)
            {
                Socket *clientSock = Accept(server->serverSock);
                server->clients[server->playerCount++] = clientSock;
                printf("client joined with fd %d\n", SocketFd(clientSock));
                polls[server->playerCount] = (PollFd){ .sock = clientSock, .events = PollRead };
            }
            else
            {
                Socket *sock = polls[i].sock;
                int rc = Read(sock, &msg, sizeof(msg));
                if(rc <= 0)
                {
                    printf("client with fd %d has disconnected\n", SocketFd(sock));
                    Close(sock);
                    
                    server->playerCount--;
                    server->clients[i-1] = server->clients[server->playerCount];
                    polls[i].sock        = server->clients[server->playerCount];
                }
            }
        }
    }
    return 0;
}

void StartGame(Server *server, char *FEN)
{
    TimeControl TimeControl = { .minutes = 10, .increment = 10 };
    Message msg = {0};
    msg.gameStart.flag = GAMESTART;
    msg.gameStart.TimeControl = TimeControl;
    strcpy(msg.gameStart.FEN, FEN);

    for(int i = 0; i < 3; i++)
    {
        int colour = (i+1)*8;
        msg.gameStart.colour = colour;
        server->colour[i] = colour;
        Send(server, i, &msg);
    }

    InitBoard(&server->board, FEN);
    InitClock(&server->board, TimeControl);

    GenerateMoves(&server->board, &legalMoves);
    uint8_t movingColour = server->board.colourToMove;

    double prevFrameStart = 0;
    while(true)
    {
        double start = GetTime();
        double deltaTime = start - prevFrameStart;
        if(deltaTime < 0 || prevFrameStart == 0) deltaTime = 0;
        prevFrameStart = start;

        if(movingColour != server->board.colourToMove)
        {
            GenerateMoves(&server->board, &legalMoves);
            if(legalMoves.count == 0)
            {
                printf("server->board.eliminatedColour = %d\n", server->board.eliminatedColour);
                if(server->board.eliminatedColour == 0) 
                {
                    EliminatePlayer(server, server->board.colourToMove);
                    GenerateMoves(&server->board, &legalMoves);
                }
                else if(InCheck()) return;
            }
            movingColour = server->board.colourToMove;
        }

        UpdateClock(&server->board, deltaTime);
        if(FlaggedClock(&server->board)) 
        {
            EliminatePlayer(server, server->board.colourToMove);
            if(server->board.eliminatedColour != 0) return;
        }

        int res = Poll(&polls[0], PLAYERS+1, 0);

        for(int i = 1; res > 0 && i <= server->playerCount; i++)
        {
            int playerIndex = i-1;
            Message msg = {0};
            if((polls[i].revents & PollRead) == 0) continue;
            Socket *sock = polls[i].sock;

            int rc = Read(sock, &msg, sizeof(msg));
            if(rc <= 0) 
            {
                printf("client with fd %d disconnected\n", SocketFd(sock));

                uint8_t colour = server->colour[playerIndex];
                if(colour != server->board.eliminatedColour)
                {
                    EliminatePlayer(server, colour);
                    if(server->board.eliminatedPlayerCount > 1) return;
                }

                Close(server->clients[playerIndex]);
                server->playerCount--;
                server->clients[playerIndex] = server->clients[server->playerCount];
                polls[i].sock                = server->clients[server->playerCount];
                server->colour[playerIndex]  = server->colour [server->playerCount];
                continue;
            }
            HandleMessage(server, playerIndex, &msg);
        }

        double end = GetTime();
        double diff = end-start;
        Wait(target-diff);
    }
}

void CloseServer(Server *server)
{
    for(int i = 0; i < server->playerCount; i++)
    {
        // shutdown(server->clientFd[i], SHUT_RDWR);
        Close(server->clients[i]);
    }
    Close(server->serverSock);
}

// the functions Wait and GetTime were "inspired" by raylib
void Wait(double t)
{
    if(t <= 0) return;
    #if defined(_WIN32)
        Sleep(t * 1000);
    #elif defined(__GNUC__)
        struct timespec time = {0};
        time.tv_sec = (time_t)t;
        time.tv_nsec = (t*1000000000ULL - time.tv_sec);
        while(nanosleep(&time, &time) == -1);
    #endif
}

double GetTime()
{
    #if defined(_WIN32)
        return (double)GetTickCount64() / 1000;
    #elif defined(__GNUC__)
        struct timespec ts = {0};
        clock_gettime(CLOCK_MONOTONIC, &ts);
        double t = ts.tv_sec;
        t += (double)ts.tv_nsec / (double)1000000000.0;
        return t;
    #endif
}

int main()
{
    #if defined(_WIN32)
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
    #endif
    InitSockets();
    Server server = {0};
    if(InitServer(&server) != 0) return 1;
    if(AwaitPlayers(&server) != 0) return 1;
    StartGame(&server, DEFAULT_FEN);
    CloseServer(&server);
    CleanupSockets();
    return 0;
}