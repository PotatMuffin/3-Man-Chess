#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include "./common/common.h"
#include "raylib.h"
#include "raymath.h"
#include "../nob.h"

#define BUNDLE_CONTENT
#include "bundle.h"

#define BACKGROUND      GetColor(0x181818FF)
#define CLOCKBACKGROUND GetColor(0x202020FF)
#define BOARDBORDER     GetColor(0x664a3eFF)

#define SELECTLIGHT GetColor(0xDDD07CFF)
#define SELECTDARK  GetColor(0xC59E5EFF)

#define MAX_CHARS 16
#define WIDTH 1920
#define HEIGHT 1080

const int rankSize = 60;
const int pieceSize = 60;
const int centerSize = 100;
const int borderSize = rankSize/3;
const int boardRadius = centerSize+rankSize*6;
float scaleX = 0;
float scaleY = 0;

Message msg = {0};

enum {
    WhitePerspective = 0,
    BlackPerspective = 1,
    GrayPerspective  = 2,
};

enum GameState {
    NOGAME,
    YESGAME,
    CONNECTFAILED,
    CONNECTING,
};

enum GameState gameState = NOGAME;
double connectTime = 0.0f;

uint8_t perspective = WhitePerspective;

int selectedSquare = -1;
char highlightedSquares[144];

MoveList moveList = {0};
Move lastMove = {0};

Vector2 SquareCenterCoords[144];

Sound moveSound;
Sound castleSound;
Sound promoteSound;
Sound checkSound;
Sound captureSound;

void UpdateGame(Board *board, Socket *sock, double deltaTime);
void UpdateTextBox(char *text, int *length, int max);
void HandleInput(Board *board, Socket *sock);

Vector2 GetMousePositionScaled();
void InitSquareCenterCoords();

void HighlightSquares(int square, MoveList *moveList);
void HighlightSquare(int square);
void ResetSquares();
void DrawClock(Board *board);
void DrawBoard(Board *board, Texture2D spriteSheet);

int PollConnection(Socket *sock);
int HandleServerMessage(Socket *sock, Board *board);
void Send(int fd, Message *msg);

bool LoadAudio();
void PlayMoveAudio(Board *board, Move move);

int main()
{
    if(InitSockets() != 0) return 1;

    Board board = {0};
    int result = InitBoard(&board, DEFAULT_FEN);
    if(result != 0) return 1;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_BORDERLESS_WINDOWED_MODE);
    InitWindow(0, 0, "3 Man Chess");
    ToggleFullscreen();
    SetTargetFPS(60);
    SetWindowMinSize(80, 80);

    InitAudioDevice();
    LoadAudio();

    int width = GetScreenWidth();
    int height = GetScreenHeight();
    RenderTexture2D target = LoadRenderTexture(WIDTH, HEIGHT);
    scaleX = (float)width/WIDTH;
    scaleY = (float)height/HEIGHT;

    InitSquareCenterCoords();
    Image pieces = LoadImageFromMemory(".png", &bundle[0], assets[0].length);
    Texture2D spriteSheet = LoadTextureFromImage(pieces);

    char ip[MAX_CHARS+1] = "\0";
    int charCount = 0;
    Socket *sock = NULL;

    while(!WindowShouldClose())
    {
        double deltaTime = GetFrameTime();
        UpdateTextBox(&ip[0], &charCount, MAX_CHARS);
        UpdateGame(&board, sock, deltaTime);

        if(IsWindowResized())
        {
            width = GetScreenWidth();
            height = GetScreenHeight();
            scaleX = (float)width/WIDTH;
            scaleY = (float)height/HEIGHT;
        }

        if(IsKeyPressed(KEY_F11))
        {
            if(IsWindowFullscreen()) SetWindowSize(width-100, height-100);
            else 
            {
                int monitor = GetCurrentMonitor();
                int width = GetMonitorWidth(monitor);
                int height = GetMonitorHeight(monitor);
                SetWindowSize(width, height);
            }
            ToggleFullscreen();
        }

        if(IsKeyPressed(KEY_ENTER) && charCount > 0)
        {
            if(gameState != CONNECTING && gameState != YESGAME)
            {
                sock = JoinGame(ip, PORT);
                if(sock != NULL) 
                {
                    gameState = CONNECTING;
                }
                else gameState = CONNECTFAILED;
            }
        }

        BeginDrawing();

        BeginTextureMode(target);
        ClearBackground(BACKGROUND);

        if(gameState == YESGAME)
        {
            UpdateClock(&board, deltaTime);
            DrawClock(&board);
        }
        else 
        {
            if(gameState == CONNECTING)
            {
                DrawText("Connecting...", 55, 115, 40, RL_MAROON);

            }
            else if(gameState == CONNECTFAILED)
            {
                DrawText("Connection failed", 55, 115, 40, RL_MAROON);
            }

            Rectangle textBox = { .x = 50, .y = 50, .width = 300, .height = 50};
            DrawRectangleRec(textBox, RL_LIGHTGRAY);
            DrawRectangleLinesEx(textBox, 2, RL_WHITE);
            DrawText(ip, textBox.x+5, textBox.y+8, 40, RL_MAROON);
        }
        DrawFPS(0, 0);
        DrawBoard(&board, spriteSheet);
        EndTextureMode();

        DrawTexturePro(target.texture, (Rectangle){0, 0, target.texture.width, -target.texture.height}, (Rectangle){0, 0, width, height}, (Vector2){0,0}, 0, RL_WHITE);
        EndDrawing();
    }

    CloseAudioDevice();
    CleanupSockets();
    UnloadRenderTexture(target);
    CloseWindow();
    return 0;
}

