#include "./common.h"
#include <stdlib.h>
#include <stdio.h>

enum {
    NO = 0,
    EA = 1,
    SO = 2,
    WE = 3,
    NW = 4,
    NE = 5,
    SE = 6,
    SW = 7
};

const uint8_t OppositeDir[] = { [NO] = SO, [SO] = NO, [EA] = WE, [WE] = EA, [NW] = SE, [SE] = NW, [NE] = SW, [SW] = NE };

void GenerateKingMoves(Board *board, MoveList *moveList);
void GenerateSlidingMoves(Board *board, MoveList *moveList);
void GeneratePawnMoves(Board *board, MoveList *moveList);
void GenerateKnightMoves(Board *board, MoveList *moveList);
void CalculateAttackData(Board *board);
bool CrossesMoat(Move move, int dir, int distance);
bool KnightCrossesMoat(Move move);
bool CanCrossMoat(Board *board, Move move, int dir, int distance);
bool CrossesCreek(Move move);
bool blocksCheck(Move move);
bool ChecksEnemy(Board *board, Move move);
bool MovingAlongRay(int square, int dir);
bool KnightMovingAlongRay(int square, Move move, int dir);
bool IsEnPassant(Board *board, Move move);

bool dataGenerated = false;
Move moves[144][8][24] = {0};
Move knightMoves[144][8] = {0};
int  squareDirections[144][144] = {0};

static int friendIndex;
static int friendKingSquare;
static bool attackMap[144];
static bool checkBlockMap[144];
static bool checkingPiecesMap[144];
static int checkingPieces;
static int checks;
static bool pinMap[144];
static int pinDirection[144];

inline void AddMove(MoveList *list, Move move)
{
    if(list->count+1 >= list->capacity)
    {
        if(list->capacity == 0) list->capacity = 32;
        else list->capacity *= 2;
        list->moves = realloc(list->moves, list->capacity*sizeof(Move));
        if(list->moves == NULL) fprintf(stderr, "Damn we ran out of ram :(\nconsider downloading some :D\n");
    }
    list->moves[list->count++] = move;
}

void GenerateMoveData()
{
    for(int i = 0; i < 144; i++)
    {
        int rank = i / 24;
        for(int j = 0; j < 24; j++)
        {
            int target = Up(i, j+1);
            if(target == -1) break;
            moves[i][NO][j] = (Move) { .start = i, .target = target, .flag = 0 };
            squareDirections[i][target] = NO;
        }
        for(int j = 0; j < 24; j++)
        {
            int target = Down(i, j+1);
            if(target == -1) break;
            moves[i][SO][j] = (Move) { .start = i, .target = target, .flag = 0 };
            squareDirections[i][target] = SO;
        }
        for(int j = 0; j < 24; j++)
        {
            int target = Left(i, j+1);
            if(target == i) break;
            moves[i][WE][j] = (Move) { .start = i, .target = target, .flag = 0 };
            squareDirections[i][target] = WE;
        }
        for(int j = 0; j < 24; j++)
        {
            int target = Right(i, j+1);
            if(target == i) break;
            moves[i][EA][j] = (Move) { .start = i, .target = target, .flag = 0};
            squareDirections[i][target] = EA;
        }
        for(int j = 0; j < 24; j++)
        {
            int target = UpLeft(i, j+1);
            if(target == -1 || target == i) break;
            moves[i][NW][j] = (Move) { .start = i, .target = target, .flag = 0};
            squareDirections[i][target] = NW;
        }
        for(int j = 0; j < 24; j++)
        {
            int target = UpRight(i, j+1);
            if(target == -1 || target == i) break;
            moves[i][NE][j] = (Move) { .start = i, .target = target, .flag = 0};
            squareDirections[i][target] = NE;
        }
        for(int j = 0; j < 24; j++)
        {
            int target = DownRight(i, j+1);
            if(target == -1 || target == i) break;
            moves[i][SE][j] = (Move) { .start = i, .target = target, .flag = 0};
            squareDirections[i][target] = SE;
        }
        for(int j = 0; j < 24; j++)
        {
            int target = DownLeft(i, j+1);
            if(target == -1 || target == i) break;
            moves[i][SW][j] = (Move) { .start = i, .target = target, .flag = 0};
            squareDirections[i][target] = SW;
        }

        knightMoves[i][0] = (Move) { .start = i, .target = Right(Up(i, 2), 1), .flag = 0 };
        knightMoves[i][1] = (Move) { .start = i, .target = Left (Up(i, 2), 1), .flag = 0 };
        knightMoves[i][2] = (Move) { .start = i, .target = Right(Up(i, 1), 2), .flag = 0 };
        knightMoves[i][3] = (Move) { .start = i, .target = Left (Up(i, 1), 2), .flag = 0 };
        if(rank != 0) knightMoves[i][4] = (Move) { .start = i, .target = Right(Down(i, 1), 2), .flag = 0 };
        if(rank != 0) knightMoves[i][5] = (Move) { .start = i, .target = Left (Down(i, 1), 2), .flag = 0 };
        if(rank  > 1) knightMoves[i][6] = (Move) { .start = i, .target = Right(Down(i, 2), 1), .flag = 0 };
        if(rank  > 1) knightMoves[i][7] = (Move) { .start = i, .target = Left (Down(i, 2), 1), .flag = 0 };
    }
    dataGenerated = true;
}

