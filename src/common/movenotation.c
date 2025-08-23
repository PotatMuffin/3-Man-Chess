#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "../../nob.h"

static const char sectionNames[] = { 'W', 'G', 'B' };

inline char GetPieceNotation(int pieceType)
{
    switch(pieceType)
    {
        case KNIGHT: return 'N';
        case BISHOP: return 'B';
        case ROOK:   return 'R';
        case QUEEN:  return 'Q';
        case KING:   return 'K';
        default: return 0;
    }
}

void GetMoveNotation(Board *board, Move move, MoveNotations *notations)
{
    if(IsNullMove(move))
    {
        nob_da_append(notations, "xxx");
        return;
    }

    char *moveNotation = malloc(8);
    
    int targetRank    = move.target / 24;
    int targetFile    = move.target % 24;
    int targetSection = targetFile / 8;
    targetFile %= 8; // make the file relative to the section

    int index = 0;
    if(move.flag == CASTLE)
    {
        char *notation = "O-O";
        // queenside
        if(targetFile == 5) notation = "O-O-O";
        strcpy(moveNotation, notation);
        index = strlen(notation);
    }
    else 
    {
        int piece = board->map[move.start];
        int pieceType = GetPieceType(piece);
        bool isCapture = board->map[move.target] != NONE;

        char pieceNotation = GetPieceNotation(pieceType);
        if(pieceNotation != 0) moveNotation[index++] = pieceNotation;
        if(isCapture || move.flag == ENPASSANT)
        {
            moveNotation[index++] = 'x';
        }

        moveNotation[index++] = sectionNames[targetSection];
        moveNotation[index++] = 'a' + 7 - targetFile;
        moveNotation[index++] = '1' + targetRank;
        if(pieceType == PAWNCC)
        {
            switch(move.flag)
            {
                case PROMOTETOKNIGHT: moveNotation[index++] = '='; moveNotation[index++] = GetPieceNotation(KNIGHT); break;
                case PROMOTETOBISHOP: moveNotation[index++] = '='; moveNotation[index++] = GetPieceNotation(BISHOP); break;
                case PROMOTETOROOK:   moveNotation[index++] = '='; moveNotation[index++] = GetPieceNotation(ROOK);   break;
                case PROMOTETOQUEEN:  moveNotation[index++] = '='; moveNotation[index++] = GetPieceNotation(QUEEN);  break;
                default: break;
            }
        }   
    }

    Board _board = *board;
    static MoveList list = { 0 };
    list.count = 0;

    GenerateMoves(&_board, &list);
    if(list.count == 0 && InCheck())
    {
        moveNotation[index++] = '#';
    }
    else if(ChecksEnemy(board, move)) moveNotation[index++] = '+';
    moveNotation[index++] = '\00';
    nob_da_append(notations, moveNotation);
}