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