bool InCheck() { return checks > 0; }

void GenerateMoves(Board *board, MoveList *moveList)
{
    moveList->count = 0;
    if(!dataGenerated) GenerateMoveData();
    for(int i = 0; i < 144; i++) 
    {
        attackMap[i] = false;
        checkBlockMap[i] = false;
        checkingPiecesMap[i] = false;
        pinMap[i] = false;
    }

    PieceList *king = GetPieceList(board, board->colourToMove | KING);
    if(king->count == 0) return;

    friendIndex = (board->colourToMove >> 3) - 1;
    friendKingSquare = king->pieces[0];
    checkingPieces = 0;
    checks = 0;

    CalculateAttackData(board);
    GenerateKingMoves(board, moveList);
    if(checkingPieces > 1) return;

    GeneratePawnMoves(board, moveList);
    GenerateKnightMoves(board, moveList);
    GenerateSlidingMoves(board, moveList);
}

void GenerateKingMoves(Board *board, MoveList *moveList)
{
    PieceList *list = GetPieceList(board, board->colourToMove | KING);
    int square = list->pieces[0];

    for(int dir = 0; dir < 8; dir++)
    {
        Move move = moves[square][dir][0];
        if(IsNullMove(move)) continue;

        int capturedPiece = board->map[move.target];
        if(IsColour(capturedPiece, board->colourToMove)) continue;
        if(attackMap[move.target]) continue;
        if(CrossesMoat(move, dir, 0)) 
        {
            if(!CanCrossMoat(board, move, dir, 0)) continue;
            if(board->map[move.target] != NONE) continue;
        }
        AddMove(moveList, move);
        if(capturedPiece != NONE) continue;

        int targetFile = move.target % 8; // file relative to the section
        if(dir == EA && board->castleRights[friendIndex].kingSide)
        {
            move = moves[square][dir][1];
            if(board->map[move.target] != NONE) continue;
            if(attackMap[move.target]) continue;
            move.flag = CASTLE;
            AddMove(moveList, move);
        }

        if(dir == WE && board->castleRights[friendIndex].queenSide)
        {
            move = moves[square][dir][1];
            if(board->map[move.target] != NONE || board->map[move.target+1] != NONE) continue;
            if(attackMap[move.target]) continue;
            move.flag = CASTLE;
            AddMove(moveList, move);
        }
    }
}