void UpdateTextBox(char *text, int *length, int max)
{
    int charCount = *length;
    int key = GetCharPressed();

    if(IsKeyPressed(KEY_BACKSPACE) && charCount > 0)
    {
        charCount--;
        text[charCount] = '\0';
    }

    if(IsKeyDown(KEY_LEFT_CONTROL))
    {
        if(IsKeyPressed(KEY_V))
        {
            const char *clipboard = GetClipboardText();  
            if(clipboard != NULL)
            {
                int len = strlen(clipboard);
                memcpy(text, clipboard, max);
                if(len > max) text[max] = '\0';
                charCount = (len >= max) ? max : len;
            } 
        }
        else if(IsKeyPressed(KEY_C))
        {
            SetClipboardText(text);
        }
        else if(IsKeyPressed(KEY_BACKSPACE))
        {
            text[0] = '\0';
            charCount = 0;
        }
    }
    while(key > 0)
    {
        if((key >= '0' && key <= '9' || key == '.') && charCount < max)
        {
            text[charCount++] = (char)key;
            text[charCount] = '\0';
        }
        key = GetCharPressed();
    }
    *length = charCount;
}

void UpdateGame(Board *board, Socket *sock, double deltaTime)
{
    if(gameState == YESGAME)
    {
        int res = HandleServerMessage(sock, board);
        if(res != 0) 
        {
            gameState = NOGAME;
            Close(sock);
            sock = NULL;
            
            InitBoard(board, DEFAULT_FEN);
        }
        HandleInput(board, sock);
    }
    else if(gameState == CONNECTING) 
    {
        int res = PollConnection(sock);
        if(res == 0)
        {
            if(connectTime >= 5.0f) 
            {
                gameState = CONNECTFAILED;
                connectTime = 0.0f;
            }
            connectTime += deltaTime;
        }
        else if(res == 1)
        {
            connectTime = 0.0f;
            gameState = YESGAME;
        }
        else
        {
            gameState = CONNECTFAILED;
            connectTime = 0.0f;
        }
    }
}

Vector2 GetMousePositionScaled()
{
    Vector2 mousePos = GetMousePosition();
    mousePos.x /= scaleX;
    mousePos.y /= scaleY;
    return mousePos;
}

