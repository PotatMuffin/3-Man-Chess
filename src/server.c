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

#define PLAYERS 3

typedef struct {
    Board board;
    Socket *serverSock;
    Socket *clients[3];
    int colour[3];
    int playerCount;
    int eliminatedPlayerCount;
} Server;

typedef enum {
    NOGAME,
    YESGAME,
    AWAITINGREMATCH,
} GameState;

const double target = 1.0/60.0;;
static double timer = 0.0f;

PollFd polls[PLAYERS+1] = {0};
bool pingResponses[PLAYERS];
uint32_t pingData = 0;

GameState gameState = NOGAME;
MoveList legalMoves = {0};
bool movePlayed = false;
bool playerDisconnected = false;

void Wait(double t);
double GetTime();
int InitServer(Server *server);
int AwaitPlayers(Server *server);
void CloseServer(Server *server);
void HandlePlayerMessage(Server *server, int *disconnectedIndices, int *PdisconnectedCount);

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

// this exists for the windows version
// with the windows socket api you can't detect a disconnect the same way as on linux
void Ping(Server *server, int *disconnectedIndices, int *PdisconnectedCount)
{
    static double lastTime = 0.0f;
    static int pings = 0;
    double diff = timer - lastTime;

    int disconnectedCount = *PdisconnectedCount;
    if(diff >= 1.0f)
    {
        lastTime = timer;
        for(int i = 0; i < server->playerCount; i++)
        {
            if(pings == 0) break;
            if(!pingResponses[i])
            {
                disconnectedIndices[disconnectedCount++] = i + 1;
            }
            pingResponses[i] = false;
        }

        pingData = rand();
        Message msg = { 0 };
        msg.flag = PING;
        msg.ping.data = pingData;
        Broadcast(server, &msg);
        pings++;
    }
    *PdisconnectedCount = disconnectedCount;
}

void EliminatePlayer(Server *server, int playerIndex)
{
    Message msg = {0};
    uint8_t colour = server->colour[playerIndex];
    int colourIndex = colour / 8 - 1;
    EliminateColour(&server->board, colour);
    msg.eliminated.colour = colour;
    msg.eliminated.clockTime = server->board.clock.seconds[colourIndex];
    msg.flag = ELIMINATED;
    Broadcast(server, &msg);
    if(server->board.colourToMove == colour) NextMove(&server->board);
    server->eliminatedPlayerCount++;
}

void HandlePlayerMessage(Server *server, int *disconnectedIndices, int *PdisconnectedCount)
{
    Message response = {0};
    Message message  = {0};

    int disconnectedCount = *PdisconnectedCount;

    int count = Poll(&polls[0], server->playerCount + 1, 0);
    if(count <= 0) return;

    for(int i = 0; i < server->playerCount + 1; i++)
    {
        if((polls[i].revents & PollRead) == 0) continue;
        Socket *sock = polls[i].sock;

        if(i == 0) 
        {
            if(gameState != YESGAME) continue;
            Socket *clientSock = Accept(server->serverSock);
            response.flag = GAMEINPROGRESS;
            Write(clientSock, &response, sizeof(response));
            Close(clientSock);
            continue;
        }

        int playerIndex = i-1;

        uint8_t colour = server->colour[playerIndex]; 
        int colourIndex = colour / 8 - 1;

        int rc = Read(sock, &message, sizeof(message));
        if(rc <= 0)
        {
            disconnectedIndices[disconnectedCount++] = i;
            continue;
        }

        switch(message.flag)
        {
            case PLAYMOVE: {
                if(gameState != YESGAME) break;
                if(colour != server->board.colourToMove) 
                {
                    printf("someone whose turn it isn't tried to play a move\n");
                    printf("colour of player: %d, colour to move: %d, player: %d\n", colour, server->board.colourToMove, playerIndex);
                    return;
                }

                Move playedMove = message.playMove.move;
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
                    response.movePlayed.clockTime = server->board.clock.seconds[colourIndex];
                    Broadcast(server, &response);
                    movePlayed = true;
                }
            }; break;
            case PING: {
                if(message.ping.data == pingData)
                {
                    pingResponses[playerIndex] = true;
                }
            }; break;
        }
    }
    *PdisconnectedCount = disconnectedCount;
}

void HandleDisconnect(Server *server, int *Pindex, int count)
{
    int oldPlayerCount = server->playerCount;

    for(int i = 0; i < count; i++)
    {
        int index = Pindex[i];
        if(index == 0) continue;

        int playerIndex = index - 1;
        Socket *sock = server->clients[playerIndex];
        if(sock == NULL) continue;

        printf("client with fd %d disconnected\n", SocketFd(sock));

        Close(server->clients[playerIndex]);
        server->clients[playerIndex] = NULL;
        server->playerCount--;
        playerDisconnected = true;
    }

    if(oldPlayerCount == server->playerCount) return;

    int playerIndexToReplace = -1;
    int playerIndexToMove    = -1;

    for(int playerIndex = 0; playerIndex < oldPlayerCount; playerIndex++)
    {
        if(server->clients[playerIndex] == NULL && playerIndexToReplace == -1)
        {
            playerIndexToReplace = playerIndex;
        }

        if(server->clients[playerIndex] != NULL && playerIndex > playerIndexToMove)
        {
            playerIndexToMove = playerIndex;
        }
    }

    if(playerIndexToReplace == -1 || playerIndexToMove == -1) return;

    polls[playerIndexToReplace+1].sock    = server->clients[playerIndexToMove];
    server->clients[playerIndexToReplace] = server->clients[playerIndexToMove];
    server->colour[playerIndexToReplace]  = server->colour [playerIndexToMove];
    pingResponses[playerIndexToReplace]   = pingResponses  [playerIndexToMove];
}