void GeneratePawnMoves(Board *board, MoveList *moveList)
{
    PieceList *pawns = GetPieceList(board, board->colourToMove | PAWN);
    for(int pieceIndex = 0; pieceIndex < pawns->count; pieceIndex++)
    {
        int square = pawns->pieces[pieceIndex];
        int rank = square / 24;
        int file = square % 24;
        int section = file / 8;

        bool crossedCenter = (GetPieceType(board->map[square]) == PAWNCC);
        int dir = (crossedCenter) ? SO : NO;

        Move firstMove = moves[square][dir][0]; 
        if(board->map[firstMove.target] == NONE)
        {
            if(pinMap[square] && !MovingAlongRay(square, dir)) goto skipForward;
            int targetRank = firstMove.target / 24;
            uint8_t flag = NOFLAG;
            if(rank == 5 && targetRank == 5) flag = PAWNCROSSCENTER; 
            if(blocksCheck(firstMove)) 
            {
                if(targetRank == 0)
                {
                    AddMove(moveList, (Move) { .start  = firstMove.start, .target = firstMove.target, .flag = PROMOTETOQUEEN  });
                    AddMove(moveList, (Move) { .start  = firstMove.start, .target = firstMove.target, .flag = PROMOTETOROOK   });
                    AddMove(moveList, (Move) { .start  = firstMove.start, .target = firstMove.target, .flag = PROMOTETOBISHOP });
                    AddMove(moveList, (Move) { .start  = firstMove.start, .target = firstMove.target, .flag = PROMOTETOKNIGHT });
                }
                else AddMove(moveList, (Move) { .start  = firstMove.start, .target = firstMove.target, .flag = flag });
            }

            if (rank == 1 && !crossedCenter)
            {
                Move secondMove = moves[square][dir][1];
                if(board->map[secondMove.target] == NONE)
                {
                    secondMove.flag = PAWNTWOFORWARD;
                    if(blocksCheck(secondMove)) AddMove(moveList, secondMove);
                } 
            }
        }
        skipForward:

        int startDir = (crossedCenter) ? SE : NW;
        int endDir   = (crossedCenter) ? SW : NE;
        for(int dir = startDir; dir <= endDir; dir++)
        {
            if(pinMap[square] && !MovingAlongRay(square, dir)) continue;

            Move move = moves[square][dir][0];
            if(CrossesCreek(move) && !crossedCenter) continue;
            if(CrossesMoat(move, dir, 0))
            {
                if(!CanCrossMoat(board, move, dir, 0)) continue;
                if(board->map[move.target] != NONE) continue;
            }
            if(!blocksCheck(move)) continue;
            int targetRank = move.target / 24;

            int piece = board->map[move.target];
            uint8_t flag = NOFLAG;
            if(rank == 5 && targetRank == 5) flag = PAWNCROSSCENTER; 
            else if(IsEnPassant(board, move)) flag = ENPASSANT;

            if((piece != NONE && !IsColour(piece, board->colourToMove)) || flag == ENPASSANT)
            {
                if(targetRank == 0)
                {
                    AddMove(moveList, (Move) { .start  = move.start, .target = move.target, .flag = PROMOTETOQUEEN  });
                    AddMove(moveList, (Move) { .start  = move.start, .target = move.target, .flag = PROMOTETOROOK   });
                    AddMove(moveList, (Move) { .start  = move.start, .target = move.target, .flag = PROMOTETOBISHOP });
                    AddMove(moveList, (Move) { .start  = move.start, .target = move.target, .flag = PROMOTETOKNIGHT });
                } 
                else AddMove(moveList, (Move) { .start = move.start, .target = move.target, .flag = flag});
            }
        }
    }
}

void GenerateKnightMoves(Board *board, MoveList *moveList)
{
    PieceList *knights = GetPieceList(board, board->colourToMove | KNIGHT);

    for(int pieceIndex = 0; pieceIndex < knights->count; pieceIndex++)
    {
        int square = knights->pieces[pieceIndex];
        for(int i = 0; i < 8; i++)
        {
            Move move = knightMoves[square][i];
            if(IsNullMove(move)) continue;
            if(pinMap[square] && !KnightMovingAlongRay(square, move, i)) continue;

            if(KnightCrossesMoat(move)) 
            {
                if(!CanCrossMoat(board, move, i, 0)) continue;
                if(board->map[move.target] != NONE) continue;
                if(ChecksEnemy(board, move)) continue;
            }
            if(!blocksCheck(move)) continue;

            uint8_t piece = board->map[move.target];
            if(IsColour(piece, board->colourToMove)) continue;
            AddMove(moveList, move);
        }
    }
}

void GenerateRookMoves(Board *board, MoveList *moveList, PieceList *pieceList)
{
    for(int pieceIndex = 0; pieceIndex < pieceList->count; pieceIndex++)
    {
        int square = pieceList->pieces[pieceIndex];
        for(int dir = 0; dir < 4; dir++)
        {
            bool crossesBridgedMoat = false;
            if(pinMap[square] && !MovingAlongRay(square, dir)) continue;

            for(int i = 0; i < 24; i++)
            {
                Move move = moves[square][dir][i];
                if(IsNullMove(move)) break;

                uint8_t piece = board->map[move.target];
                if(IsColour(piece, board->colourToMove)) break;
                if(CrossesMoat(move, dir, i))
                {
                    if(!CanCrossMoat(board, move, dir, i)) break;
                    crossesBridgedMoat = true;
                }   

                if(crossesBridgedMoat)
                {
                    if(piece != NONE) break;
                    if(ChecksEnemy(board, move)) continue;
                }

                if(!blocksCheck(move)) continue;

                AddMove(moveList, move);
                if(piece != NONE) break;
            }   
        }
    }
}