void HandleInput(Board *board, Socket *sock)
{
    if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        Vector2 center = {WIDTH/2, HEIGHT/2};

        Vector2 mousePos = GetMousePositionScaled();

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

        if(perspective == BlackPerspective)     closestSquare = Left(closestSquare, 8);
        else if(perspective == GrayPerspective) closestSquare = Right(closestSquare, 8);

        if (selectedSquare == -1)
        {
            if(board->map[closestSquare] == NONE) return;
            selectedSquare = closestSquare;

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
            Write(sock, &msg, sizeof(msg));
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
    DrawCircle(center.x, center.y, centerSize+rankSize*6+borderSize, BOARDBORDER);

    for(int i = 0; i < 6; i++)
    {
        for(int j = 0; j < 24; j++)
        {
            int index = i*24+j;
            if(perspective == BlackPerspective)     index = Left(index, 8);
            else if(perspective == GrayPerspective) index = Right(index, 8);
            Color colour = {0};
            if((i+j)%2)
            {
                if(index == selectedSquare)          colour = SELECTLIGHT;
                else if (highlightedSquares[index]) colour = GetColor(0xdd5959ff);
                else if((index == lastMove.start || index == lastMove.target) && !IsNullMove(lastMove)) colour = SELECTLIGHT;
                else colour = GetColor(0xeed8c0FF);
            }
            else 
            {
                if(index == selectedSquare)         colour = SELECTDARK;
                else if(highlightedSquares[index]) colour = GetColor(0xc5444fff);
                else if((index == lastMove.start || index == lastMove.target) && !IsNullMove(lastMove)) colour = SELECTDARK;
                else colour = GetColor(0xab7a65FF);
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
        DrawLineEx(startPos, endPosMoat, 8, RL_BLUE);
        DrawLineEx(startPos, endPosCreek, 4, RL_BLUE);
    }
    DrawCircle(center.x, center.y, centerSize, BOARDBORDER);

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
            int file = square % 24;
            float size = pieceSize - (20*(float)rank/5);

            int index = square;
            if(perspective == BlackPerspective)     index = Right(index, 8);
            else if(perspective == GrayPerspective) index = Left(index, 8);

            Vector2 position =  SquareCenterCoords[index];
            DrawTexturePro(spriteSheet, pieceSprite, 
                (Rectangle){position.x, position.y, size, size}, 
                (Vector2){size/2, size/2}, 0, RL_WHITE);
        }
    }
}

void DrawClock(Board *board)
{
    static const Rectangle clockBoxes[] = {
        [0] = { .x = 50,   .y = 900, .width = 300, .height = 100 },
        [1] = { .x = 50,   .y = 50,  .width = 300, .height = 100 },
        [2] = { .x = 1550, .y = 50,  .width = 300, .height = 100 }
    };

    for(int i = 0; i < 3; i++)
    {
        int index = (i + perspective) % 3;
        Color colour = CLOCKBACKGROUND;
        Rectangle ClockBox = clockBoxes[i];
        DrawRectangleRec(ClockBox, colour);

        double time = board->clock.seconds[index];
        int minutes = (int)time/60;
        int seconds = (int)time%60;

        bool flagged = time <= 0.0f;
        const char *clockText = (flagged) ? "0:00" : TextFormat("%d:%02d", minutes, seconds);

        int textSize = MeasureText(clockText, 80);
        int textOffset = ClockBox.width - textSize-10;

        Color textColour = (flagged) ? RL_RED : RL_WHITE;
        DrawText(clockText, ClockBox.x+textOffset, ClockBox.y+15, 80, textColour);
    }
}

void PlayMoveAudio(Board *board, Move move)
{
    if(ChecksEnemy(board, move))             PlaySound(checkSound);
    else if(move.flag == CASTLE)             PlaySound(castleSound);
    else if(move.flag == PROMOTETOBISHOP ||
            move.flag == PROMOTETOKNIGHT ||
            move.flag == PROMOTETOROOK   ||
            move.flag == PROMOTETOQUEEN) PlaySound(promoteSound);
    else if(board->map[move.target] != NONE) PlaySound(captureSound);
    else PlaySound(moveSound);
}

bool LoadAudio()
{
    for(int i = 0; i < NOB_ARRAY_LEN(assets); i++)
    {
        char *name = assets[i].file;

        Sound *Psound;
        if     (strcmp(name, "move.mp3")    == 0) Psound = &moveSound;
        else if(strcmp(name, "check.mp3")   == 0) Psound = &checkSound;
        else if(strcmp(name, "promote.mp3") == 0) Psound = &promoteSound;
        else if(strcmp(name, "castle.mp3")  == 0) Psound = &castleSound;
        else if(strcmp(name, "capture.mp3") == 0) Psound = &captureSound;
        else continue;
    
        Wave wave = LoadWaveFromMemory(GetFileExtension(assets[i].file), &bundle[assets[i].offset], assets[i].length);
        *Psound = LoadSoundFromWave(wave);
        printf("loaded %s\n", name);
    }
}

// returns -1 upon error
//          0 when still awaiting
//          1 upon success 
int PollConnection(Socket *sock)
{
    PollFd poll = { .sock = sock, .events = PollWrite, .revents = 0 };

    int count = Poll(&poll, 1, 0);
    if(count == 0) return 0;

    if(!IsValidConnection(sock))
    {
        return -1;
    }

    return 1;
}

int HandleServerMessage(Socket *sock, Board *board)
{
    PollFd poll = { .sock = sock, .events = PollRead, .revents = 0};

    int count = Poll(&poll, 1, 0);
    if(count == 0) return 0;

    int rc = Read(sock, &msg, sizeof(msg));
    if(rc == 0) 
    {
        return 1;
    }

    if(msg.flag == GAMESTART)
    {
        printf("Starting game with fen:\n%s\nand colour: %d\n", msg.gameStart.FEN, msg.gameStart.colour);
        perspective = (msg.gameStart.colour >> 3) - 1;
        InitBoard(board, msg.gameStart.FEN);
        InitClock(board, msg.gameStart.TimeControl);
    }
    else if(msg.flag == MOVEPLAYED)
    {
        printf("time left on clock: %f\n", msg.movePlayed.clockTime);
        SetClock(board, board->colourToMove, msg.movePlayed.clockTime);
        Move move = msg.playMove.move;
        ResetSquares();
        PlayMoveAudio(board, move);
        MakeMove(board, move);
        lastMove = move;
    }
    else if(msg.flag == ELIMINATED)
    {
        uint8_t colour = msg.eliminated.colour;
        SetClock(board, colour, msg.eliminated.clockTime);
        EliminateColour(board, colour);
        if(board->colourToMove == colour) NextMove(board);
    }
    return 0;
}