int InitServer(Server *server)
{
    memset(server, 0, sizeof(*server));
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

// returns -1 on error
// returns  0 when still awaiting players
// returns  1 when enough players have joined 
int AwaitPlayers(Server *server)
{
    Message msg = {0};
    if(!Listen(server->serverSock, 3)) return -1;
    polls[0] = (PollFd) { .sock = server->serverSock, .events = PollRead | PollUrgent, .revents = 0 };

    int disconnectedIndices[3] = { 0 };
    int disconnectedCount = 0;

    HandlePlayerMessage(server, &disconnectedIndices[0], &disconnectedCount);
    Ping(server, &disconnectedIndices[0], &disconnectedCount);
    HandleDisconnect(server, &disconnectedIndices[0], disconnectedCount);

    int count = Poll(&polls[0], 1, 0);
    if(count <= 0) return 0;

    if((polls[0].revents & polls[0].events) != 0)
    {
        pingResponses[server->playerCount] = true;
        Socket *clientSock = Accept(server->serverSock);
        server->clients[server->playerCount++] = clientSock;
        printf("client joined with fd %d\n", SocketFd(clientSock));
        polls[server->playerCount] = (PollFd){ .sock = clientSock, .events = PollRead };
    }

    return server->playerCount == 3;
}

void StartGame(Server *server, char *FEN)
{
    TimeControl timeControl = { .minutes = 10, .increment = 10 };
    Message msg = {0};
    msg.flag = GAMESTART;
    msg.gameStart.timeControl = timeControl;
    strcpy(msg.gameStart.FEN, FEN);

    int availableColours[] = { WHITE, GRAY, BLACK };
    int availableColourCount = 3;

    for(int i = 0; i < 3; i++)
    {
        int index = rand() % availableColourCount;   
        int colour = availableColours[index];

        availableColourCount--;
        availableColours[index] = availableColours[availableColourCount];

        msg.gameStart.colour = colour;
        server->colour[i] = colour;
        Send(server, i, &msg);
    }

    InitBoard(&server->board, FEN);
    InitClock(&server->board, timeControl);

    GenerateMoves(&server->board, &legalMoves);
}

bool UpdateGame(Server *server, double deltaTime, struct EndOfGame *gameEnd)
{
    static int playerIndex = -1;
    int endReason = -1;
    bool isDraw = false;

    if(playerIndex == -1 || movePlayed || playerDisconnected)
    {
        for(int i = 0; i < server->playerCount; i++) 
        {
            if(server->colour[i] == server->board.colourToMove)
            {
                playerIndex = i;
                break;
            }
        }
    }

    if(movePlayed || playerDisconnected)
    {
        GenerateMoves(&server->board, &legalMoves);
        if(legalMoves.count == 0)
        {
            printf("server->board.eliminatedColour = %d\n", server->board.eliminatedColour);
            if(server->eliminatedPlayerCount == 0) 
            {
                EliminatePlayer(server, playerIndex);
                GenerateMoves(&server->board, &legalMoves);
            }
            else 
            {
                if(InCheck()) 
                {
                    EliminatePlayer(server, playerIndex);
                    endReason = CHECKMATE;
                }
                else 
                {
                    endReason = STALEMATE;
                    isDraw = true;
                }
            }
        }
    }

    movePlayed = false;
    playerDisconnected = false;

    UpdateClock(&server->board, deltaTime);
    if(FlaggedClock(&server->board)) 
    {
        EliminatePlayer(server, playerIndex);
        endReason = TIMEOUT;
    }

    int disconnectedIndices[3] = { 0 };
    int disconnectedCount = 0;

    HandlePlayerMessage(server, &disconnectedIndices[0], &disconnectedCount);
    Ping(server, &disconnectedIndices[0], &disconnectedCount);

    if(disconnectedCount != 0)
    {
        for(int i = 0; i < disconnectedCount; i++)
        {
            int playerIndex = disconnectedIndices[i] - 1;
            EliminatePlayer(server, playerIndex);
        }
        HandleDisconnect(server, &disconnectedIndices[0], disconnectedCount);
        endReason = ABANDONMENT;
    }

    if(server->eliminatedPlayerCount > 1 || isDraw)
    {
        gameEnd->reason = endReason;
        return false;
    }

    return true;
}

void CloseServer(Server *server)
{
    for(int i = 0; i < server->playerCount; i++)
    {
        Close(server->clients[i]);
    }
    Shutdown(server->serverSock);
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

    srand(time(NULL));
    InitSockets();
    Server server;

    if(InitServer(&server) != 0) return 1;

    Message message = { 0 };
    struct EndOfGame gameEnd = { 0 };

    double prevFrameStart = 0;
    while(true)
    {
        double start = GetTime();
        double deltaTime = start - prevFrameStart;
        if(deltaTime < 0 || prevFrameStart == 0) deltaTime = 0;
        prevFrameStart = start;
        timer += deltaTime;

        if(gameState == NOGAME)
        {
            int res = AwaitPlayers(&server);
            if(res == -1) 
            {
                printf("%d\n", server.playerCount);
                break;
            }
            if(res == 1) 
            {
                StartGame(&server, DEFAULT_FEN);

                movePlayed = false;
                playerDisconnected = false;

                gameState = YESGAME;
            }
        }
        else if(gameState == YESGAME)
        {
            if(!UpdateGame(&server, deltaTime, &gameEnd)) 
            {
                gameState = NOGAME;

                message.flag = ENDOFGAME;
                message.endOfGame = gameEnd;
                Broadcast(&server, &message);
                break;
            }
        }

        double end = GetTime();
        double diff = end-start;
        Wait(target-diff);
    }

    CloseServer(&server);
    
    CleanupSockets();
    return 0;
}