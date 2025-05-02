#include "./common.h"

int InitBoard(Board *board, char *FEN)
{
    *board = (Board){0};
    int result = LoadFen(board, FEN);
    if(result != 0) return 1;

    for(int i = 0; i < 144; i++)
    {
        char piece = board->map[i];
        if(piece == NONE) continue;

        PieceList *pieceList = GetPieceList(board, piece);
        AddPiece(pieceList, i);
    }
}

void MakeMove(Board *board, Move move)
{
    uint8_t piece = board->map[move.start];
    uint8_t pieceType = GetPieceType(piece);
    uint8_t capturedPiece = board->map[move.target];
    int colourIndex = (board->colourToMove >> 3) - 1;
    board->enPassantSquares[colourIndex] = -1;

    board->map[move.start] = NONE;
    board->map[move.target] = piece;

    PieceList *pieceList = GetPieceList(board, piece);
    MovePiece(pieceList, move.start, move.target);

    if(capturedPiece != NONE)
    {
        PieceList *list = GetPieceList(board, capturedPiece);
        RemovePiece(list, move.target);
    }

    switch(move.flag)
    {
        case CASTLE:
            // king side
            if(move.target == 1 || move.target == 9 || move.target == 17)
            {
                uint8_t rook = board->map[move.target-1];
                board->map[move.target-1] = NONE;
                board->map[move.target+1] = rook;
                PieceList *list = GetPieceList(board, rook);
                MovePiece(list, move.target-1, move.target+1);
            }

            // queen side
            if(move.target == 5 || move.target == 13 || move.target == 21)
            {
                uint8_t rook = board->map[move.target+2];
                board->map[move.target+2] = NONE;
                board->map[move.target-1] = rook;
                PieceList *list = GetPieceList(board, rook);
                MovePiece(list, move.target+2, move.target-1);
            }
            break;
        case PAWNCROSSCENTER:
            board->map[move.target] = PAWNCC | board->colourToMove;
            break;
        case PROMOTETOQUEEN: 
        {
            board->map[move.target] = QUEEN | board->colourToMove;
            PieceList *list = GetPieceList(board, QUEEN | board->colourToMove);
            RemovePiece(pieceList, move.target);
            AddPiece(list, move.target);
            break;
        }
        case PROMOTETOROOK: 
        {
            board->map[move.target] = ROOK | board->colourToMove;
            PieceList *list = GetPieceList(board, ROOK | board->colourToMove);
            RemovePiece(pieceList, move.target);
            AddPiece(list, move.target);
            break;
        }
        case PROMOTETOBISHOP: 
        {
            board->map[move.target] = BISHOP | board->colourToMove;
            PieceList *list = GetPieceList(board, BISHOP | board->colourToMove);
            RemovePiece(pieceList, move.target);
            AddPiece(list, move.target);
            break;
        }
        case PROMOTETOKNIGHT: 
        {
            board->map[move.target] = KNIGHT | board->colourToMove;
            PieceList *list = GetPieceList(board, KNIGHT | board->colourToMove);
            RemovePiece(pieceList, move.target);
            AddPiece(list, move.target);
            break;
        }
        case PAWNTWOFORWARD:
            board->enPassantSquares[colourIndex] = move.target-24;
            break;
        case ENPASSANT:
        {
            int capturedPieceSquare = move.target + 24;
            capturedPiece = board->map[capturedPieceSquare];
            PieceList *list = GetPieceList(board, capturedPiece);
            board->map[capturedPieceSquare] = NONE;
            RemovePiece(list, capturedPieceSquare);
        }
    }

    if(pieceType == KING) 
    {
        board->castleRights[colourIndex].kingSide  = false;
        board->castleRights[colourIndex].queenSide = false;
    }

    if(move.start == 0  || move.target == 0 ) board->castleRights[0].kingSide  = false;
    if(move.start == 7  || move.target == 7 ) board->castleRights[0].queenSide = false;
    if(move.start == 8  || move.target == 8 ) board->castleRights[1].kingSide  = false;
    if(move.start == 15 || move.target == 15) board->castleRights[1].queenSide = false;
    if(move.start == 16 || move.target == 16) board->castleRights[2].kingSide  = false;
    if(move.start == 23 || move.target == 23) board->castleRights[2].queenSide = false;
    
    NextMove(board);
}

void NextMove(Board *board)
{
    if(board->colourToMove == WHITE) 
    { 
        if(board->eliminatedColour == GRAY) board->colourToMove = BLACK;
        else board->colourToMove = GRAY;
    }
    else if(board->colourToMove == GRAY) 
    { 
        if(board->eliminatedColour == BLACK) board->colourToMove = WHITE;
        else board->colourToMove = BLACK;
    }
    else if(board->colourToMove == BLACK) 
    { 
        if(board->eliminatedColour == WHITE) board->colourToMove = GRAY;
        else board->colourToMove = WHITE;
    }
}

void EliminateColour(Board *board, uint8_t colour)
{
    board->eliminatedColour = colour;
    int index = (colour >> 3)-1;
    board->bridgedMoats[index] = true;
    board->bridgedMoats[(index+1)%3] = true;
}