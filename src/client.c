#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include "./common/common.h"
#include "../dep/raylib/raylib.h"
#include "../dep/raylib/raymath.h"

#define MAX_CHARS 14
#define WIDTH 1920
#define HEIGHT 1080

const int rankSize = 60;
const int pieceSize = 60;
const int centerSize = 100;
const int borderSize = rankSize/3;
const int boardRadius = centerSize+rankSize*6;
float scale = 0;

struct sockaddr_in serverAddr;
Message msg = {0};

// const float rankScreenRatio = 0.055555556f;
// const float centerScreenRatio = 0.092592593f;
// int boardRadius;
// int rankSize;
// int centerSize;
// int pieceSize;
// int borderSize;

int selectedSquare = -1;
char highlightedSquares[144];

MoveList moveList = {0};

Vector2 SquareCenterCoords[144];

void DrawBoard(Board *board, Texture2D spriteSheet);
void InitSquareCenterCoords();
void HandleInput(Board *board, int sockFd);
void HighlightSquares(int square, MoveList *moveList);
void HighlightSquare(int square);
void ResetSquares();
int JoinGame(char *ip, short port);
void HandleServerMessage(int sockFd, Board *board);
void Send(int fd, Message *msg);

int main()
{
    Board board = {0};
    int result = InitBoard(&board, DEFAULT_FEN);
    if(result != 0) return 1;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_BORDERLESS_WINDOWED_MODE);
    InitWindow(0, 0, "3 Man Chess");
    // ToggleFullscreen();
    SetTargetFPS(60);
    SetWindowMinSize(80, 80);

    int width = GetScreenWidth();
    int height = GetScreenHeight();
    RenderTexture2D target = LoadRenderTexture(WIDTH, HEIGHT);
    scale = fminf((float)GetScreenWidth()/WIDTH, (float)GetScreenHeight()/HEIGHT);

    // rankSize = rankScreenRatio*height;
    // centerSize = centerScreenRatio*height;
    // pieceSize = rankSize;
    // borderSize = rankSize/3;
    // boardRadius = centerSize+rankSize*6;

    InitSquareCenterCoords();
    Texture2D spriteSheet = LoadTexture("./sprites/pieces.png");

    char ip[MAX_CHARS+1] = "\0";
    int charCount = 0;
    bool GameInProgress = false;
    int sockFd = -1;
    // int sockFd = JoinGame("127.0.0.1", 42069);
    // if(sockFd < 0) return 1;

    while(!WindowShouldClose())
    {
        BeginDrawing();
        BeginTextureMode(target);
        if(GameInProgress)
        {
            HandleServerMessage(sockFd, &board);
            HandleInput(&board, sockFd);
        }
        else 
        {
            int key = GetCharPressed();

            if(IsKeyPressed(KEY_BACKSPACE) && charCount > 0)
            {
                charCount--;
                ip[charCount] = '\0';
            }
            if(IsKeyPressed(KEY_ENTER) && charCount > 0)
            {
                sockFd = JoinGame(ip, 42069);
                if(sockFd >= 0) GameInProgress = true; 
            }
            while(key > 0)
            {
                if((key >= '0' && key <= '9' || key == '.') && charCount <= MAX_CHARS)
                {
                    ip[charCount++] = (char)key;
                    ip[charCount] = '\0';
                }
                key = GetCharPressed();
            }

            Rectangle textBox = { .x = 50, .y = 50, .width = 300, .height = 50};
            DrawRectangleRec(textBox, LIGHTGRAY);
            DrawRectangleLinesEx(textBox, 2, GetColor(0xFFFFFFFF));
            DrawText(ip, textBox.x+5, textBox.y+8, 40, MAROON);
        }
        ClearBackground(GetColor(0x181818FF));
        DrawFPS(0, 0);
        DrawBoard(&board, spriteSheet);
        EndTextureMode();

        DrawTexturePro(target.texture, (Rectangle){0, 0, target.texture.width, -target.texture.height}, (Rectangle){0, 0, width, height}, (Vector2){0,0}, 0, GetColor(0xFFFFFFFF));
        EndDrawing();
    }

    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}

