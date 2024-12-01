#define UNICODE
#include <algorithm>
#include <chrono>
#include <conio.h>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <Windows.h>

using namespace std::chrono_literals;

#ifdef _MSC_VER
#pragma region Globals
#endif

CONSOLE_SCREEN_BUFFER_INFO sbInfo;

enum eDisplay
{
    SPACE = 0,
    SQUID = 1,
    CRAB = 2,
    OCTOPUS = 3,
    PLAYER = 4,
    BARRIER = 5,
    UFO = 6
};

enum eDirection
{
    RIGHT = 0,
    DOWN = 1,
    LEFT = 2,
    UP = 3
};

struct Invader
{
    eDisplay type;
    int x;
    int y;
};

struct Bullet
{
    eDirection direction;
    int x;
    int y;
};

int screenWidth = 80,
    screenHeight = 30;
wchar_t *screen;

int fieldWidth = 51, fieldHeight = 25;
unsigned char *pField = nullptr;

bool gameOver = false;
int playerX, playerY, playerWidth = 3;

const int totalInvadersPerRow = 11;
std::unordered_map<eDisplay, int> invaderWidths{{eDisplay::SQUID, 2}, {eDisplay::CRAB, 3}, {eDisplay::OCTOPUS, 3}, {eDisplay::UFO, 3}};
std::vector<Invader> invaders;

std::vector<Bullet> bullets;

std::wstring displayValues = L" SCOABU";
std::string keysToCheck = " QAD";

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

void calculateBarriers(std::vector<int> &barrierPositions, int barrierWidth, int totalBarriers, int fixedSpacing)
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

void calculateInvaders(int fieldWidthMiddle, int startY, int endY, eDisplay invaderType, int alignmentOffset = 0)
{
    int invaderWidth = invaderWidths[invaderType];
    int totalWidthOccupied = totalInvadersPerRow * (invaderWidth + alignmentOffset) + (totalInvadersPerRow - 1) * 1;
    int startX = fieldWidthMiddle - totalWidthOccupied / 2 + alignmentOffset;

    for (int y = startY; y < endY; y += 2)
    {
        int x = startX;

        for (int i = 0; i < totalInvadersPerRow; ++i)
        {
            invaders.push_back({invaderType, x, y});
            x += invaderWidth + alignmentOffset + 1;
        }
    }
}

void Setup()
{
    gameOver = false;

    int fieldWidthMiddle = fieldWidth / 2;
    playerX = fieldWidthMiddle;
    playerY = fieldHeight - 1;

    // Populate invaders
    calculateInvaders(fieldWidthMiddle, 2, 4, eDisplay::SQUID, 1);
    calculateInvaders(fieldWidthMiddle, 4, 8, eDisplay::CRAB);
    calculateInvaders(fieldWidthMiddle, 8, 12, eDisplay::OCTOPUS);

    // Create playing field
    pField = new unsigned char[fieldWidth * fieldHeight];

    std::vector<int> barrierPositions;
    int barrierWidth = 5;
    int totalBarriers = 4;
    int fixedSpacing = 5;
    calculateBarriers(barrierPositions, barrierWidth, totalBarriers, fixedSpacing);

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
    return newPlayerX >= 0 &&
           ((newPlayerX + (playerWidth - 1)) < fieldWidth);
}

int GetNextMovingRow(int limit = -1)
{
    auto max_y_it = std::max_element(invaders.cbegin(), invaders.cend(),
                                     [limit](const Invader &a, const Invader &b)
                                     {
                                         if (limit != -1)
                                         {
                                             if (a.y >= limit)
                                                 return true; // If a.y exceeds limit, consider it "less" so it isn't picked.
                                             if (b.y >= limit)
                                                 return false; // If b.y exceeds limit, consider it "less" so it isn't picked.
                                         }
                                         return a.y < b.y;
                                     });

    if (max_y_it != invaders.end() &&
        (limit == -1 ||
         max_y_it->y < limit))
    {
        return max_y_it->y;
    }
    else
    {
        return -1;
    }
}

bool CanInvaderMove(eDirection moveDirection, eDisplay invaderType, int invaderX)
{
    switch (moveDirection)
    {
    case eDirection::RIGHT:
    {
        int widthAdjustment = invaderWidths[invaderType] - 1;
        return invaderX + widthAdjustment < fieldWidth - 1;
    }
    case eDirection::LEFT:
    {
        return invaderX > 0;
    }
    default:
    {
        throw std::invalid_argument("Invalid Direction for Invaders to move in!");
    }
    }
}

