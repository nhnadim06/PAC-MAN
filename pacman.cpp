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
const int DX4[4] = { 1,  0, -1,  0 };
const int DY4[4] = { 0, -1,  0,  1 };

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
int totalDots = 0;  // count of cells that start as 0 or 4
int dotsEaten = 0;

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
float gameTime = 0.0f;
float startTime = 0.0f;
int   highScore = 0;

// ---------------------------------------------------------------------------
// initMaze — copy layout, mark power pellet positions, count total dots
// ---------------------------------------------------------------------------
void initMaze() {
    int layout[MAZE_HEIGHT][MAZE_WIDTH] = {
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,0,1,1,1,1,0,1,1,1,1,1,0,1,1,0,1,1,1,1,1,0,1,1,1,1,0,1},
        {1,4,1,1,1,1,0,1,1,1,1,1,0,1,1,0,1,1,1,1,1,0,1,1,1,1,4,1},
        {1,0,1,1,1,1,0,1,1,1,1,1,0,1,1,0,1,1,1,1,1,0,1,1,1,1,0,1},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,0,1,1,1,1,0,1,1,0,1,1,1,1,1,1,1,1,0,1,1,0,1,1,1,1,0,1},
        {1,0,1,1,1,1,0,1,1,0,1,1,1,1,1,1,1,1,0,1,1,0,1,1,1,1,0,1},
        {1,0,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,1},
        {1,1,1,1,1,1,0,1,1,1,1,1,2,1,1,2,1,1,1,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,1,1,1,1,1,2,1,1,2,1,1,1,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,1,1,2,2,2,2,2,2,2,2,2,2,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,1,1,2,1,1,1,3,3,1,1,1,2,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,1,1,2,1,3,3,3,3,3,3,1,2,1,1,0,1,1,1,1,1,1},
        {2,2,2,2,2,2,0,2,2,2,1,3,3,3,3,3,3,1,2,2,2,0,2,2,2,2,2,2},
        {1,1,1,1,1,1,0,1,1,2,1,3,3,3,3,3,3,1,2,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,1,1,2,1,1,1,1,1,1,1,1,2,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,1,1,2,2,2,2,2,2,2,2,2,2,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,1,1,2,1,1,1,1,1,1,1,1,2,1,1,0,1,1,1,1,1,1},
        {1,1,1,1,1,1,0,1,1,2,1,1,1,1,1,1,1,1,2,1,1,0,1,1,1,1,1,1},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,0,1,1,1,1,0,1,1,1,1,1,0,1,1,0,1,1,1,1,1,0,1,1,1,1,0,1},
        {1,0,1,1,1,1,0,1,1,1,1,1,0,1,1,0,1,1,1,1,1,0,1,1,1,1,0,1},
        {1,4,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,4,1},
        {1,1,1,0,1,1,0,1,1,0,1,1,1,1,1,1,1,1,0,1,1,0,1,1,0,1,1,1},
        {1,1,1,0,1,1,0,1,1,0,1,1,1,1,1,1,1,1,0,1,1,0,1,1,0,1,1,1},
        {1,0,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,1,1,0,0,0,0,0,0,1},
        {1,0,1,1,1,1,1,1,1,1,1,1,0,1,1,0,1,1,1,1,1,1,1,1,1,1,0,1},
        {1,0,1,1,1,1,1,1,1,1,1,1,0,1,1,0,1,1,1,1,1,1,1,1,1,1,0,1},
        {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
        {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
    };

    totalDots = 0;
    for (int i = 0; i < MAZE_HEIGHT; i++)
        for (int j = 0; j < MAZE_WIDTH; j++) {
            maze[i][j] = layout[i][j];
            if (maze[i][j] == 0 || maze[i][j] == 4) totalDots++;
        }
}

// ---------------------------------------------------------------------------
// initPacman
// ---------------------------------------------------------------------------
void initPacman() {
    pacman.x = 13.5f;
    pacman.y = 23.5f;
    pacman.speed = 0.1f;
    pacman.direction = 0;
    pacman.nextDirection = 0;
    pacman.mouthAngle = 0.0f;
    pacman.mouthOpening = 1;
    pacman.lives = 3;
    pacman.score = 0;
    pacman.invincibleTimer = 0.0f;
    pacman.energizedTimer = 0.0f;
}