void GenerateBishopMoves(Board *board, MoveList *moveList, PieceList *pieceList)
{
    for(int pieceIndex = 0; pieceIndex < pieceList->count; pieceIndex++)
    {
        int square = pieceList->pieces[pieceIndex];
        for(int dir = 4; dir < 8; dir++)
        {
            bool crossesBridgedMoat = false;
            if(pinMap[square] && !MovingAlongRay(square, dir)) continue;
            for(int i = 0; i < 24; i++)
            {
                Move move = moves[square][dir][i];
                if(IsNullMove(move)) break;

                uint8_t piece = board->map[move.target];
                if(IsColour(piece, board->colourToMove)) break;
                if(CrossesMoat(move, dir, i))
                {
                    if(!CanCrossMoat(board, move, dir, i)) break;
                    crossesBridgedMoat = true;
                }

                if(crossesBridgedMoat)
                {
                    if(piece != NONE) break;
                    if(ChecksEnemy(board, move)) continue;
                }

                if(!blocksCheck(move)) continue;

                AddMove(moveList, move);
                if(piece != NONE) break;
            }   
        }
    }
}

void GenerateSlidingMoves(Board *board, MoveList *moveList)
{
    PieceList *rooks   = GetPieceList(board, board->colourToMove | ROOK);
    PieceList *bishops = GetPieceList(board, board->colourToMove | BISHOP);
    PieceList *queens  = GetPieceList(board, board->colourToMove | QUEEN);

    GenerateRookMoves(board, moveList, rooks);
    GenerateBishopMoves(board, moveList, bishops);

    GenerateRookMoves(board, moveList, queens);
    GenerateBishopMoves(board, moveList, queens);
}

