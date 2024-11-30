#define UNICODE
#include <algorithm>
#include <conio.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <Windows.h>

#ifdef _MSC_VER
#pragma region Globals
#endif

CONSOLE_SCREEN_BUFFER_INFO sbInfo;

int screenWidth = 80, screenHeight = 60;
wchar_t *screen;

int fieldWidth = 50, fieldHeight = 40;
unsigned char *pField = nullptr;

bool gameOver = false;
int playerX, playerY, playerWidth = 3;

std::vector<int> barrierPositions;

std::string keysToCheck = "QAD";

enum eDisplay
{
    SPACE = 0,
    SQUID = 1,
    CRAB = 2,
    OCTOPUS = 3,
    PLAYER = 4,
    BARRIER = 5
};

#ifdef _MSC_VER
#pragma endregion Globals
#endif

#ifdef _MSC_VER
#pragma region Helpers
#endif

void ClearInputBuffer()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(hStdin);
}

void DisableEcho()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode &= ~ENABLE_ECHO_INPUT; // Disable echo input
    SetConsoleMode(hStdin, mode);
}

void EnableEcho()
{
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    mode |= ENABLE_ECHO_INPUT; // Enable echo input
    SetConsoleMode(hStdin, mode);
}

#ifdef _MSC_VER
#pragma endregion Helpers
#endif

void calculateBarriers(int barrierWidth, int totalBarriers, int fixedSpacing)
{
    int totalBarrierWidth = totalBarriers * barrierWidth;
    int totalFixedSpacing = (totalBarriers - 1) * fixedSpacing;
    int totalRemainingSpace = fieldWidth - totalBarrierWidth - totalFixedSpacing;
    int endSpacing = totalRemainingSpace / 2;
    int currentPos = endSpacing;
    for (int i = 0; i < totalBarriers; ++i)
    {
        barrierPositions.push_back(currentPos);
        currentPos += barrierWidth + fixedSpacing;
    }
}

void Setup()
{
    gameOver = false;
    playerX = fieldWidth / 2;
    playerY = fieldHeight - 1;

    // Create playing field
    pField = new unsigned char[fieldWidth * fieldHeight];
    int barrierWidth = 5;
    int totalBarriers = 4;
    int fixedSpacing = 5;
    calculateBarriers(barrierWidth, totalBarriers, fixedSpacing);

    for (int x = 0; x < fieldWidth; ++x)
    {
        for (int y = 0; y < fieldHeight; ++y)
        {
            int cell = (fieldWidth * y) + x;
            bool isBarrier = false;
            bool isBottomRow = fieldHeight - 3 == y;
            bool isTopRow = fieldHeight - 6 == y;
            bool isBarrierRow = (isBottomRow || (fieldHeight - 4 == y) || (fieldHeight - 5 == y) || isTopRow);

            for (int start : barrierPositions)
            {
                int center = start + barrierWidth / 2;

                if (x >= start && x < start + barrierWidth && isBarrierRow)
                {
                    bool isMiddle = (x >= center - 1 && x <= center + 1);
                    bool isMiddleRemoved = isBottomRow && isMiddle;
                    bool isEdgeRemoved = isTopRow && !isMiddle;
                    isBarrier = true && !isMiddleRemoved && !isEdgeRemoved;
                    break;
                }
            }

            if (isBarrier)
            {
                pField[cell] = eDisplay::BARRIER;
            }
            else
            {
                pField[cell] = eDisplay::SPACE;
            }
        }
    }
}

void CheckKeyStates(std::unordered_map<char, bool> &keyStates, const std::string &keys)
{
    for (char key : keys)
    {
        keyStates[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
    }
}

bool CanMovePlayer(int newPlayerX)
{
    return newPlayerX >= 0 && newPlayerX + (playerWidth - 1) < fieldWidth;
}

int main()
{
    DisableEcho();

    // Console size check
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &sbInfo))
    {
        std::wcout << L"Unable to get ScreenBufferInfo" << std::endl;
        return 1;
    }
    screenWidth = sbInfo.dwSize.X;
    screenHeight = sbInfo.dwSize.Y;
    screen = new wchar_t[screenWidth * screenHeight];

    // Setup writing to screen via buffer
    for (int i = 0; i < screenWidth * screenHeight; ++i)
    {
        screen[i] = L' ';
    }

    HANDLE hConsole = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (hConsole == INVALID_HANDLE_VALUE)
    {
        std::wcout << L"Unable to create screen buffer" << std::endl;
        return 1;
    }
    SetConsoleActiveScreenBuffer(hConsole);
    DWORD dwBytesWritten = 0;

    Setup();

    std::unordered_map<char, bool> keyStates;

    // Run Game
    while (!gameOver)
    {
        // Time

        // Input
        CheckKeyStates(keyStates, keysToCheck);
        if (keyStates['A'] && CanMovePlayer(playerX - 1))
        {
            playerX -= 1;
        }
        if (keyStates['D'] && CanMovePlayer(playerX + 1))
        {
            playerX += 1;
        }
        if (keyStates['Q'])
        {
            gameOver = true;
        }

        // Logic

        // Draw
        for (int x = 0; x < fieldWidth; ++x)
        {
            for (int y = 0; y < fieldHeight; ++y)
            {
                screen[(screenWidth * y) + x] = L" SCOAB"[pField[(fieldWidth * y) + x]];
            }

            // Display line at the bottom of the field
            screen[(screenWidth * fieldHeight) + x] = L'_';
        }

        // Draw player
        for (int px = 0; px < playerWidth; ++px)
        {
            int cell = (screenWidth * playerY) + (playerX + px);
            screen[cell] = L'A';
        }

        // Display
        WriteConsoleOutputCharacterW(hConsole, screen, screenWidth * screenHeight, {0, 0}, &dwBytesWritten);
    }

    // Game over, tidy up
    CloseHandle(hConsole);
    ClearInputBuffer();
    EnableEcho();

    // Wrap up the game
    // TODO

    // Wait to close...
    ClearInputBuffer();
    std::cout << '\n'
              << "Press any key to exit..." << std::endl;
    _getch();

    delete[] screen;
    delete[] pField;

    return 0;
}