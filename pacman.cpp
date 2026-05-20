// =============================================================================
// PAC-MAN  —  CSE 426 Computer Graphics Lab  |  Spring 2025
// =============================================================================
// Base requirements implemented:
//   [x] Playable Pac-Man game
//   [x] Menu page with Start / High Score / Exit
//   [x] Pause / Resume / Exit at any time
//   [x] Time, score, and lives shown while playing
//   [x] Ghosts become faster over time
//   [x] Win when all dots eaten, lose when lives run out
//
// Bonus features implemented:
//   [x] Unique ghost personalities (Blinky chase, Pinky ambush,
//       Inky pincer, Clyde scatter)
//   [x] Power pellets — eat a ghost while energized for bonus points
//   [x] Ghosts turn blue (frightened) when power pellet eaten
//   [x] Post-death invincibility grace period with flashing animation
//   [x] High score tracking across sessions
//   [x] Game-over and You-Win screens with stats
// =============================================================================

#define GL_SILENCE_DEPRECATION
#ifdef __APPLE__
  #include <GLUT/glut.h>
  #undef main
#else
  #include <GL/glut.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define WINDOW_WIDTH   900
#define WINDOW_HEIGHT  620
#define MAZE_WIDTH      28
#define MAZE_HEIGHT     31
#define CELL_SIZE       18
#define MAZE_OFFSET_X   10   // pixels from left edge to maze
#define MAZE_OFFSET_Y   10   // pixels from top  edge to maze

// Power pellet locations (hardcoded to match classic positions)
#define NUM_POWER_PELLETS 4

// ---------------------------------------------------------------------------
// Direction encoding  (used by Pacman and ghosts)
//   0 = right   1 = up   2 = left   3 = down
// Grid offsets matching this encoding:
//   dx4[d], dy4[d]
// Note: the maze uses a top-down y-axis (row 0 = top, row 30 = bottom).
// ---------------------------------------------------------------------------
const int DX4[4] = {  1,  0, -1,  0 };
const int DY4[4] = {  0, -1,  0,  1 };

// ---------------------------------------------------------------------------
// Game states
// ---------------------------------------------------------------------------
enum GameState { MENU, PLAYING, PAUSED, GAME_OVER, WIN };
GameState currentState = MENU;

// ---------------------------------------------------------------------------
// Maze grid
//   1 = wall
//   0 = path with regular dot
//   2 = empty path (dot eaten)
//   3 = ghost-house interior  (impassable to Pacman and roaming ghosts)
//   4 = power pellet (large dot — eating one frightens all ghosts)
// ---------------------------------------------------------------------------
int maze[MAZE_HEIGHT][MAZE_WIDTH];
int totalDots  = 0;  // count of cells that start as 0 or 4
int dotsEaten  = 0;

// ---------------------------------------------------------------------------
// Pacman
// ---------------------------------------------------------------------------
struct Pacman {
    float x, y;           // position in grid-cell units
    float speed;
    int   direction;      // current movement direction
    int   nextDirection;  // buffered keyboard input
    float mouthAngle;     // current mouth opening (degrees, 0..45)
    int   mouthOpening;   // +1 = opening, -1 = closing
    int   lives;
    int   score;
    float invincibleTimer; // post-death grace period (seconds)
    float energizedTimer;  // power-pellet effect remaining (seconds)
} pacman;

// ---------------------------------------------------------------------------
// Ghost
// ---------------------------------------------------------------------------
struct Ghost {
    float x, y;
    float speed;
    int   direction;
    int   color;          // 0=Red 1=Pink 2=Cyan 3=Orange
    bool  exitingHouse;   // true while navigating out of the ghost house
    float exitDelay;      // absolute gameTime when this ghost may leave
    bool  frightened;     // true while Pacman is energized
    bool  eaten;          // true after being eaten — ghost returns to house
    float frightenTimer;  // mirrors pacman.energizedTimer
} ghosts[4];

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
float gameTime  = 0.0f;
float startTime = 0.0f;
int   highScore = 0;