void HandleInput(Board *board, int sockFd)
{
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 center = {WIDTH/2, HEIGHT/2};

        Vector2 mousePos = GetMousePosition();
        mousePos.x /= scale;
        mousePos.y /= scale;
        float mouseDistance = Vector2Distance(mousePos, center);
        if(mouseDistance > boardRadius || mouseDistance < centerSize) return;

        float shortestDistance = 100000.0f;
        int closestSquare = -1; 
        for(int square = 0; square < 144; square++)
        {
            float distance = Vector2Distance(mousePos, SquareCenterCoords[square]);
            if(distance < shortestDistance)
            {
                shortestDistance = distance;
                closestSquare = square;
            }
        }

        if (selectedSquare == -1)
        {
            if(board->map[closestSquare] == NONE) return;
            selectedSquare = closestSquare;

            moveList.count = 0;
            GenerateMoves(board, &moveList);
            HighlightSquares(selectedSquare, &moveList);
        }
        else 
        {
            ResetSquares();
            int start = selectedSquare;
            int target = closestSquare;
            selectedSquare = -1;
            if(start == target) return;

            Move chosenMove = {0};
            moveList.count = 0;
            GenerateMoves(board, &moveList);

            for(int i = 0; i < moveList.count; i++)
            {
                Move move = moveList.moves[i];
                if(move.start == start && move.target == target) 
                {
                    chosenMove = move;
                    break;
                }
            }

            if(chosenMove.start == 0 && chosenMove.target == 0) return;

            msg.flag = PLAYMOVE;
            msg.playMove.move = chosenMove;
            Send(sockFd, &msg);
            // MakeMove(board, chosenMove);
        }
    }
}

void InitSquareCenterCoords()
{
    Vector2 center = {WIDTH/2, HEIGHT/2};

    for(int section = 0; section < 3; section++)
    {
        for(int rank = 0; rank < 6; rank++)
        {
            for(int file = 0; file < 8; file++)
            {
                int square = GetIndex(rank, file, section);
                int radius = centerSize+(6-rank)*rankSize-rankSize/2;
                double angle = (((double)file+(double)section*8.0f)*15.0f+37.5f);
                angle *= DEG2RAD;
                SquareCenterCoords[square].x = center.x+radius*cosf(angle);
                SquareCenterCoords[square].y = center.y+radius*sinf(angle);
            }
        }
    }
}

inline void HighlightSquares(int square, MoveList *moveList)
{
    for(int i = 0; i < moveList->count; i++)
    {
        Move move = moveList->moves[i];
        if(move.start == square) HighlightSquare(move.target);
    }
}

inline void HighlightSquare(int square)
{
    highlightedSquares[square] = 1;
}

inline void ResetSquares()
{
    for(int i = 0; i < 144; i++)
    {
        highlightedSquares[i] = 0;
    }
}

