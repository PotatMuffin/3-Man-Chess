#include <stdlib.h>
#include "common.h"
#include "../../nob.h"

static const char sectionNames[] = { 'W', 'G', 'B' };

char GetPieceNotation(int pieceType)
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
    int index = 0;

    int piece = board->map[move.start];
    int pieceType = GetPieceType(piece);
    bool isCapture = board->map[move.target] != NONE;

    int targetRank    = move.target / 24;
    int targetFile    = move.target % 24;
    int targetSection = targetFile / 8;
    targetFile %= 8; // make the file relative to the section

    char pieceNotation = GetPieceNotation(pieceType);
    if(pieceNotation != 0) moveNotation[index++] = pieceNotation;
    if(isCapture)
    {
        moveNotation[index++] = 'x';
    }

    moveNotation[index++] = sectionNames[targetSection];
    moveNotation[index++] = 'a' + targetFile;
    moveNotation[index++] = '1' + targetRank;
    moveNotation[index++] = '\00';

    nob_da_append(notations, moveNotation);
}