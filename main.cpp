#define UNICODE
#include <algorithm>
#include <chrono>
#include <conio.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <random>
#include <set>
#include <sstream>
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
    int shootingCooldown = 0;
};

struct Bullet
{
    eDirection direction;
    int x;
    int y;
    bool forRemoval = false;
};

struct Explosion
{
    int x;
    int y;
};

int screenWidth = 80,
    screenHeight = 45;
wchar_t *screen;

int fieldWidth = 51, fieldHeight = 25;
unsigned char *pField = nullptr;

bool gameOver = false;
int playerLives = 3;
int playerX, playerY, playerWidth = 3;
int score = 0;

const int totalInvadersPerRow = 11;
std::unordered_map<eDisplay, int> invaderScores{{eDisplay::SQUID, 30}, {eDisplay::CRAB, 20}, {eDisplay::OCTOPUS, 10}, {eDisplay::UFO, 0}};
std::unordered_map<eDisplay, int> invaderWidths{{eDisplay::SQUID, 2}, {eDisplay::CRAB, 3}, {eDisplay::OCTOPUS, 3}, {eDisplay::UFO, 3}};
std::vector<Invader> invaders;

int movingInvaderY;
eDirection invadersDirection = eDirection::RIGHT;

std::vector<Bullet> bullets;
std::vector<Explosion> explosions;

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

bool randomChance(float probability)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> dis(0.0, 1.0);
    return dis(gen) < probability;
}

std::wstring intToPaddedWString(int number, int totalWidth, int zeroPaddingWidth)
{
    std::wostringstream wss;
    wss << std::setw(zeroPaddingWidth) << std::setfill(L'0') << number;
    std::wstring zeroPaddedStr = wss.str();

    int leadingSpaces = totalWidth - zeroPaddedStr.length();
    if (leadingSpaces < 0)
    {
        leadingSpaces = 0; // Ensure non-negative
    }

    // Add leading spaces
    std::wostringstream result;
    result << std::setw(leadingSpaces) << L' ' << zeroPaddedStr;
    return result.str();
}

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

#ifdef _MSC_VER
#pragma region Main Loop Functions
#endif