bool CheckInvadersAreAtLeftWall()
{
    auto min_x_it = std::min_element(invaders.cbegin(), invaders.cend(),
                                     [](const Invader &a, const Invader &b)
                                     {
                                         return a.x < b.x;
                                     });

    return min_x_it != invaders.cend() &&
           !CanInvaderMove(eDirection::LEFT, min_x_it->type, min_x_it->x);
}

bool CheckInvadersAreAtRightWall()
{
    auto max_x_it = std::max_element(invaders.cbegin(), invaders.cend(),
                                     [](const Invader &a, const Invader &b)
                                     {
                                         return a.x < b.x;
                                     });

    return max_x_it != invaders.cend() &&
           !CanInvaderMove(eDirection::RIGHT, max_x_it->type, max_x_it->x);
}

bool CanBulletMove(int newBulletY)
{
    return newBulletY >= 0 &&
           newBulletY <= fieldHeight;
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
    int drawOffsetX = 2;
    int movingInvaderY = GetNextMovingRow();
    eDirection invadersDirection = eDirection::RIGHT;
    std::unordered_map<char, bool> keyStates;

    // Run Game
    while (!gameOver)
    {
        // Time
        std::this_thread::sleep_for(50ms);

        /* Input START
         */
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
        if (keyStates[' '])
        {
            // Player is 3 wide, so we want it to come from the middle
            bullets.push_back({eDirection::UP, playerX + 1, playerY});
        }

        /* Input END
         */

        /* Logic START
         */

        // Invader movement
        for (auto &enemy : invaders)
        {
            if (invadersDirection == eDirection::DOWN)
            {
                enemy.y += 1;
                continue;
            }

            if (enemy.y != movingInvaderY)
            {
                continue;
            }

            enemy.x += invadersDirection == eDirection::RIGHT
                           ? 1
                           : -1;
        }

        movingInvaderY = GetNextMovingRow(movingInvaderY);
        if (movingInvaderY == -1)
        {
            if (
                ((invadersDirection == eDirection::RIGHT &&
                  CheckInvadersAreAtRightWall()) ||
                 (invadersDirection == eDirection::LEFT &&
                  CheckInvadersAreAtLeftWall())))
            {
                movingInvaderY = -1; // Will allow reset on next pass
                invadersDirection = eDirection::DOWN;
            }
            else
            {
                movingInvaderY = GetNextMovingRow();
            }
        }
        else if (invadersDirection == eDirection::DOWN &&
                 movingInvaderY != -1)
        {
            invadersDirection =
                CheckInvadersAreAtLeftWall() ? eDirection::RIGHT : eDirection::LEFT;
        }

        // Bullet movement
        for (auto it = bullets.begin(); it != bullets.end();)
        {
            int diffY = it->direction == eDirection::UP
                            ? -1
                            : 1;

            if (CanBulletMove(it->y + diffY))
            {
                it->y += diffY;
                ++it;
            }
            else
            {
                it = bullets.erase(it);
            }

            // TODO
            // Check if bullet hit something
            // Remove what need be removed, add an "explosion"
        }

        /* Logic END
         */

        /* Draw START
         */
        for (int x = 0; x < fieldWidth; ++x)
        {
            for (int y = 0; y < fieldHeight; ++y)
            {
                screen[(screenWidth * y) + (x + drawOffsetX)] = displayValues[pField[(fieldWidth * y) + x]];
            }

            // Display line at the bottom of the field
            screen[(screenWidth * fieldHeight) + (x + drawOffsetX)] = L'_';
        }

        // Draw invaders
        for (const auto &enemy : invaders)
        {
            int invaderWidth = invaderWidths[enemy.type];

            for (int w = 0; w < invaderWidth; ++w)
            {
                screen[(screenWidth * enemy.y) + enemy.x + w + drawOffsetX] = displayValues[enemy.type];
            }
        }

        // Draw player
        for (int px = 0; px < playerWidth; ++px)
        {
            int cell = (screenWidth * playerY) + (playerX + px + drawOffsetX);
            screen[cell] = L'A';
        }

        // Draw bullets
        for (const auto &b : bullets)
        {
            int cell = (screenWidth * b.y) + (b.x + drawOffsetX);
            screen[cell] = L'|';
        }

        /* Draw END
         */

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