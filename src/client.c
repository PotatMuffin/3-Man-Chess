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

#define NORMALLIGHT GetColor(0xdd5959ff)
#define NORMALDARK  GetColor(0xc5444fff)
#define SELECTLIGHT GetColor(0xDDD07CFF)
#define SELECTDARK  GetColor(0xC59E5EFF)

#define MAX_CHARS 16
#define WIDTH 1920
#define HEIGHT 1080

typedef struct {
    Move move;
    Vector2 position;
    Vector2 path[24];
    int     points;
    int     index;
    float   time;
    uint8_t piece;
    bool    isplaying;
} Animation;

typedef struct {
    Rectangle bounds;
    char     *normalText;
    char     *toggleText;
    float     fontSize;
    Color     bgColour;
    Color     fgColour;
    bool      active;
    bool      toggled;
} Button;

static const int rankSize = 60;
static const int pieceSize = 60;
static const int centerSize = 100;
static const int borderSize = rankSize/3;
static const int boardRadius = centerSize+rankSize*6;
static const Vector2 center = {WIDTH/2, HEIGHT/2};

static const Vector2   gameOverBoxSize = { 300, 150 }; 
static const Rectangle gameOverBox = { .x = center.x - gameOverBoxSize.x / 2, .y = center.y - gameOverBoxSize.y / 2, .width = gameOverBoxSize.x, .height = gameOverBoxSize.y };
static const int gameOverButtonPadding = 10;
static const int gameOverButtonHeight = 50;
static const int gameOverButtonWidth = gameOverBox.width / 2 - (float)gameOverButtonPadding * 1.5f;

enum Button {
    BUTTONREMATCH,
    BUTTONEXIT,
    BUTTONRESIGN,
};

// this is probably a bad way to do this
// but there aren't many buttons so I don't care
static Button buttons[] = {
    [BUTTONREMATCH] = { 
        .bounds = {
            .x = gameOverBox.x + gameOverButtonPadding,
            .y = gameOverBox.y + gameOverBox.height - gameOverButtonPadding - gameOverButtonHeight,
            .height = gameOverButtonHeight,
            .width  = gameOverButtonWidth,
        },  
        .normalText = "rematch",
        .toggleText = "X rematch",
        .bgColour = {0x81, 0xB6, 0x4C, 0xFF},
        .fgColour = RL_WHITE,
        .fontSize = 24.0f,
    },
    [BUTTONEXIT] = {
        .bounds = {
            .x = gameOverBox.x + gameOverBox.width - gameOverButtonWidth - gameOverButtonPadding,
            .y = gameOverBox.y + gameOverBox.height - gameOverButtonPadding - gameOverButtonHeight,
            .height = gameOverButtonHeight,
            .width  = gameOverButtonWidth,
        },
        .normalText = "exit",
        .bgColour = {0x81, 0xB6, 0x4C, 0xFF},
        .fgColour = RL_WHITE,
        .fontSize = 24.0f,
    },
};

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
    GAMEOVER,
    CONNECTFAILED,
    CONNECTING,
    GAMEALREADYSTARTED,
};

enum GameState gameState  = NOGAME;
enum EndFlag   endReason  = NONE;
int            gameWinner = -1;
double connectTime = 0.0f;

uint8_t perspective = WhitePerspective;

int selectedSquare = -1;
char highlightedSquares[144];

MoveList moveList = {0};
Move lastMove = {0};

Vector2 SquareCenterCoords[144];

const double animationDuration = 0.1f;
Animation _animation;

Sound moveSound;
Sound castleSound;
Sound promoteSound;
Sound checkSound;
Sound captureSound;

Texture2D spriteSheet;

Font font;

Socket *sock = NULL;

void UpdateGame(Board *board, double deltaTime);
void UpdateTextBox(char *text, int *length, int max);
void UpdateButtons(Board *board);
void HandleInput(Board *board);

Vector2 GetMousePositionScaled();
void InitSquareCenterCoords();
void CreateAnimation(Animation *animation, Board *board, Move move);
int AdjustForPerspective(int index);