// ---------------------------------------------------------------------------
// initGhosts
// exitDelay is an ABSOLUTE gameTime threshold — correct across respawns.
// Pass timeOffset=0 at game start, timeOffset=gameTime after a death.
// ---------------------------------------------------------------------------
void initGhosts(float timeOffset = 0.0f) {
    // Blinky — starts at open corridor (13,11), maze[11][13]==2.
    // Integer coords required: floorf(13.0+0.5)=13, floorf(11.0+0.5)=11.
    // Using 11.5 would snap to row 12 (ghost house, cell 3) and break AI.
    ghosts[0].x = 13.0f; ghosts[0].y = 11.0f; ghosts[0].color = 0; ghosts[0].speed = 0.08f;
    ghosts[0].exitingHouse = false; ghosts[0].exitDelay = timeOffset + 0.0f;

    ghosts[1].x = 13.5f; ghosts[1].y = 14.0f; ghosts[1].color = 1; ghosts[1].speed = 0.08f;
    ghosts[1].exitingHouse = true;  ghosts[1].exitDelay = timeOffset + 3.0f;

    ghosts[2].x = 11.5f; ghosts[2].y = 14.0f; ghosts[2].color = 2; ghosts[2].speed = 0.08f;
    ghosts[2].exitingHouse = true;  ghosts[2].exitDelay = timeOffset + 6.0f;

    ghosts[3].x = 15.5f; ghosts[3].y = 14.0f; ghosts[3].color = 3; ghosts[3].speed = 0.08f;
    ghosts[3].exitingHouse = true;  ghosts[3].exitDelay = timeOffset + 9.0f;

    for (int i = 0; i < 4; i++) {
        ghosts[i].direction = 1;     // start moving upward
        ghosts[i].frightened = false;
        ghosts[i].eaten = false;
        ghosts[i].frightenTimer = 0.0f;
    }
}

// ---------------------------------------------------------------------------
// resetGame
// ---------------------------------------------------------------------------
void resetGame() {
    initMaze();
    initPacman();
    initGhosts(0.0f);
    dotsEaten = 0;
    gameTime = 0.0f;
    startTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
}

// ---------------------------------------------------------------------------
// canMove — true if stepping one unit in 'dir' from (x,y) stays within
//           the maze and does NOT enter a wall (1) or ghost house (3).
//           Ghost exit-path code bypasses this intentionally.
// ---------------------------------------------------------------------------
bool canMove(float x, float y, int dir) {
    float nx = x + DX4[dir] * 0.4f;
    float ny = y + DY4[dir] * 0.4f;
    int gx = (int)(nx + 0.5f);
    int gy = (int)(ny + 0.5f);
    if (gx < 0 || gx >= MAZE_WIDTH || gy < 0 || gy >= MAZE_HEIGHT) return false;
    int c = maze[gy][gx];
    return (c != 1 && c != 3);
}

// ---------------------------------------------------------------------------
// chaseGreedy — steers 'ghost' one step toward (targetX, targetY).
//
// Coordinate system: y increases DOWNWARD (row 0 = screen top).
// So "target is below" means targetY > ghost.y → direction 3 (down).
//
// Rule (mirrors classic arcade Pac-Man):
//   Moving horizontally → try vertical turn toward target first.
//   Moving vertically   → try horizontal turn toward target first.
//   Preferred turn blocked → keep straight.
//   Straight also blocked  → any open non-reverse direction.
//
// Called only at cell centres to prevent mid-corridor direction changes.
// ---------------------------------------------------------------------------
void chaseGreedy(Ghost& ghost, float targetX, float targetY) {
    int cur = ghost.direction;
    int opp = (cur + 2) % 4;

    // Pick the best non-reversing direction toward the target
    // Strategy: always choose from all 4 directions (minus reverse),
    // picking whichever brings the ghost closest to the target.
    int bestDir = -1;
    float bestDist = 1e9f;

    for (int d = 0; d < 4; d++) {
        if (d == opp) continue;            // never reverse
        if (!canMove(ghost.x, ghost.y, d)) continue;
        float nx = ghost.x + DX4[d];
        float ny = ghost.y + DY4[d];
        float dist = (nx - targetX) * (nx - targetX) + (ny - targetY) * (ny - targetY);
        if (dist < bestDist) { bestDist = dist; bestDir = d; }
    }

    if (bestDir != -1) ghost.direction = bestDir;
    // If ALL non-reverse dirs are blocked (dead end), allow reverse
    else if (!canMove(ghost.x, ghost.y, cur)) ghost.direction = opp;
}