void Setup(int invaderRowOffset = 0)
{
    gameOver = false;

    int fieldWidthMiddle = fieldWidth / 2;
    playerX = fieldWidthMiddle;
    playerY = fieldHeight - 1;

    // Clear down
    bullets.clear();
    explosions.clear();

    // Populate invaders
    calculateInvaders(fieldWidthMiddle, 2 + invaderRowOffset, 4 + invaderRowOffset, eDisplay::SQUID, 1);
    calculateInvaders(fieldWidthMiddle, 4 + invaderRowOffset, 8 + invaderRowOffset, eDisplay::CRAB);
    calculateInvaders(fieldWidthMiddle, 8 + invaderRowOffset, 12 + invaderRowOffset, eDisplay::OCTOPUS);

    // Invader movement starting
    movingInvaderY = GetNextMovingRow();
    invadersDirection = eDirection::RIGHT;

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

void Draw(int drawOffset)
{
    int actualHeight = fieldHeight + drawOffset;
    for (int x = 0; x < fieldWidth; ++x)
    {
        for (int y = 0; y < fieldHeight; ++y)
        {
            screen[(screenWidth * (y + drawOffset)) + (x + drawOffset)] = displayValues[pField[(fieldWidth * y) + x]];
        }

        // Display line at the bottom of the field
        screen[(screenWidth * actualHeight) + (x + drawOffset)] = L'_';
    }

    // Draw invaders
    for (const auto &enemy : invaders)
    {
        int invaderWidth = invaderWidths[enemy.type];

        for (int w = 0; w < invaderWidth; ++w)
        {
            screen[(screenWidth * (enemy.y + drawOffset)) + enemy.x + w + drawOffset] = displayValues[enemy.type];
        }
    }

    // Draw bullets
    for (const auto &b : bullets)
    {
        int cell = (screenWidth * (b.y + drawOffset)) + (b.x + drawOffset);
        screen[cell] = L'|';
    }

    // Draw explosions
    for (const auto &e : explosions)
    {
        int cell = (screenWidth * (e.y + drawOffset)) + (e.x + drawOffset);
        screen[cell] = L'*';
    }

    // Draw player
    for (int px = 0; px < playerWidth; ++px)
    {
        int cell = (screenWidth * (playerY + drawOffset)) + (playerX + px + drawOffset);
        screen[cell] = L'A';
    }

    // Draw game information
    // Score
    int lineWidth = fieldWidth;
    std::wstring scoreStr = intToPaddedWString(score, 6, 4);
    swprintf_s(&screen[(screenWidth) + (drawOffset)], lineWidth, L"SCORE<1> HI-SCORE SCORE<2>");
    swprintf_s(&screen[(screenWidth * 2) + (drawOffset)], lineWidth, L"%ls", scoreStr.c_str());

    // Lives
    std::wstring playerIcon(playerWidth, displayValues[eDisplay::PLAYER]);
    std::wstring livesDisplay = L" " + std::to_wstring(playerLives);

    for (int i = 1; i < playerLives; ++i)
    {
        livesDisplay += L" " + playerIcon;
    }

    std::wstring rightAlignedText = L"CREDIT 00";
    int textLength = livesDisplay.length() + rightAlignedText.length();
    int spacesNeeded = fieldWidth - textLength - 2;
    std::wstring finalLine = livesDisplay + std::wstring(spacesNeeded, L' ') + rightAlignedText;

    wmemset(&screen[(screenWidth * (actualHeight + 2)) + (drawOffset)], L' ', lineWidth);
    swprintf_s(&screen[(screenWidth * (actualHeight + 2)) + (drawOffset)], lineWidth, L"%ls", finalLine.c_str());
}

#ifdef _MSC_VER
#pragma endregion Main Loop Functions
#endif

int main()
{
    DisableEcho();

    // std::ofstream logFile("invader_debug.log");

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
    int level = 0;
    int drawOffset = 2;
    int actualHeight = fieldHeight + drawOffset;
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
            // TODO throttle how often player can shoot
            // Player is 3 wide, so we want it to come from the middle
            bullets.push_back({eDirection::UP, playerX + 1, playerY});
        }

        /* Input END
         */

        /* Logic START
         */

        // Clear down explosions
        explosions.clear();

        // Invader movement
        for (auto &enemy : invaders)
        {
            if (invadersDirection == eDirection::DOWN)
            {
                enemy.y += 1;

                // Have Invaders reached the bottom?
                if (enemy.y == playerY - 1)
                {
                    gameOver = true;
                }

                // Clear square (only has an effect if there is a barrier there)
                pField[(fieldWidth * enemy.y) + enemy.x] = eDisplay::SPACE;
                continue;
            }

            if (enemy.y != movingInvaderY)
            {
                continue;
            }

            enemy.x += invadersDirection == eDirection::RIGHT
                           ? 1
                           : -1;

            // Clear square (only has an effect if there is a barrier there)
            pField[(fieldWidth * enemy.y) + enemy.x] = eDisplay::SPACE;
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

        // Invaders shoot?
        int invaderCount = invaders.size();
        int shootingChance = std::max(1, 100 / invaderCount);
        std::map<int, Invader *> bottomMostInvaders;
        std::set<Invader *> processedInvaders;

        for (auto &invader : invaders)
        {
            bool isBottom = true;
            int invaderX = invader.x;
            if (invader.type == eDisplay::SQUID)
            {
                invaderX = invader.x - 1;
            }

            for (int dx = 0; dx < invaderWidths[invader.type]; ++dx)
            {
                int invaderColumn = invaderX + dx;

                if (bottomMostInvaders.find(invaderColumn) != bottomMostInvaders.end() &&
                    invader.y < bottomMostInvaders[invaderColumn]->y)
                {
                    isBottom = false;
                    break;
                }
            }

            if (isBottom)
            {
                for (int dx = 0; dx < invaderWidths[invader.type]; ++dx)
                {
                    int invaderColumn = invaderX + dx;
                    if (bottomMostInvaders[invaderColumn] != nullptr)
                    {
                        processedInvaders.erase(bottomMostInvaders[invaderColumn]);
                    }

                    bottomMostInvaders[invaderColumn] = &invader;
                }

                processedInvaders.insert(&invader);
            }
        }

        for (auto *enemy : processedInvaders)
        {
            if (enemy->shootingCooldown > 0)
            {
                enemy->shootingCooldown--;
            }
            else if (rand() % 1000 < shootingChance)
            {
                enemy->shootingCooldown = 4 * invaderCount;
                bullets.push_back({eDirection::DOWN, enemy->x + 1, enemy->y});
            }
        }

        // Bullet movement
        for (auto it = bullets.begin(); it != bullets.end(); ++it)
        {
            if (it->forRemoval)
            {
                continue;
            }

            int diffY = it->direction == eDirection::UP
                            ? -1
                            : 1;

            if (CanBulletMove(it->y + diffY))
            {
                it->y += diffY;
            }
            else
            {
                it->forRemoval = true;
                continue;
            }

            // Check for bullet on bullet collisions
            bool bulletDestroyed = false;
            for (auto other = bullets.begin(); other != bullets.end(); ++other)
            {
                if (it != other &&
                    it->x == other->x &&
                    it->y == other->y)
                {
                    if (it->direction == eDirection::UP &&
                        other->direction == eDirection::DOWN)
                    {
                        // Destroy player's bullet
                        it->forRemoval = true;

                        // Random chance to destroy invader's bullet
                        if (randomChance(0.5))
                        {
                            other->forRemoval = true;
                        }
                        break;
                    }
                    else if (it->direction == eDirection::DOWN &&
                             other->direction == eDirection::UP)
                    {
                        // Destroy player's bullet
                        other->forRemoval = true;

                        // Random chance to destroy invader's bullet
                        if (randomChance(0.5))
                        {
                            it->forRemoval = true;
                        }
                        break;
                    }
                }
            }

            if (it->forRemoval)
            {
                continue;
            }

            // Hit barrier?
            if (pField[(fieldWidth * it->y) + it->x] == eDisplay::BARRIER)
            {
                explosions.push_back({it->x, it->y});
                pField[(fieldWidth * it->y) + it->x] = eDisplay::SPACE;
                it->forRemoval = true;
            }
            else if (it->direction == eDirection::UP)
            {
                // Hit invader?
                auto enemyIt = std::find_if(invaders.begin(), invaders.end(),
                                            [&it](Invader a)
                                            {
                                                int invaderWidth = invaderWidths[a.type];
                                                for (int dx = 0; dx < invaderWidth; ++dx)
                                                {
                                                    if ((a.x + dx) == it->x && a.y == it->y)
                                                    {
                                                        return true;
                                                    }
                                                }
                                                return false;
                                            });

                if (enemyIt != invaders.end())
                {
                    int invaderWidth = invaderWidths[enemyIt->type];
                    int scoredPoints = invaderScores[enemyIt->type];
                    for (int dx = 0; dx < invaderWidth; ++dx)
                    {
                        explosions.push_back({enemyIt->x + dx, enemyIt->y});
                    }

                    score += scoredPoints;
                    invaders.erase(enemyIt);
                    it->forRemoval = true;
                }
            }
            else
            {
                // Hit player?
                bool playerHit = false;
                for (int dx = 0; dx < playerWidth; ++dx)
                {
                    if ((playerX + dx) == it->x && playerY == it->y)
                    {
                        playerLives -= 1;
                        if (playerLives == 0)
                        {
                            gameOver = true;
                        }

                        // Blow up player
                        for (int dx = 0; dx < playerWidth; ++dx)
                        {
                            explosions.push_back({playerX + dx, playerY});
                        }

                        // Re-center player on death
                        playerX = fieldWidth / 2;
                        playerY = fieldHeight - 1;

                        it->forRemoval = true;
                        playerHit = true;
                        break;
                    }
                }

                if (playerHit)
                {
                    continue;
                }
            }
        }

        // Remove bullets marked as collided
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                                     [](const Bullet &bullet)
                                     {
                                         return bullet.forRemoval;
                                     }),
                      bullets.end());

        /* Logic END
         */

        // Draw
        Draw(drawOffset);

        // Display
        WriteConsoleOutputCharacterW(hConsole, screen, screenWidth * screenHeight, {0, 0}, &dwBytesWritten);

        /* More Logic?! START
         */
        // Have you beaten the level?
        if (invaders.empty())
        {
            level++;
            Setup(level > 5 ? 5 : level);

            // TODO increase speed of invaders
            // TODO increase shooting chance of invaders

            // Quickly show the newly setup game with a pause
            Draw(drawOffset);
            WriteConsoleOutputCharacterW(hConsole, screen, screenWidth * screenHeight, {0, 0}, &dwBytesWritten);
            std::this_thread::sleep_for(1s);
        }
        /* More Logic?! END
         */
    }

    // Game over, tidy up
    swprintf_s(&screen[(screenWidth * (actualHeight + 4)) + (drawOffset)], fieldWidth, L"Game Over!");

    WriteConsoleOutputCharacterW(hConsole, screen, screenWidth * screenHeight, {0, 0}, &dwBytesWritten);
    std::this_thread::sleep_for(2s);

    // Wait to close...
    ClearInputBuffer();
    EnableEcho();

    swprintf_s(&screen[(screenWidth * (actualHeight + 14)) + (drawOffset)], fieldWidth, L"Press any key to exit...");
    WriteConsoleOutputCharacterW(hConsole, screen, screenWidth * screenHeight, {0, 0}, &dwBytesWritten);

    _getch();

    // Clean up things here
    CloseHandle(hConsole);
    ClearInputBuffer();

    // logFile.close();

    delete[] screen;
    delete[] pField;

    return 0;
}