void HighlightSquares(int square, MoveList *moveList);
void HighlightSquare(int square);
void ResetSquares();
void DrawClock(Board *board);
void DrawBoard(Board *board);
void DrawEndScreen();
void DrawButton(enum Button buttonIndex);
void UpdateAnimation(Animation *animation, double deltaTime);

int PollConnection();
int HandleServerMessage(Board *board);
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
    spriteSheet = LoadTextureFromImage(pieces);

    font = GetFontDefault();

    char ip[MAX_CHARS+1] = "\0";
    int charCount = 0;

    while(!WindowShouldClose())
    {
        double deltaTime = GetFrameTime();
        UpdateTextBox(&ip[0], &charCount, MAX_CHARS);
        UpdateGame(&board, deltaTime);

        buttons[BUTTONREMATCH].active = gameState == GAMEOVER;
        buttons[BUTTONEXIT].active    = gameState == GAMEOVER;
        buttons[BUTTONRESIGN].active  = gameState == YESGAME;

        UpdateButtons(&board);

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

        DrawBoard(&board);
        UpdateAnimation(&_animation, deltaTime);

        if(gameState == YESGAME)
        {
            DrawClock(&board);
        }
        else if(gameState == GAMEOVER)
        {
            DrawClock(&board);
            DrawEndScreen();
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
            else if(gameState == GAMEALREADYSTARTED)
            {
                DrawText("Game already started", 55, 115, 40, RL_MAROON);
            }

            Rectangle textBox = { .x = 50, .y = 50, .width = 300, .height = 50};
            DrawRectangleRec(textBox, RL_LIGHTGRAY);
            DrawRectangleLinesEx(textBox, 2, RL_WHITE);
            DrawText(ip, textBox.x+5, textBox.y+8, 40, RL_MAROON);
        }
        DrawFPS(0, 0);
        EndTextureMode();

        DrawTexturePro(target.texture, (Rectangle){0, 0, target.texture.width, -target.texture.height}, (Rectangle){0, 0, width, height}, (Vector2){0,0}, 0, RL_WHITE);
        EndDrawing();
    }

    msg.flag = GOODBYE;
    Write(sock, &msg, sizeof(msg));
    Shutdown(sock);
    Close(sock);

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

void UpdateGame(Board *board, double deltaTime)
{
    if(gameState == CONNECTING) 
    {
        int res = PollConnection(sock);
        if(res == 0)
        {
            if(connectTime >= 5.0f) 
            {
                Close(sock);
                sock = NULL;
                gameState = CONNECTFAILED;
                connectTime = 0.0f;
            }
            connectTime += deltaTime;
        }
        else if(res == 1)
        {
            connectTime = 0.0f;
            gameState = YESGAME;
            endReason  = NONE;
            gameWinner = -1;
        }
        else
        {
            Close(sock);
            sock = NULL;
            gameState = CONNECTFAILED;
            connectTime = 0.0f;
        }
        return;
    }

    if(sock == NULL) return;
    int res = HandleServerMessage(board);
    if(res != 0)
    {
        gameState = NOGAME;
        lastMove = (Move){ 0 };
        InitBoard(board, DEFAULT_FEN);
        Close(sock);
        sock = NULL;
    }

    if(gameState == YESGAME)
    {
        UpdateClock(board, deltaTime);
        HandleInput(board);
    }
}

Vector2 GetMousePositionScaled()
{
    Vector2 mousePos = GetMousePosition();
    mousePos.x /= scaleX;
    mousePos.y /= scaleY;
    return mousePos;
}

void HandleInput(Board *board)
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