void DrawBoard(Board *board, Texture2D spriteSheet)
{
    Vector2 center = {WIDTH/2, HEIGHT/2};
    DrawCircle(center.x, center.y, centerSize+rankSize*6+borderSize, GetColor(0x664a3eFF));

    for(int i = 0; i < 6; i++)
    {
        for(int j = 0; j < 24; j++)
        {
            int index = i*24+j;
            Color colour = {0};
            if((i+j)%2)
            {
                if (!highlightedSquares[index]) colour = GetColor(0xeed8c0FF);
                else colour = GetColor(0xdd5959ff);
            }
            else 
            {
                if(!highlightedSquares[index]) colour = GetColor(0xab7a65FF);
                else colour = GetColor(0xc5444fff);
            }

            int radius = centerSize + (6-i) * rankSize;
            int start = j * 15 + 30;
            int end = start + 15;
            DrawCircleSector(center, radius, start, end, 10, colour);
        }
    }

    int startRadius = centerSize+rankSize*6;
    int endRadiusMoat = centerSize+rankSize*5;
    int endRadiusCreek = centerSize+rankSize*3;

    for(int i = 0; i < 3; i++)
    {
        float angle = DEG2RAD*(30+i*15*8);
        Vector2 startPos    = { center.x+startRadius   *cosf(angle), center.y+startRadius   *sinf(angle)};
        Vector2 endPosMoat  = { center.x+endRadiusMoat *cosf(angle), center.y+endRadiusMoat *sinf(angle)};
        Vector2 endPosCreek = { center.x+endRadiusCreek*cosf(angle), center.y+endRadiusCreek*sinf(angle)};
        DrawLineEx(startPos, endPosMoat, 8, BLUE);
        DrawLineEx(startPos, endPosCreek, 4, BLUE);
    }
    DrawCircle(center.x, center.y, centerSize, GetColor(0x664a3eFF));

    for(int square = 0; square < 144; square++)
    {
        char piece = board->map[square];
        if (piece != 0)
        {
            Rectangle pieceSprite = {0, 0, 450.0f, 450.0f};
            char pieceType = GetPieceType(piece);

            switch (pieceType) {
                case KING:
                    break;
                case QUEEN:
                    pieceSprite.x = 450.0f;
                    break;
                case BISHOP:
                    pieceSprite.x = 900.0f;
                    break;
                case KNIGHT:
                    pieceSprite.x = 1350.0f;
                    break;
                case ROOK:
                    pieceSprite.x = 1800.0f;
                    break;
                case PAWN:
                case PAWNCC:
                    pieceSprite.x = 2250.0f;
                    break;
            }

            char pieceColour = GetPieceColour(piece);
            switch (pieceColour)
            {
                case WHITE: { pieceSprite.y = 0.0f;   break; };
                case BLACK: { pieceSprite.y = 450.0f; break; };
                case GRAY:  { pieceSprite.y = 900.0f; break; };
            }

            int rank = square / 24;
            float size = pieceSize - (20*(float)rank/5);

            Vector2 position =  SquareCenterCoords[square];
            DrawTexturePro(spriteSheet, pieceSprite, 
                (Rectangle){position.x, position.y, size, size}, 
                (Vector2){size/2, size/2}, 0, GetColor(0xFFFFFFFF));
        }
    }
}

int JoinGame(char *ip, short port)
{
    int fd = socket(AF_INET, SOCK_STREAM | O_NONBLOCK, 0);

    memset(&serverAddr, 0, sizeof serverAddr);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ip);
    if(serverAddr.sin_addr.s_addr == -1) return -1;

    connect(fd, (struct sockaddr *)&serverAddr, sizeof(serverAddr));

    struct pollfd pollFd = {
        .fd = fd,
        .events = POLLOUT
    };

    struct sockaddr addr = {0};
    int size = sizeof(addr);

    poll(&pollFd, 1, -1);
    if(getpeername(fd, &addr, &size) != 0)
    {
        printf("failed to connect\n");
        return -1;
    }

    return fd;
}

void HandleServerMessage(int sockFd, Board *board)
{
    struct pollfd pollFd = {
        .fd = sockFd,
        .events = POLLIN
    };

    poll(&pollFd, 1, 0);

    if((pollFd.revents & POLLIN) == 0) return;
    int rc = read(sockFd, &msg, sizeof(msg));
    if(rc == 0) 
    {
        close(sockFd);
        return;
    }

    if(msg.flag == GAMESTART)
    {
        printf("Starting game with fen:\n%s\nand colour: %d\n", msg.gameStart.FEN, msg.gameStart.colour);
        InitBoard(board, msg.gameStart.FEN);
    }
    else if(msg.flag == PLAYMOVE)
    {
        Move move = msg.playMove.move;
        ResetSquares();
        MakeMove(board, move);
    }
    else if(msg.flag == ELIMINATED)
    {
        uint8_t colour = msg.eliminated.colour;
        EliminateColour(board, colour);
        if(board->colourToMove == colour) NextMove(board);
    }
    return;
}

void Send(int fd, Message *msg)
{
    int _ = write(fd, msg, sizeof(*msg));
}