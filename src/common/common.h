#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PORT 42069

#define DEFAULT_FEN "B 8/8/8/8/BpBpBpBpBpBpBpBp/BrBnBbBkBqBbBnBr\n" \
                    "G 8/8/8/8/GpGpGpGpGpGpGpGp/GrGnGbGkGqGbGnGr\n" \
                    "W 8/8/8/8/WpWpWpWpWpWpWpWp/WrWnWbWkWqWbWnWr\n" \
                    "w WkWqGkGqBkBq - - -"

#define NONE   0b00000000
#define KING   0b00000001
#define PAWN   0b00000010
#define PAWNCC 0b00000011
#define KNIGHT 0b00000100
#define BISHOP 0b00000101
#define ROOK   0b00000110
#define QUEEN  0b00000111
#define WHITE  0b00001000
#define GRAY   0b00010000
#define BLACK  0b00011000

#define PIECEMASK  0b00000111
#define COLOURMASK 0b00011000

typedef struct {
    uint8_t map[144];
    uint8_t pieces[16];
    int count; 
} PieceList;

typedef struct {
    bool kingSide;
    bool queenSide;
} CastleRights;

typedef struct {
    double seconds[3];
    int increment;
} Clock;

typedef struct {
    int minutes;
    int increment;
} TimeControl;

typedef struct {
    uint8_t map[144];
    PieceList piecelists[24];
    CastleRights castleRights[3];
    uint8_t enPassantSquares[3];
    bool bridgedMoats[3];
    uint8_t colourToMove;
    uint8_t eliminatedColour;
    Clock clock;
    int fiftyMoveClock;
    int moveCount;
} Board;

typedef struct {
    uint8_t start;
    uint8_t target;
    uint8_t flag;
} Move;

enum MoveFlag {
    NOFLAG,
    CASTLE,
    PAWNCROSSCENTER,
    PROMOTETOQUEEN,
    PROMOTETOROOK,
    PROMOTETOBISHOP,
    PROMOTETOKNIGHT,
    PAWNTWOFORWARD,
    ENPASSANT,
};

#define nullMove ((Move) { 0 })

enum MessageFlag {
    GAMESTART,
    PLAYMOVE,
    MOVEPLAYED,
    ELIMINATED,
    ENDOFGAME,
    PING,
    GAMEINPROGRESS,
    GOODBYE,
    REMATCH,
    RESIGN,
};

enum EndFlag {
    NOTHING,
    TIMEOUT,
    CHECKMATE,
    ABANDONMENT,
    RESIGNATION,
    STALEMATE,
    FIFTYRULE,
    INSUFFMAT,
    AGREEMENT,
    REPETITION,
};

static const char *EndFlagString[] = {
    [NOTHING]     = "nothing?!?!",
    [TIMEOUT]     = "timeout",
    [CHECKMATE]   = "checkmate",
    [ABANDONMENT] = "abandonment",
    [RESIGNATION] = "resignation",
    [STALEMATE]   = "stalemate",
    [FIFTYRULE]   = "fifty move rule",
    [INSUFFMAT]   = "insufficient material",
    [AGREEMENT]   = "agreement",
    [REPETITION]  = "repetition",
};

struct GameStart {
    uint8_t  colour;
    TimeControl timeControl;
    char FEN[256];
};

struct PlayMove {
    Move move;
};

struct MovePlayed {
    Move move;
    double clockTime;
};

struct Eliminated {
    uint8_t  colour;
    double clockTime;
};

struct EndOfGame {
    uint8_t  winner; // 0 if its a draw, else its WHITE, GRAY, or BLACK depending on who won
    uint8_t  reason; // reason the game terminated, ( e.g. checkmate, stalemate, timeout )
};

struct Ping {
    uint32_t data;
};

struct Rematch {
    bool agree;
};

typedef struct {
    uint16_t flag;
    union {
        struct GameStart gameStart;
        struct PlayMove playMove;
        struct MovePlayed movePlayed;
        struct Eliminated eliminated;
        struct EndOfGame endOfGame;
        struct Ping ping;
        struct Rematch rematch;
    };
} Message;

typedef struct {
    Move *moves;
    int count;
    int capacity;
} MoveList;

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} MoveNotations;

typedef struct Socket Socket;

typedef enum {
    #ifdef _WIN32
        PollWrite  = 16,
        PollRead   = 256,
        PollUrgent = 0, // ignored since POLLPRI on windows is not supported
    #elif __GNUC__
        PollRead   = 1,
        PollUrgent = 2,
        PollWrite  = 4,
    #endif
} PollEvent;

typedef struct {
    Socket *sock;
    uint32_t events;
    uint32_t revents;
} PollFd;

extern Move moves[144][8][24];

inline uint8_t GetPieceType(uint8_t piece) { return piece & PIECEMASK; }
inline uint8_t GetPieceColour(uint8_t piece) { return piece & COLOURMASK; }
inline bool IsColour(uint8_t piece, uint8_t colour) { return (piece & COLOURMASK) == colour; }
inline bool IsType(uint8_t piece, uint8_t type) { return (piece & PIECEMASK) == type; }
inline bool IsQueenOrRook(uint8_t piece) { return ((piece & PIECEMASK) & ROOK) == ROOK; }
inline bool IsQueenOrBishop(uint8_t piece) { return ((piece & PIECEMASK) & BISHOP) == BISHOP; }