// ---------------------------------------------------------------------------
// fleeGreedy — steers ghost AWAY from (targetX, targetY).
// Comparisons are inverted relative to chaseGreedy.
// ---------------------------------------------------------------------------
void fleeGreedy(Ghost& ghost, float targetX, float targetY) {
    int cur = ghost.direction;
    int opp = (cur + 2) % 4;

    // Pick the direction that moves FARTHEST from target
    int bestDir = -1;
    float bestDist = -1.0f;

    for (int d = 0; d < 4; d++) {
        if (d == opp) continue;
        if (!canMove(ghost.x, ghost.y, d)) continue;
        float nx = ghost.x + DX4[d];
        float ny = ghost.y + DY4[d];
        float dist = (nx - targetX) * (nx - targetX) + (ny - targetY) * (ny - targetY);
        if (dist > bestDist) { bestDist = dist; bestDir = d; }
    }

    if (bestDir != -1) ghost.direction = bestDir;
    else if (!canMove(ghost.x, ghost.y, cur)) ghost.direction = opp;
}

// ===========================================================================
// DRAWING
// ===========================================================================

void drawCircle(float cx, float cy, float r, int seg) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= seg; i++) {
        float a = 2.0f * M_PI * i / seg;
        glVertex2f(cx + cos(a) * r, cy + sin(a) * r);
    }
    glEnd();
}

// World-to-screen helper for maze cells
float cellX(float gx) { return MAZE_OFFSET_X + gx * CELL_SIZE; }
float cellY(float gy) { return MAZE_OFFSET_Y + gy * CELL_SIZE; }

void drawMaze() {
    for (int i = 0; i < MAZE_HEIGHT; i++) {
        for (int j = 0; j < MAZE_WIDTH; j++) {
            float x = cellX(j);
            float y = cellY(i);

            if (maze[i][j] == 1) {
                // Wall — deep blue with a subtle lighter border for depth
                glColor3f(0.05f, 0.05f, 0.55f);
                glBegin(GL_QUADS);
                glVertex2f(x, y); glVertex2f(x + CELL_SIZE, y);
                glVertex2f(x + CELL_SIZE, y + CELL_SIZE); glVertex2f(x, y + CELL_SIZE);
                glEnd();
                // Inner highlight
                glColor3f(0.1f, 0.1f, 0.8f);
                glBegin(GL_LINE_LOOP);
                glVertex2f(x + 1, y + 1); glVertex2f(x + CELL_SIZE - 1, y + 1);
                glVertex2f(x + CELL_SIZE - 1, y + CELL_SIZE - 1); glVertex2f(x + 1, y + CELL_SIZE - 1);
                glEnd();

            }
            else if (maze[i][j] == 0) {
                // Regular dot
                glColor3f(1.0f, 0.9f, 0.7f);
                drawCircle(x + CELL_SIZE / 2, y + CELL_SIZE / 2, 1.5f, 8);

            }
            else if (maze[i][j] == 4) {
                // Power pellet — larger, pulsing yellow
                float pulse = 2.5f + 1.0f * sinf(gameTime * 5.0f);
                glColor3f(1.0f, 1.0f, 0.2f);
                drawCircle(x + CELL_SIZE / 2, y + CELL_SIZE / 2, pulse, 16);
            }
            // cells 2 and 3 render as black background
        }
    }
}

void drawPacman() {
    float sx = cellX(pacman.x) + CELL_SIZE / 2;
    float sy = cellY(pacman.y) + CELL_SIZE / 2;

    glPushMatrix();
    glTranslatef(sx, sy, 0);
    glRotatef(-pacman.direction * 90.0f, 0, 0, 1);

    // Colour: flashes white during grace period, yellow when energized, normal yellow
    if (pacman.invincibleTimer > 0.0f) {
        int f = (int)(pacman.invincibleTimer * 10) % 2;
        if (f == 0) glColor3f(1, 1, 1); else glColor3f(1, 1, 0);
    }
    else if (pacman.energizedTimer > 0.0f) {
        glColor3f(1.0f, 0.8f, 0.0f); // slightly orange tint while powered
    }
    else {
        glColor3f(1.0f, 1.0f, 0.0f);
    }

    float sa = pacman.mouthAngle * M_PI / 180.0f;
    float ea = (360.0f - pacman.mouthAngle) * M_PI / 180.0f;
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(0, 0);
    for (float a = sa; a <= ea; a += 0.1f)
        glVertex2f(cos(a) * 8, sin(a) * 8);
    glEnd();
    glPopMatrix();
}

