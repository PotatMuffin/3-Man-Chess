#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>

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
    uint8_t map[144];
    PieceList piecelists[24];
    CastleRights castleRights[3];
    uint8_t enPassantSquares[3];
    bool bridgedMoats[3];
    uint8_t colourToMove;
    uint8_t eliminatedColour;
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

enum MessageFlag {
    GAMESTART,
    PLAYMOVE,
    ELIMINATED,

};

struct GameStart {
    uint16_t flag;
    uint8_t  colour;
    char FEN[2048];
};

struct PlayMove {
    uint16_t flag;
    Move move;
};

struct Eliminated {
    uint16_t flag;
    uint8_t  colour;
};

typedef union {
    uint16_t flag;
    struct GameStart gameStart;
    struct PlayMove playMove;
    struct Eliminated eliminated;
} Message;

typedef struct {
    Move *moves;
    int count;
    int capacity;
} MoveList;

typedef struct {
    Board board;
    int serverFd;
    int clientFd[3];
    int colour[3];
    int players;
} Server;

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
inline PieceList *GetPieceList(Board *board, uint8_t piece) { if(GetPieceType(piece) == PAWNCC) piece &= 0b11111110; return &board->piecelists[piece-8]; }

void GenerateMoves(Board *board, MoveList *moveList);
bool InCheck();

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

int InitServer(Server *server);
int AwaitPlayers(Server *server);
void CloseServer(Server *server);

#endif