int InitBoard(Board *board, char *FEN);
int LoadFen(Board *board, char *FEN);
void MakeMove(Board *board, Move move);
void NextMove(Board *board);
void EliminateColour(Board *board, uint8_t colour);

inline void SetClock(Board *board, uint8_t colour, double time)
{
    int index = (colour>>3)-1;
    board->clock.seconds[index] = time;
}
inline void InitClock(Board *board, TimeControl control)
{
    for(int i = 0; i < 3; i++) board->clock.seconds[i] = control.minutes * 60;
    board->clock.increment = control.increment;
}
inline void UpdateClock(Board *board, double deltaTime)
{
    int index = (board->colourToMove>>3)-1;
    board->clock.seconds[index] -= deltaTime;
}

inline void IncrementClock(Board *board)
{
    int index = (board->colourToMove>>3)-1;
    board->clock.seconds[index] += board->clock.increment;
}

inline bool FlaggedClock(Board *board)
{
    int index = (board->colourToMove>>3)-1;
    return board->clock.seconds[index] <= 0.0f;
}

inline PieceList *GetPieceList(Board *board, uint8_t piece) { if(GetPieceType(piece) == PAWNCC) piece &= 0b11111110; return &board->piecelists[piece-8]; }

#define mod(x, y) ((x%y+y)%y)
inline int Up(int square, int distance)
{
    int rank = square / 24;
    int file = square % 24;
    rank += distance;
    
    if(rank >= 6)
    {
        file += 12;
        file = mod(file, 24);
        rank = 5 - (rank-6);
    }
    if(rank < 0) return -1;
    return rank*24+file;
}

inline int Down(int square, int distance)
{
    return Up(square, -distance);
}

inline int Left(int square, int distance)
{
    int rank = square / 24;
    int file = square % 24;

    file += distance;
    file = mod(file, 24);
    return rank*24+file;
}

inline int Right(int square, int distance)
{
    return Left(square, 24-distance);
}

inline int UpLeft(int square, int distance)
{
    int rank = square / 24;
    int file = square % 24;

    rank += distance;
    file += distance;
    file = mod(file, 24);
    if(rank >= 6)
    {
        file += 13;
        file = mod(file, 24);
        rank = 5 - (rank-6);
    }

    if(rank < 0) return -1;
    return rank*24+file;
}

inline int UpRight(int square, int distance)
{
    int rank = square / 24;
    int file = square % 24;

    rank += distance;
    file += (24 - distance);
    file = mod(file, 24);
    if(rank >= 6)
    {
        file += 11;
        file = mod(file, 24);
        rank = 5 - (rank-6);
    }

    if(rank < 0) return -1;
    return rank*24+file;
}

inline int DownLeft(int square, int distance)
{
    return UpRight(square, -distance);
}

inline int DownRight(int square, int distance)
{
    return UpLeft(square, -distance);
}

void AddMove(MoveList *list, Move move);
void GenerateMoves(Board *board, MoveList *moveList);
bool InCheck();
bool ChecksEnemy(Board *board, Move move);

int NextColourToPlay(Board *board);
inline int GetIndex(int rank, int file, int section) { return rank*24+file+section*8; }

inline void AddPiece(PieceList *list, uint8_t square)
{
    int index = list->count++;
    list->map[square] = index;
    list->pieces[index] = square;
}

inline void RemovePiece(PieceList *list, uint8_t square)
{
    int lastIndex = list->count-1;
    int otherSquare = list->pieces[lastIndex];
    int index = list->map[square];
    list->pieces[index]    = otherSquare;
    list->map[otherSquare] = index;
    list->count--;
}

inline void MovePiece(PieceList *list, uint8_t start, uint8_t target)
{
    int index = list->map[start];
    list->map[target] = index;
    list->pieces[index] = target;
}

inline bool IsNullMove(Move move)
{
    return move.start == 0 && move.target == 0;
}

inline char *GetColourString(int colour)
{
    switch(colour)
    {
        case WHITE: return "White";
        case GRAY:  return "Gray";
        case BLACK: return "Black";
        default:    return "erm, invalid colour :/"; 
    }
}

void GetMoveNotation(Board *board, Move move, MoveNotations *notations);

int InitSockets();
void CleanupSockets();
Socket *JoinGame(char *ip, short port);
Socket *CreateSocket();
bool Bind(Socket *sock, int port);
bool Listen(Socket *sock, int backlog);
Socket *Accept(Socket *sock);
int Poll(PollFd *Ppolls, int count, int timeout);
int Read(Socket *sock, void *dest, int count);
int Write(Socket *sock, void *src, int count);
bool IsValidConnection(Socket *sock);
int SocketFd(Socket *sock);
int Shutdown(Socket *sock);
void Close(Socket *sock);

#endif