void drawGhost(Ghost& g) {
    float x = cellX(g.x) + CELL_SIZE / 2;
    float y = cellY(g.y) + CELL_SIZE / 2;

    // Frightened ghosts are blue; eaten ghosts are just eyes (returning)
    if (g.eaten) {
        // Only draw eyes when returning to house
        glColor3f(1, 1, 1);
        drawCircle(x - 3, y - 2, 2, 16);
        drawCircle(x + 3, y - 2, 2, 16);
        glColor3f(0, 0, 1);
        drawCircle(x - 3, y - 2, 1, 16);
        drawCircle(x + 3, y - 2, 1, 16);
        return;
    }

    if (g.frightened) {
        // Flash white when fright is about to expire (last 2 seconds)
        if (g.frightenTimer < 2.0f) {
            int f = (int)(g.frightenTimer * 5) % 2;
            if (f == 0) glColor3f(1, 1, 1); else glColor3f(0.1f, 0.1f, 0.8f);
        }
        else {
            glColor3f(0.1f, 0.1f, 0.8f); // dark blue
        }
    }
    else {
        switch (g.color) {
        case 0: glColor3f(1.0f, 0.0f, 0.0f);  break; // Blinky — red
        case 1: glColor3f(1.0f, 0.75f, 0.8f);  break; // Pinky  — pink
        case 2: glColor3f(0.0f, 1.0f, 1.0f);  break; // Inky   — cyan
        case 3: glColor3f(1.0f, 0.65f, 0.0f);  break; // Clyde  — orange
        }
    }

    // Dome
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int i = 0; i <= 180; i += 10) {
        float a = i * M_PI / 180.0f;
        glVertex2f(x + cos(a) * 8, y + sin(a) * 8);
    }
    glEnd();

    // Wavy skirt
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    glVertex2f(x - 8, y); glVertex2f(x - 6, y + 4); glVertex2f(x - 3, y);
    glVertex2f(x, y + 4); glVertex2f(x + 3, y);   glVertex2f(x + 6, y + 4); glVertex2f(x + 8, y);
    glEnd();

    // Eyes (white + blue pupil)
    glColor3f(1, 1, 1);
    drawCircle(x - 3, y - 2, 2, 16); drawCircle(x + 3, y - 2, 2, 16);
    glColor3f(0, 0, 1);
    drawCircle(x - 3, y - 2, 1, 16); drawCircle(x + 3, y - 2, 1, 16);
  
void drawText(float x, float y, const char *text, void *font = GLUT_BITMAP_HELVETICA_18) {
    glRasterPos2f(x, y);
    for (const char *c = text; *c; c++)
        glutBitmapCharacter(font, *c);
}

void drawLargeText(float x, float y, const char *text) {
    drawText(x, y, text, GLUT_BITMAP_TIMES_ROMAN_24);
}

// Side panel HUD
void drawHUD() {
    float px = (float)(MAZE_OFFSET_X + MAZE_WIDTH * CELL_SIZE + 20);
    float py = 60.0f;
    char buf[64];

    glColor3f(1.0f, 1.0f, 0.0f);
    drawLargeText(px, py, "PAC-MAN");
    py += 40;

    glColor3f(0.8f, 0.8f, 0.8f);
    drawText(px, py, "SCORE"); py += 22;
    snprintf(buf, sizeof(buf), "%d", pacman.score);
    glColor3f(1,1,1); drawLargeText(px, py, buf); py += 36;

    glColor3f(0.8f, 0.8f, 0.8f);
    drawText(px, py, "HIGH SCORE"); py += 22;
    snprintf(buf, sizeof(buf), "%d", highScore);
    glColor3f(1,1,0); drawLargeText(px, py, buf); py += 36;

    glColor3f(0.8f, 0.8f, 0.8f);
    drawText(px, py, "TIME"); py += 22;
    snprintf(buf, sizeof(buf), "%.1f s", gameTime);
    glColor3f(1,1,1); drawText(px, py, buf); py += 34;

    glColor3f(0.8f, 0.8f, 0.8f);
    drawText(px, py, "DOTS"); py += 22;
    snprintf(buf, sizeof(buf), "%d / %d", dotsEaten, totalDots);
    glColor3f(1,1,1); drawText(px, py, buf); py += 34;

    // Lives shown as small Pac-Man circles
    glColor3f(0.8f, 0.8f, 0.8f);
    drawText(px, py, "LIVES"); py += 22;
    for (int i = 0; i < pacman.lives; i++) {
        glColor3f(1,1,0);
        drawCircle(px + 10 + i * 22, py - 5, 7, 20);
    }
    py += 30;

    // Energized status
    if (pacman.energizedTimer > 0.0f) {
        glColor3f(1.0f, 1.0f, 0.2f);
        snprintf(buf, sizeof(buf), "POWER %.1fs", pacman.energizedTimer);
        drawText(px, py, buf); py += 24;
    }

    if (pacman.invincibleTimer > 0.0f) {
        glColor3f(0.6f, 0.6f, 1.0f);
        drawText(px, py, "SAFE...");
    }

    // Controls reminder at bottom
    glColor3f(0.4f, 0.4f, 0.4f);
    drawText(px, WINDOW_HEIGHT - 100, "Arrow keys: move");
    drawText(px, WINDOW_HEIGHT -  80, "P: pause");
    drawText(px, WINDOW_HEIGHT -  60, "ESC: menu");
}

