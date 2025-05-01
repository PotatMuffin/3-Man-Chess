#include <stdio.h>
#include "./common.h"

int LoadFen(Board *board, char *FEN)
{
    int index = 0;
    int section = 0;
    char colours[] = { 'B', 'G', 'W' };

    for(int i = 0; i < 3; i++)
    {
        if(FEN[index] != colours[i]) 
        {
            printf("Expected '%c'\n", colours[i]);
            return 1;
        }
        int rank = 5;
        int file = 0;
        index += 2;
        section = 2-i;

        while(true)
        {
            if(FEN[index] == '\n') 
            {
                index++;
                break;
            }
            if(FEN[index] >= '1' && FEN[index] <= '8')
            {
                int empty = FEN[index++] - '0';
                file += empty;
            }
            else if(FEN[index] == '/')
            {
                index++;
                rank--;
                file = 0;
            }
            else if(FEN[index] == 'W' || FEN[index] == 'G' || FEN[index] == 'B')
            {
                char colour = FEN[index++];
                char piece  = FEN[index++];
                if(piece != 'r' && piece != 'n' && piece != 'b' && piece != 'k' && piece != 'q' && piece != 'p')
                {
                    printf("'%c' is not a valid piece\n", piece);
                    return 1;
                }

                int square = GetIndex(rank, file, section);
                switch(piece)
                {
                    case 'k': { board->map[square] = KING;   break; }
                    case 'p': { board->map[square] = PAWN;   break; }
                    case 'n': { board->map[square] = KNIGHT; break; }
                    case 'b': { board->map[square] = BISHOP; break; }
                    case 'r': { board->map[square] = ROOK;   break; }
                    case 'q': { board->map[square] = QUEEN;  break; }
                }

                switch(colour)
                {
                    case 'W': { board->map[square] |= WHITE; break; }
                    case 'G': { board->map[square] |= GRAY;  break; }
                    case 'B': { board->map[square] |= BLACK; break; }
                }
                file++;
            }
        }
    }

    char colourToMove = FEN[index++];
    if(colourToMove == 'w') board->colourToMove = WHITE;
    else if(colourToMove == 'g') board->colourToMove = GRAY;
    else if(colourToMove == 'b') board->colourToMove = BLACK;
    else {
        printf("colour to move must be 'w', 'g', or 'b'\n");
        return 1;
    }
    index++;

    if(FEN[index] != '-')
    {
        while(FEN[index] != ' ' && FEN[index] != '\x00')
        {
            char c = FEN[index++];
            int castleIndex = -1;
            if(c == 'W') castleIndex = 0;
            else if(c == 'G') castleIndex = 1;
            else if(c == 'B') castleIndex = 2;
            if(castleIndex == -1) return 1;

            c = FEN[index++];
            if(c == 'k') board->castleRights[castleIndex].kingSide = true;
            else if(c == 'q') board->castleRights[castleIndex].queenSide = true;
            else return 1;
        }
        index++;
    }
    else index += 2;

    for(int i = 0; i < 3; i++)
    {
        if(FEN[index] != '-')
        {
            int section;
            if(FEN[index++] == 'W' && FEN[index++] == 'H') section = 0;
            else if(FEN[index++] == 'G' && FEN[index++] == 'R') section = 1;
            else if(FEN[index++] == 'B' && FEN[index++] == 'L') section = 2;
            else return 1;

            if(section != i) return 1;

            int file = 7 - (FEN[index++] - 'a');
            if(file > 7 || file < 0) return 1;
            int rank = FEN[index++] - '0' - 1;
            if(rank > 5 || rank < 0) return 1;

            board->enPassantSquares[i] = rank * 24 + section * 8 + file;
            printf("en passant: { section: %d, rank: %d, file: %d }\n", section, rank, file);
            index++;
        }
        else 
        {
            board->enPassantSquares[i] = -1;
            index += 2;
        }
    }

    return 0;
}