void UpdateButtons(Board *board)
{
    if(!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
    for(int i = 0; i < NOB_ARRAY_LEN(buttons); i++)
    {
        if(!buttons[i].active) continue;
        Vector2 mousePos = GetMousePositionScaled();
        if(!CheckCollisionPointRec(mousePos, buttons[i].bounds)) continue;

        switch(i)
        {
            case BUTTONREMATCH: {
                buttons[i].toggled = !buttons[i].toggled;
                msg.flag = REMATCH;
                msg.rematch.agree = buttons[i].toggled;
                Write(sock, &msg, sizeof(msg));
            }; break;
            case BUTTONEXIT: {
                gameState = NOGAME;
                InitBoard(board, DEFAULT_FEN);
                msg.flag = GOODBYE;
                Write(sock, &msg, sizeof(msg));
                Shutdown(sock);
                Close(sock);
                sock = NULL;
            }; break;
        }
        break;
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

void DrawPiece(int piece, Vector2 position, float size)
{
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

        DrawTexturePro(spriteSheet, pieceSprite, 
            (Rectangle){position.x, position.y, size, size}, 
            (Vector2){size/2, size/2}, 0, RL_WHITE);
    }
}

void DrawBoard(Board *board)
{
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
                else if (highlightedSquares[index])  colour = NORMALLIGHT;
                else if((index == lastMove.start || index == lastMove.target) && !IsNullMove(lastMove)) colour = SELECTLIGHT;
                else colour = GetColor(0xeed8c0FF);
            }
            else 
            {
                if(index == selectedSquare)         colour = SELECTDARK;
                else if(highlightedSquares[index])  colour = NORMALDARK;
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
        if(_animation.isplaying && square == _animation.move.target) continue;
        char piece = board->map[square];
        int rank = square / 24;
        int file = square % 24;
        float size = pieceSize - (20*(float)rank/5);

        int index = square;
        if(perspective == BlackPerspective)     index = Right(index, 8);
        else if(perspective == GrayPerspective) index = Left(index, 8);

        Vector2 position = SquareCenterCoords[index];
        DrawPiece(piece, position, size);
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
        DrawRectangleRounded(ClockBox, 0.1f, 0, colour);

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

void DrawButton(enum Button buttonIndex)
{
    Button button = buttons[buttonIndex];
    char *text = button.toggled ? button.toggleText : button.normalText;
    DrawRectangleRounded(button.bounds, 0.1f, 0, button.bgColour);
    Vector2 textSize = MeasureTextEx(font, text, button.fontSize, button.fontSize / 10.0f);
    Vector2 textPos = { (button.bounds.width - textSize.x) / 2, (button.bounds.height - textSize.y) / 2 }; 
    DrawText(text, button.bounds.x + textPos.x, button.bounds.y + textPos.y, button.fontSize, button.fgColour);
}

void DrawEndScreen()
{
    DrawRectangleRounded(gameOverBox, 0.1f, 0, GetColor(0x3C3A38FF));

    static const int headerOffset = 20;
    const char *header = (gameWinner == 0) ? "Draw!" : TextFormat("%s won!", GetColourString(gameWinner));
    static const float headerFontSize = 30;
    static const float headerSpacing  = headerFontSize / 10.0f;
    Vector2 headerSize = MeasureTextEx(font, header, headerFontSize, headerSpacing);

    Vector2 headerPos = { (gameOverBoxSize.x - headerSize.x) / 2, headerOffset };
    DrawText(header, gameOverBox.x + headerPos.x, gameOverBox.y + headerPos.y, headerFontSize, RL_WHITE);

    static const int footerOffset = 5;
    const char *footer = TextFormat("by %s", EndFlagString[endReason]);
    static const float footerFontSize = 20;
    static const float footerSpacing = footerFontSize / 10.0f;
    Vector2 footerSize = MeasureTextEx(font, footer, footerFontSize, footerSpacing);

    Vector2 footerPos = { (gameOverBoxSize.x - footerSize.x) / 2, footerOffset + headerOffset + headerSize.y};
    DrawText(footer, gameOverBox.x + footerPos.x, gameOverBox.y + footerPos.y, footerFontSize, RL_WHITE);

    DrawButton(BUTTONREMATCH);
    DrawButton(BUTTONEXIT);
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
    }
}

// returns -1 upon error
//          0 when still awaiting
//          1 upon success 
int PollConnection()
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

int HandleServerMessage(Board *board)
{
    PollFd poll = { .sock = sock, .events = PollRead, .revents = 0 };

    int count = Poll(&poll, 1, 0);
    if(count == 0) return 0;

    int rc = Read(sock, &msg, sizeof(msg));
    if(rc == 0) 
    {
        return 1;
    }

    if(msg.flag == GAMESTART)
    {
        gameState = YESGAME;
        buttons[BUTTONREMATCH].toggled = false;
        printf("Starting game with fen:\n%s\nand colour: %d\n", msg.gameStart.FEN, msg.gameStart.colour);
        perspective = (msg.gameStart.colour >> 3) - 1;
        InitBoard(board, msg.gameStart.FEN);
        InitClock(board, msg.gameStart.timeControl);
    }
    else if(msg.flag == MOVEPLAYED)
    {
        printf("time left on clock: %f\n", msg.movePlayed.clockTime);
        SetClock(board, board->colourToMove, msg.movePlayed.clockTime);
        Move move = msg.playMove.move;
        ResetSquares();
        PlayMoveAudio(board, move);
        CreateAnimation(&_animation, board, move);
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
    else if(msg.flag == ENDOFGAME)
    {
        gameState = GAMEOVER;
        endReason = msg.endOfGame.reason;
        gameWinner = msg.endOfGame.winner;
        lastMove = (Move){ 0 };

        printf("game ended!\nreason: %d\nwinner: %d\n", msg.endOfGame.reason, msg.endOfGame.winner);
    }
    else if(msg.flag == PING)
    {
        Message response = { 0 };
        response.flag = PING;
        response.ping.data = msg.ping.data;
        Write(sock, &response, sizeof(Message));
    }
    else if(msg.flag == GAMEINPROGRESS)
    {
        gameState = GAMEALREADYSTARTED;
    }
    return 0;
}

void CreateAnimation(Animation *animation, Board *board, Move move)
{
    *animation = (Animation) { 0 };
    animation->move = move;
    int piece = board->map[move.start];
    int pieceType = GetPieceType(piece);
    animation->piece = piece;
    animation->position = SquareCenterCoords[AdjustForPerspective(move.start)];
    animation->isplaying = true;

    if(pieceType != ROOK && pieceType != BISHOP && pieceType != QUEEN)
    {
        animation->path[0] = SquareCenterCoords[AdjustForPerspective(move.target)];
        animation->path[1] = (Vector2) { 0 };
        animation->points  = 1;
    }
    else 
    {
        int direction = -1;
        int shortestDistance = 100;
        for(int dir = 0; dir < 8; dir++)
        {
            for(int i = 0; i < 24; i++)
            {
                Move m = moves[move.start][dir][i];

                if(m.target == move.target) 
                {
                    if(shortestDistance > i)
                    {
                        direction = dir;
                        shortestDistance = i;
                    }
                }
                if(board->map[m.target] != NONE) break;
            }
        }

        if(direction == -1)
        {
            fprintf(stderr, "failed to generate animation\n");
            animation->isplaying = false;
            return;
        }

        int i = 0;
        for(; i < 24; i++)
        {
            Move m = moves[move.start][direction][i];
            animation->path[i] = SquareCenterCoords[AdjustForPerspective(m.target)];
            if(m.target == move.target) break;
        }
        animation->points = i+1;
    }
}

void UpdateAnimation(Animation *animation, double deltaTime)
{
    if(!animation->isplaying) return;
    Move move = animation->move;

    int targetRank   = move.target / 24;
    float targetSize = pieceSize - (20*(float)targetRank/5);

    animation->time += deltaTime;
    float lerpAmount = animation->time / animationDuration;
    lerpAmount *= (float)animation->points;
    lerpAmount /= (float)animation->index+1.0f;
    if(lerpAmount > 1.0f) lerpAmount = 1.0f;

    Vector2 position = Vector2Lerp(animation->position, animation->path[animation->index], lerpAmount);
    if(Vector2Equals(position, animation->path[animation->index]))
    {
        animation->position = position;
        animation->index++;
    }

    if(animation->time >= animationDuration) 
    {
        animation->isplaying = false;
        DrawPiece(animation->piece, position, targetSize);
        return;
    }

    int startRank    = move.start  / 24;
    float startSize  = pieceSize - (20*(float)startRank/5);
    float size = Lerp(startSize, targetSize, lerpAmount);

    DrawPiece(animation->piece, position, size);
}

int AdjustForPerspective(int index)
{
    if(perspective == BlackPerspective)     index = Right(index, 8);
    else if(perspective == GrayPerspective) index = Left(index, 8);   
    return index;
}