void drawMenu() {
    glColor3f(1.0f, 1.0f, 0.0f);
    drawLargeText(330, 120, "PAC-MAN");

    glColor3f(0.8f, 0.8f, 0.0f);
    drawText(330, 150, "CSE 426  Computer Graphics Lab");

    // Decorative ghost row
    Ghost dg;
    dg.eaten=false; dg.frightened=false;
    float dy = 210;
    for (int c = 0; c < 4; c++) {
        dg.color=c; dg.x=(280+c*50)/(float)CELL_SIZE - (float)MAZE_OFFSET_X/CELL_SIZE;
        dg.y=dy/(float)CELL_SIZE - (float)MAZE_OFFSET_Y/CELL_SIZE;
        float gx=280+c*50, gy=dy;
        // draw directly
        switch(c){
            case 0:glColor3f(1,0,0);break;
            case 1:glColor3f(1,.75f,.8f);break;
            case 2:glColor3f(0,1,1);break;
            case 3:glColor3f(1,.65f,0);break;
        }
        glBegin(GL_TRIANGLE_FAN); glVertex2f(gx,gy);
        for(int i=0;i<=180;i+=10){float a=i*M_PI/180.0f;glVertex2f(gx+cos(a)*12,gy+sin(a)*12);}
        glEnd();
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(gx,gy);
        glVertex2f(gx-12,gy);glVertex2f(gx-9,gy+6);glVertex2f(gx-4,gy);
        glVertex2f(gx,gy+6);glVertex2f(gx+4,gy);glVertex2f(gx+9,gy+6);glVertex2f(gx+12,gy);
        glEnd();
        glColor3f(1,1,1);drawCircle(gx-4,gy-3,3,16);drawCircle(gx+4,gy-3,3,16);
        glColor3f(0,0,1);drawCircle(gx-4,gy-3,1.5f,16);drawCircle(gx+4,gy-3,1.5f,16);
    }

    glColor3f(1,1,1);
    drawLargeText(290, 290, "Press SPACE to Start");

    glColor3f(0.7f, 0.7f, 0.7f);
    drawText(315, 340, "Press H to see High Score");
    drawText(315, 370, "Arrow Keys to move  |  P to pause");
    drawText(315, 400, "Eat power pellets to frighten ghosts!");
    drawText(315, 430, "Press ESC to Exit");

    char buf[64];
    glColor3f(1,1,0);
    snprintf(buf, sizeof(buf), "Best Time: %s", highScore > 0 ? "recorded" : "none yet");
    drawText(320, 470, buf);
}

void drawPauseScreen() {
    // Semi-transparent overlay
    glColor4f(0,0,0,0.5f);
    glBegin(GL_QUADS);
    glVertex2f(0,0);glVertex2f(WINDOW_WIDTH,0);
    glVertex2f(WINDOW_WIDTH,WINDOW_HEIGHT);glVertex2f(0,WINDOW_HEIGHT);
    glEnd();

    glColor3f(1,1,0);
    drawLargeText(350, 260, "PAUSED");
    glColor3f(1,1,1);
    drawText(300, 310, "Press P to Resume");
    drawText(300, 340, "Press ESC to return to Menu");
}

void drawGameOver() {
    glColor3f(1,0,0); drawLargeText(320, 220, "GAME OVER");
    glColor3f(1,1,1);
    char buf[64];
    snprintf(buf,sizeof(buf),"Final Score: %d", pacman.score); drawText(300,280,buf);
    snprintf(buf,sizeof(buf),"Time: %.1f seconds", gameTime);  drawText(300,310,buf);
    if (pacman.score > highScore) {
        glColor3f(1,1,0); drawText(285,345,"** NEW HIGH SCORE! **");
    }
    glColor3f(0.8f,0.8f,0.8f);
    drawText(270,390,"Press SPACE to Play Again");
    drawText(285,420,"Press ESC for Menu");
}