void CalculateAttackData(Board *board)
{
    for(int i = 1; i <= 2; i++)
    {
        int enemyIndex = (friendIndex+i)%3;
        int enemyColour = (enemyIndex+1)<<3;
        if(enemyColour == board->eliminatedColour) continue;

        PieceList *king    = GetPieceList(board, enemyColour | KING);
        if(king->count == 0) continue;

        PieceList *pawns   = GetPieceList(board, enemyColour | PAWN);
        PieceList *knights = GetPieceList(board, enemyColour | KNIGHT);
        PieceList *bishops = GetPieceList(board, enemyColour | BISHOP);
        PieceList *rooks   = GetPieceList(board, enemyColour | ROOK);
        PieceList *queens  = GetPieceList(board, enemyColour | QUEEN);

        for(int pieceIndex = 0; pieceIndex < rooks->count; pieceIndex++)
        {
            int square = rooks->pieces[pieceIndex];
            for(int dir = 0; dir < 4; dir++)
            {
                for(int i = 0; i < 24; i++)
                {
                    Move move = moves[square][dir][i];
                    if(IsNullMove(move)) break;
                    if(CrossesMoat(move, dir, i)) break;
                    attackMap[move.target] = true;
                    uint8_t piece = board->map[move.target];
                    if(piece != NONE && move.target != friendKingSquare) break;
                }   
            }
        }

        for(int pieceIndex = 0; pieceIndex < bishops->count; pieceIndex++)
        {
            int square = bishops->pieces[pieceIndex];
            for(int dir = 4; dir < 8; dir++)
            {
                for(int i = 0; i < 24; i++)
                {
                    Move move = moves[square][dir][i];
                    if(IsNullMove(move)) break;
                    if(CrossesMoat(move, dir, i)) break;
                    attackMap[move.target] = true;
                    uint8_t piece = board->map[move.target];
                    if(piece != NONE && move.target != friendKingSquare) break;
                }   
            }
        }

        for(int pieceIndex = 0; pieceIndex < queens->count; pieceIndex++)
        {
            int square = queens->pieces[pieceIndex];
            for(int dir = 0; dir < 8; dir++)
            {
                for(int i = 0; i < 24; i++)
                {
                    Move move = moves[square][dir][i];
                    if(IsNullMove(move)) break;
                    if(CrossesMoat(move, dir, i)) break;
                    attackMap[move.target] = true;
                    uint8_t piece = board->map[move.target];
                    if(piece != NONE && move.target != friendKingSquare) break;
                }   
            }
        }

        for(int pieceIndex = 0; pieceIndex < knights->count; pieceIndex++)
        {
            int square = knights->pieces[pieceIndex];
            for(int i = 0; i < 8; i++)
            {
                Move move = knightMoves[square][i];
                if(IsNullMove(move)) continue;
                if(KnightCrossesMoat(move)) continue;
                attackMap[move.target] = true;
            }
        }

        for(int pieceIndex = 0; pieceIndex < pawns->count; pieceIndex++)
        {
            int square = pawns->pieces[pieceIndex];
            bool crossedCenter = (IsType(board->map[square], PAWNCC));

            int startDir = (crossedCenter) ? SE : NW;
            int endDir   = (crossedCenter) ? SW : NE;
            for(int dir = startDir; dir <= endDir; dir++)
            {
                Move move = moves[square][dir][0];
                if(CrossesCreek(move) && !crossedCenter) continue;
                if(CrossesMoat(move, dir, 0)) continue;
                attackMap[move.target] = true;
            }
        }

        for(int dir = 0; dir < 8; dir++)
        {
            int square = king->pieces[0];
            Move move = moves[square][dir][0];
            if(IsNullMove(move)) continue;
            if(CrossesMoat(move, dir, 0)) continue;
            attackMap[move.target] = true;
        }

        int startDir = (queens->count == 0 && rooks->count == 0)   ? 4 : 0;
        int endDir   = (queens->count == 0 && bishops->count == 0) ? 4 : 8;

        for(int dir = startDir; dir < endDir; dir++)
        {
            bool ray[144] = {0};
            bool friendAlongRay = false;
            int friendSquare = -1;
            for(int i = 0; i < 24; i++)
            {
                Move move = moves[friendKingSquare][dir][i];
                if(IsNullMove(move)) break;
                if(CrossesMoat(move, dir, i)) break;
                ray[move.target] = true;

                uint8_t piece = board->map[move.target];
                if(piece == NONE) continue;
                if(IsColour(piece, enemyColour))
                {
                    if(!(dir < 4 &&  IsQueenOrRook(piece)) && !(dir >= 4 && IsQueenOrBishop(piece))) break;
                    if(!friendAlongRay)
                    {
                        checks++;
                        if (!checkingPiecesMap[move.target]) checkingPieces++;
                        checkingPiecesMap[move.target] = true;
                        for(int j = 0; j < 144; j++) if(ray[j]) checkBlockMap[j] = true;
                    }
                    else 
                    {
                        for(int j = 0; j < 144; j++) if(ray[j]) pinMap[j] = true;
                        pinDirection[friendSquare] = dir;
                    }
                    break;
                }
                else if(IsColour(piece, board->colourToMove))
                {
                    if(!friendAlongRay)
                    {
                        friendAlongRay = true;
                        friendSquare = move.target;
                    }
                    else break;
                }
            }
        }

        for(int dir = 0; dir < 8; dir++)
        {
            Move move = knightMoves[friendKingSquare][dir];
            if(IsNullMove(move)) continue;
            if(KnightCrossesMoat(move)) continue;
            uint8_t piece = board->map[move.target];
            if(piece == (enemyColour | KNIGHT)) 
            {
                checks++;
                checkingPieces++;
                checkingPiecesMap[move.target] = true;
                checkBlockMap[move.target] = true;
                break;
            }
        }

        for(int dir = NW; dir <= NE; dir++)
        {
            Move move = moves[friendKingSquare][dir][0];
            if(IsNullMove(move)) continue;
            if(CrossesMoat(move, dir, 0)) continue;
            uint8_t piece = board->map[move.target];

            if(piece == (enemyColour | PAWNCC)) 
            {
                checks++;
                checkingPieces++;
                checkingPiecesMap[move.target] = true;
                checkBlockMap[move.target] = true;
                break;
            }
        }

        for(int dir = SE; dir <= SW; dir++)
        {
            Move move = moves[friendKingSquare][dir][0];
            if(IsNullMove(move)) continue;
            if(CrossesCreek(move)) continue;
            if(CrossesMoat(move, dir, 0)) continue;
            uint8_t piece = board->map[move.target];

            if(piece == (enemyColour | PAWN)) 
            {
                checks++;
                checkingPieces++;
                checkingPiecesMap[move.target] = true;
                break;
            }
        }
    }
}