void drawWinScreen() {
    glColor3f(0,1,0); drawLargeText(330, 210, "YOU WIN!");
    glColor3f(1,1,1);
    char buf[64];
    snprintf(buf,sizeof(buf),"Score: %d", pacman.score);      drawText(310,270,buf);
    snprintf(buf,sizeof(buf),"Time: %.1f s", gameTime);       drawText(310,300,buf);
    if (pacman.score > highScore) {
        glColor3f(1,1,0); drawText(290,335,"** NEW HIGH SCORE! **");
    }
    glColor3f(0.8f,0.8f,0.8f);
    drawText(270,385,"Press SPACE to Play Again");
    drawText(285,415,"Press ESC for Menu");
}

// ===========================================================================
// UPDATE
// ===========================================================================

void updatePacman() {
    // Apply buffered direction if the path is clear
    if (canMove(pacman.x, pacman.y, pacman.nextDirection))
        pacman.direction = pacman.nextDirection;

    // Move
    if (canMove(pacman.x, pacman.y, pacman.direction)) {
        pacman.x += DX4[pacman.direction] * pacman.speed;
        pacman.y += DY4[pacman.direction] * pacman.speed;
    }

    // Horizontal tunnel wrap-around
    if (pacman.x < 0)            pacman.x = MAZE_WIDTH - 1;
    if (pacman.x >= MAZE_WIDTH)  pacman.x = 0;

    // Mouth animation
    pacman.mouthAngle += pacman.mouthOpening * 3;
    if (pacman.mouthAngle >= 45) pacman.mouthOpening = -1;
    if (pacman.mouthAngle <= 0)  pacman.mouthOpening =  1;

    // Eat dot / power pellet at current cell
    int gx = (int)(pacman.x + 0.5f);
    int gy = (int)(pacman.y + 0.5f);

    if (maze[gy][gx] == 0) {
        maze[gy][gx] = 2;
        dotsEaten++;
        pacman.score += 10;

    } else if (maze[gy][gx] == 4) {
        maze[gy][gx] = 2;
        dotsEaten++;
        pacman.score += 50;
        // Energize Pacman — frighten all active ghosts
        pacman.energizedTimer = 8.0f;
        for (int i = 0; i < 4; i++) {
            if (!ghosts[i].exitingHouse && !ghosts[i].eaten) {
                ghosts[i].frightened    = true;
                ghosts[i].frightenTimer = 8.0f;
                // Reverse direction when frightened
                ghosts[i].direction = (ghosts[i].direction + 2) % 4;
            }
        }
    }

    // Win condition
    if (dotsEaten >= totalDots) {
        currentState = WIN;
        if (pacman.score > highScore) highScore = pacman.score;
    }

    // Tick timers
    const float dt = 0.016f;
    if (pacman.invincibleTimer > 0.0f) {
        pacman.invincibleTimer -= dt;
        if (pacman.invincibleTimer < 0.0f) pacman.invincibleTimer = 0.0f;
    }
    if (pacman.energizedTimer > 0.0f) {
        pacman.energizedTimer -= dt;
        if (pacman.energizedTimer < 0.0f) {
            pacman.energizedTimer = 0.0f;
            // Un-frighten ghosts when power wears off
            for (int i = 0; i < 4; i++) {
                ghosts[i].frightened    = false;
                ghosts[i].frightenTimer = 0.0f;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// updateGhosts
//
// PHASE 1 — EXIT HOUSE
//   Wait until gameTime >= exitDelay, then follow a fixed path:
//   drift to column 13 → move up to row 11 (open corridor) → begin roaming.
//
// PHASE 2 — GHOST AI (runs only at cell centres / intersections)
//
//   Normal mode — personality targets:
//     Blinky (0): Pacman's exact cell                  [relentless chaser]
//     Pinky  (1): 4 cells ahead of Pacman's direction  [ambush attacker]
//     Inky   (2): reflection of Blinky through Pacman  [pincer flanker]
//     Clyde  (3): chase if far (>8), scatter if close  [erratic / shy]
//
//   Frightened mode — flee from Pacman's position.
//
//   Eaten mode — return to ghost house centre (13,14).
// ---------------------------------------------------------------------------
void updateGhosts() {
    int eatBonus = 200;

    for (int i = 0; i < 4; i++) {

        Ghost &g = ghosts[i];

        // =========================================================
        // PHASE 1 : EXIT GHOST HOUSE
        // =========================================================
        if (g.exitingHouse) {

            if (gameTime < g.exitDelay)
                continue;

            const float exitX = 13.0f;
            const float exitY = 11.0f;

            // Move horizontally toward exit column
            if (fabs(g.x - exitX) > g.speed) {

                if (g.x < exitX)
                    g.x += g.speed;
                else
                    g.x -= g.speed;
            }
            else {

                g.x = exitX;

                // Move upward out of house
                if (g.y > exitY + g.speed) {

                    g.y -= g.speed;
                }
                else {

                    // Fully exited
                    g.x = exitX;
                    g.y = exitY;

                    g.exitingHouse = false;

                    // Start moving left or right
                    g.direction = (rand() % 2 == 0) ? 0 : 2;
                }
            }

            continue;
        }

        // =========================================================
        // EATEN GHOST RETURNS HOME
        // =========================================================
        if (g.eaten) {

            float hx = 13.0f;
            float hy = 14.0f;

            chaseGreedy(g, hx, hy);

            if (canMove(g.x, g.y, g.direction)) {

                g.x += DX4[g.direction] * g.speed * 2.0f;
                g.y += DY4[g.direction] * g.speed * 2.0f;
            }

            // Reached home
            if (fabs(g.x - hx) < 0.3f &&
                fabs(g.y - hy) < 0.3f) {

                g.x = hx;
                g.y = hy;

                g.eaten = false;
                g.frightened = false;
                g.frightenTimer = 0.0f;

                g.exitingHouse = true;
                g.exitDelay = gameTime + 3.0f;
            }

            continue;
        }

        // =========================================================
        // PHASE 2 : NORMAL GHOST AI
        // =========================================================

        // Detect if ghost is close to tile center
        float centerX = floorf(g.x + 0.5f);
        float centerY = floorf(g.y + 0.5f);

        float fx = fabs(g.x - centerX);
        float fy = fabs(g.y - centerY);

        bool atCenter = (fx < 0.08f && fy < 0.08f);

        // ONLY update direction near intersections
        if (atCenter) {

            // Snap to exact center
            g.x = floorf(g.x + 0.5f);
            g.y = floorf(g.y + 0.5f);

            // -----------------------------------------------------
            // FRIGHTENED MODE
            // -----------------------------------------------------
            if (g.frightened) {

                fleeGreedy(g, pacman.x, pacman.y);

                g.frightenTimer -= 0.016f;

                if (g.frightenTimer <= 0.0f) {

                    g.frightened = false;
                    g.frightenTimer = 0.0f;
                }
            }

            // -----------------------------------------------------
            // NORMAL CHASE MODE
            // -----------------------------------------------------
            else {

                int pacGX = (int)(pacman.x + 0.5f);
                int pacGY = (int)(pacman.y + 0.5f);

                float tx, ty;

                // =========================
                // BLINKY
                // =========================
                if (i == 0) {

                    tx = (float)pacGX;
                    ty = (float)pacGY;
                }

                // =========================
                // PINKY
                // =========================
                else if (i == 1) {

                    tx = pacGX +
                         DX4[pacman.direction] * 4;

                    ty = pacGY +
                         DY4[pacman.direction] * 4;
                }

                // =========================
                // INKY
                // =========================
                else if (i == 2) {

                    int pivX =
                        pacGX +
                        DX4[pacman.direction] * 2;

                    int pivY =
                        pacGY +
                        DY4[pacman.direction] * 2;

                    int blX =
                        (int)(ghosts[0].x + 0.5f);

                    int blY =
                        (int)(ghosts[0].y + 0.5f);

                    tx = pivX + (pivX - blX);
                    ty = pivY + (pivY - blY);
                }

                // =========================
                // CLYDE
                // =========================
                else {

                    float d =
                        sqrtf(
                            powf(g.x - pacman.x, 2) +
                            powf(g.y - pacman.y, 2)
                        );

                    if (d > 8.0f) {

                        tx = (float)pacGX;
                        ty = (float)pacGY;
                    }
                    else {

                        tx = 1.0f;
                        ty = (float)(MAZE_HEIGHT - 2);
                    }
                }

                // Choose best direction
                chaseGreedy(g, tx, ty);
            }
        }

        // =========================================================
        // MOVE GHOST
        // =========================================================
        if (canMove(g.x, g.y, g.direction)) {
            // Snap cleanly when very close to center
        if (atCenter) {
            g.x = centerX;
            g.y = centerY;
        }

            g.x += DX4[g.direction] * g.speed;
            g.y += DY4[g.direction] * g.speed;
        }
        else {

            // If blocked, try another direction
            int opp = (g.direction + 2) % 4;

            bool moved = false;

            for (int d = 0; d < 4; d++) {

                if (d == opp)
                    continue;

                if (canMove(g.x, g.y, d)) {

                    g.direction = d;

                    g.x += DX4[d] * g.speed;
                    g.y += DY4[d] * g.speed;

                    moved = true;
                    break;
                }
            }

            // Dead-end → reverse
            if (!moved &&
                canMove(g.x, g.y, opp)) {

                g.direction = opp;

                g.x += DX4[opp] * g.speed;
                g.y += DY4[opp] * g.speed;
            }
        }

        // =========================================================
        // COLLISION WITH PACMAN
        // =========================================================
        float dist =
            sqrtf(
                powf(g.x - pacman.x, 2) +
                powf(g.y - pacman.y, 2)
            );

        if (dist < 0.6f && !g.exitingHouse) {

            // Pacman eats ghost
            if (g.frightened) {

                g.eaten = true;
                g.frightened = false;

                pacman.score += eatBonus;

                eatBonus *= 2;
            }

            // Ghost kills Pacman
            else if (pacman.invincibleTimer <= 0.0f) {

                pacman.lives--;

                if (pacman.lives <= 0) {

                    currentState = GAME_OVER;
                }
                else {

                    int savedLives = pacman.lives;
                    int savedScore = pacman.score;

                    initPacman();
                    initGhosts(gameTime);

                    pacman.lives = savedLives;
                    pacman.score = savedScore;

                    pacman.invincibleTimer = 2.0f;
                }
            }
        }

        // =========================================================
        // SPEED INCREASE OVER TIME
        // =========================================================
        g.speed = 0.08f + gameTime * 0.001f;

        if (g.speed > 0.15f)
            g.speed = 0.15f;
    }

}

// ===========================================================================
// GLUT CALLBACKS
// ===========================================================================

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    switch (currentState) {
    case MENU:
        drawMenu();
        break;
    case PAUSED:
        drawMaze(); drawPacman();
        for (int i = 0;i < 4;i++) drawGhost(ghosts[i]);
        drawHUD(); drawPauseScreen();
        break;
    case PLAYING:
        drawMaze(); drawPacman();
        for (int i = 0;i < 4;i++) drawGhost(ghosts[i]);
        drawHUD();
        break;
    case GAME_OVER: drawGameOver(); break;
    case WIN:       drawWinScreen(); break;
    }

    glutSwapBuffers();
}

void update(int v) {
    if (currentState == PLAYING) {
        gameTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f - startTime;
        updatePacman();
        updateGhosts();
    }
    glutPostRedisplay();
    glutTimerFunc(16, update, 0); // ~60 fps
}

void keyboard(unsigned char key, int x, int y) {
    switch (key) {
    case 27: // ESC
        if (currentState == PLAYING || currentState == PAUSED ||
            currentState == GAME_OVER || currentState == WIN)
            currentState = MENU;
        else exit(0);
        break;
    case ' ':
        if (currentState == MENU || currentState == GAME_OVER || currentState == WIN) {
            resetGame(); currentState = PLAYING;
        }
        break;
    case 'p': case 'P':
        if (currentState == PLAYING) {
            currentState = PAUSED;
        }
        else if (currentState == PAUSED) {
            currentState = PLAYING;
            startTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f - gameTime;
        }
        break;
    case 'h': case 'H':
        if (currentState == MENU)
            printf("High Score: %d\n", highScore);
        break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    if (currentState != PLAYING) return;
    switch (key) {
    case GLUT_KEY_RIGHT: pacman.nextDirection = 0; break;
    case GLUT_KEY_UP:    pacman.nextDirection = 1; break;
    case GLUT_KEY_LEFT:  pacman.nextDirection = 2; break;
    case GLUT_KEY_DOWN:  pacman.nextDirection = 3; break;
    }
}

void init() {
    glClearColor(0, 0, 0, 1);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    // y=0 at top, increases downward — matches maze row indexing
    gluOrtho2D(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    srand((unsigned)time(NULL));
    resetGame();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 80);
    glutCreateWindow("PAC-MAN — CSE 426 Computer Graphics Lab");
    init();
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutTimerFunc(0, update, 0);
    glutMainLoop();
    return 0;
}