bool CrossesMoat(Move move, int dir, int distance)
{
    int startRank  = move.start  / 24;
    int targetRank = move.target / 24;
    int startFile  = move.start  % 24;
    int targetFile = move.target % 24;
    int startSection  = startFile  / 8;
    int targetSection = targetFile / 8; 
    if(dir == EA || dir == WE)
    {
        int section;
        if(distance != 0)
        {
            Move prevMove = moves[move.start][dir][distance-1];
            int file = prevMove.target % 24;
            section = file / 8;
        }
        else
        {
            section = startSection;
        }
        return targetRank == 0 && section != targetSection;
    }
    else if(dir > 3) // if it is diagonal
    {
        int section;
        if(distance != 0)
        {
            Move prevMove = moves[move.start][dir][distance-1];
            int file = prevMove.target % 24;
            section = file / 8;
        }
        else 
        {
            section = startSection;
        }
        return targetRank == 0 && section != targetSection;
    }
    else return false;
}

bool KnightCrossesMoat(Move move)
{
    int startRank = move.start / 24;
    int startFile = move.start % 24;
    int startSection = startFile / 8;
    int targetRank = move.target / 24;
    int targetFile = move.target % 24;
    int targetSection = targetFile / 8;

    return (startRank == 0 || targetRank == 0) && startSection != targetSection;
}

bool CanCrossMoat(Board *board, Move move, int dir, int distance)
{
    int pieceType = GetPieceType(board->map[move.start]);
    int moat = -1;
    bool canCross = false;

    int startRank = move.start / 24;
    int startFile = move.start % 24;
    int startSection = move.start / 8;
    int targetRank = move.target / 24;
    int targetFile = move.target % 24;
    int targetSection = targetFile / 8;

    if(pieceType != KNIGHT)
    {
        if(distance != 0)
        {
            Move prevMove = moves[move.start][dir][distance-1];
            startFile = prevMove.start % 24; 
            startSection = startFile / 8;
        }
    }

    switch(startSection)
    {
        case 0:
            if(targetSection == 2) moat = 0; 
            else if(targetSection == 1) moat = 1;
            break;
        case 1:
            if(targetSection == 0) moat = 1;
            else if(targetSection == 2) moat = 2;
        case 2:
            if(targetSection == 1) moat = 2;
            else if(targetSection == 0) moat = 0;
    }
    if(moat == -1) return false;
    return board->bridgedMoats[moat];
}

bool CrossesCreek(Move move)
{
    int startRank = move.start / 24;
    int startFile = move.start % 24;
    int startSection = startFile / 8;
    int targetFile = move.target % 24;
    int targetSection = targetFile / 8;
    return startRank < 4 && startSection != targetSection;
}

bool blocksCheck(Move move)
{
    if(checks == 0)      return true;
    else if(checks == 1) return checkBlockMap[move.target];
    else if(checks > 1)   return checkingPiecesMap[move.target];
}

bool ChecksEnemy(Board *board, Move move)
{
    uint8_t pieceType = GetPieceType(board->map[move.start]);

    if(pieceType != KNIGHT)
    {
        int startDir = (IsQueenOrRook(pieceType)) ? 0 : 4;
        int endDir   = (IsQueenOrBishop(pieceType)) ? 8 : 4;

        for(int dir = startDir; dir < endDir; dir++)
        {
            for(int i = 0; i < 24; i++)
            {
                Move m = moves[move.start][dir][i];
                if(CrossesMoat(m, dir, i)) break;
                int piece = board->map[m.target];
                if(GetPieceType(piece) == KING && GetPieceColour(piece) != board->colourToMove) return true;
                if(piece != NONE) break;
            }
        }
    }
    else
    {
        for(int i = 0; i < 8; i++)
        {
            Move m = knightMoves[move.start][i];
            int piece = board->map[m.target];
            if(GetPieceType(piece) == KING && GetPieceColour(piece) != board->colourToMove) return true;
        }
    }

    return false;
}

bool MovingAlongRay(int square, int dir)
{
    int pinDir = pinDirection[square];
    return pinDir == dir || OppositeDir[pinDir] == dir;
}

bool KnightMovingAlongRay(int square, Move move, int dir)
{
    int kingRank = friendKingSquare / 24;
    int distance = (5-kingRank);
    int pinDir = pinDirection[square];
    if(move.target == moves[friendKingSquare][pinDir][distance].target) return true;
    if(distance != 0 && move.target == moves[friendKingSquare][pinDir][distance-1].target) return true;
    return false;
}

bool IsEnPassant(Board *board, Move move)
{
    for(int i = 0; i < 3; i++)
    {
        if(friendIndex == i) continue;
        if(board->enPassantSquares[i] == move.target) return true;
    }
    return false;
}