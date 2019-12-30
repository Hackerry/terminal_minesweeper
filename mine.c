#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <curses.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#define DEFAULT_W 10
#define DEFAULT_H 10
#define CW 5
#define CH 3
#define DEFAULT_MINE  10
#define DEFAULT_C     10

#define MINE '*'
#define FLAG 'F'
#define ZERO '0'
#define SHOW_MASK 0x80
#define VALUE_MASK 0x3f
#define FLAG_MASK 0x40

void printBoard(int physicalH, int physicalW);
void initGame();
void resetTerminal();
void initKeypad();
bool testMine();
bool showSquare(int currY, int currX);
void flagMine();
void showAll();
void gameWin();
void gameOver();
void addChar(int y, int x, chtype c);
void printInfo();
void updateInfo();
void redrawScreen();
void initColorPairs();
static int W = DEFAULT_W, H = DEFAULT_H, N = DEFAULT_MINE;
static int X = 0, Y = 0, STEP_X = CW-1, STEP_Y = CH-1;
static WINDOW* board_win;
static unsigned char* board_arry;
static unsigned char* temp_arry;
static unsigned int tempSize;
static char* info;
static char* infoFormat = "Remain: %d"; //Cheat here -> Correct: %d";
static char* control = "MOVE: [AWDS or arrows] PROBE: [SPACE] FLAG: F REDRAW: R";
static int REMAIN_N = 0, CORRECT_N = 0, GAME_END_PRESS = 0;
static bool GAME_END = false, COL_SUPP = false;

int main(int argc, char** argv) {
  if(argc > 1 && argc != 4) {
    fprintf(stderr, "Usage: mine [row] [col] [#mines] to customize map.\n");
    exit(1);
  } else if(argc == 4) {
    errno = 0;
    H = atoi(argv[1]);
    W = atoi(argv[2]);
    N = atoi(argv[3]);

    if(errno) {
      fprintf(stderr, "Error reading arguements.\n");
      exit(1);
    }

    // printf("%d-%d-%d\n", H, W, N);

    if(H < 10 || H > 40) {
      fprintf(stderr, "Row number must be no less than 10 and smaller than 40.\n");
      exit(1);
    } else if(W < 10 || W > 40) {
      fprintf(stderr, "Column number must be no less than 10 and smaller than 40.\n");
      exit(1);
    } else if(N <= 0) {
      fprintf(stderr, "Mine number must be positive.\n");
      exit(1);
    }

    // Just for fun
    if(N > W*H) N = W*H;
  }

  // Make sure terminal is big enough

  board_arry = calloc(H*W, sizeof(unsigned char));
  tempSize = H*W*2;
  temp_arry = calloc(tempSize, sizeof(unsigned char));
  info = calloc(120, sizeof(char));

  if(board_arry == NULL) {
    fprintf(stderr, "Error allocating memory.\n");
    exit(1);
  }
  if(temp_arry == NULL) {
    fprintf(stderr, "Error allocating memory.\n");
    exit(1);
  }
  if(info == NULL) {
    fprintf(stderr, "Error allocating memory.\n");
    exit(1);
  }

  // Save oiginal setting
  savetty();

  // Initialize map and info message
  initGame(board_arry);
  REMAIN_N = N;

  initscr();
  int physicalW = W*STEP_X+1;
  int physicalH = H*STEP_Y+1;
  board_win = subwin(stdscr, physicalH, physicalW, 0, 0);

  // Initialize color pairs
  initColorPairs();

  // Can't crate new window
  if(board_win == NULL) {
    fprintf(stderr, "Can't create subwindow. Try a smaller map or scale the terminal.\n");
    resetTerminal();
    return 1;
  }

  // Draw the physical board
  printBoard(physicalH, physicalW);

  // Print controls and the information line
  updateInfo();
  mvprintw(H*STEP_Y+2, 0, control);

  // Move cursor to first square
  wmove(board_win, 1, 2);
  X = 0;
  Y = 0;

  touchwin(stdscr);
  refresh();

  // Initialize keypad
  initKeypad();

  // Clean up
  delwin(board_win);
  resetty();
  echo();
  endwin();
  free(info);
  free(board_arry);
  free(temp_arry);
  exit(EXIT_SUCCESS);
}

void resetTerminal() {
  delwin(board_win);
  resetty();
  echo();
  endwin();
  exit(EXIT_FAILURE);
}

void redrawScreen() {
  printBoard(H*STEP_Y+1, W*STEP_X+1);
  updateInfo();
  mvprintw(H*STEP_Y+2, 0, control);
  touchwin(board_win);
  wrefresh(board_win);
  refresh();
}

// Print board with specified row and col
void printBoard(int physicalH, int physicalW) {
  int i, j, k;
  bool horizontal = true;
  int tempIdx;
  unsigned char tempVal;
  chtype printChar;

  for(i = 0; i < physicalH; i++) {
    for(j = 0; j < physicalW; j++) {
      // Horizontal border
      if(horizontal) {
        if(i == 0) {
          // First border
          if(j == 0) printChar = ACS_ULCORNER;
          else if(j == physicalW-1) printChar = ACS_URCORNER;
          else if(j % STEP_X== 0) printChar = ACS_TTEE;
          else printChar = ACS_HLINE;
        } else if(i == physicalH-1) {
          // Last border
          if(j == 0) printChar = ACS_LLCORNER;
          else if(j == physicalW-1) printChar = ACS_LRCORNER;
          else if(j % STEP_X == 0) printChar = ACS_BTEE;
          else printChar = ACS_HLINE;
        } else {
          // In the middle
          if(j == 0) printChar = ACS_LTEE;
          else if(j == physicalW-1) printChar = ACS_RTEE;
          else if(j % STEP_X == 0) printChar = ACS_PLUS;
          else printChar = ACS_HLINE;
        }
      } else {
        if(j % (CW-1) == 0) printChar = ACS_VLINE;
        else if(j % STEP_X == 2) {
          // This is a square
          tempIdx = (i/STEP_Y)*W+(j/STEP_X);
          tempVal = board_arry[tempIdx];
          if(tempVal & SHOW_MASK) {
            if((tempVal & VALUE_MASK) == MINE) {
              if(COL_SUPP) wattron(board_win, A_BOLD);
              mvwaddch(board_win, i, j, MINE);
              if(COL_SUPP) wattroff(board_win, A_BOLD);
            } else if((tempVal & VALUE_MASK) == ZERO) {
              mvwaddch(board_win, i, j, ' ');
            } else {
              if(COL_SUPP) wattrset(board_win, COLOR_PAIR((tempVal & VALUE_MASK) - ZERO));
              mvwaddch(board_win, i, j, (tempVal & VALUE_MASK));
              if(COL_SUPP) wattrset(board_win, COLOR_PAIR(DEFAULT_C));
            }
          } else if(tempVal & FLAG_MASK) {
            if(COL_SUPP) wattron(board_win, A_BOLD);
            mvwaddch(board_win, i, j, FLAG);
            if(COL_SUPP) wattroff(board_win, A_BOLD);
          } else {
            mvwaddch(board_win, i, j, ACS_CKBOARD);
          }
          continue;
        } else printChar = ' ';
      }
      mvwaddch(board_win, i, j, printChar);
    }

    horizontal = !horizontal;
  }
}

void initGame() {
  // Initialize to character 0
  memset(board_arry, ZERO, H*W);

  // Generate random mines
  int randIdx, randX, randY;
  srand(time(NULL));
  for(int i = 0; i < N; i++) {
    // Test for overlapping mines
    do {
      randIdx = rand() % (W*H);
    } while(board_arry[randIdx] == MINE);

    board_arry[randIdx] = MINE;
    randX = randIdx % W;
    randY = randIdx / W;

    // Update left 3 squares
    if(randX > 0) {
      if(randY > 0) {
        if(board_arry[randIdx-W-1] != MINE) board_arry[randIdx-W-1]++;
      }
      if(randY < H-1) {
        if(board_arry[randIdx+W-1] != MINE) board_arry[randIdx+W-1]++;
      }
      if(board_arry[randIdx-1] != MINE) board_arry[randIdx-1]++;
    }

    // Update right 3 squares
    if(randX < W-1) {
      if(randY > 0) {
        if(board_arry[randIdx-W+1] != MINE) board_arry[randIdx-W+1]++;
      }
      if(randY < H-1) {
        if(board_arry[randIdx+W+1] != MINE) board_arry[randIdx+W+1]++;
      }
      if(board_arry[randIdx+1] != MINE) board_arry[randIdx+1]++;
    }

    // Update top and bottom squares
    if(randY > 0) {
      if(board_arry[randIdx-W] != MINE) board_arry[randIdx-W]++;
    }
    if(randY < H-1) {
      if(board_arry[randIdx+W] != MINE) board_arry[randIdx+W]++;
    }
  }

  // Debug
  /*for(int i = 1; i <= H*W; i++) {
    printf("%c ", board_arry[i-1]);
    if(i % W == 0) printf("\n");
    }
    sleep(2);
  */
}

void initKeypad() {
  // Initailize keypad
  crmode();
  keypad(board_win, TRUE);
  noecho();
  int key;

  do {
    key = wgetch(board_win);
    if(GAME_END) {
      if(!GAME_END_PRESS--) break;
    }
    switch(key) {
      case KEY_LEFT:
      case 'a':
      case 'A':
        if(X-1 >= 0) X--;
        break;
      case KEY_RIGHT:
      case 'd':
      case 'D':
        if(X+1 < W) X++;
        break;
      case KEY_UP:
      case 'w':
      case 'W':
        if(Y-1 >= 0) Y--;
        break;
      case KEY_DOWN:
      case 's':
      case 'S':
        if(Y+1 < H) Y++;
        break;
      case 'f':
      case 'F':
        flagMine(board_win);
        break;
      case ' ':
        testMine(board_win);
        break;
      case 'r':
      case 'R':
        redrawScreen();
        break;
      /*case 'i':    <- Another cheat
        sprintf(info, "Curr: %i", board_arry[Y*W+X]);
        printInfo();
        break;*/
      default:
        break;
    }

    // Update cursor position
    wmove(board_win, Y*STEP_Y+1, X*STEP_X+2);
    touchwin(stdscr);
    refresh();
    // printf("%d-%d\n", X*STEP_X+2, Y*STEP_Y+1);
  } while(key != ERR && key != 'q' && key != 'Q');

  resetty();
  echo();
}

bool testMine() {
  int idx = Y*W+X;
  if((board_arry[idx] & SHOW_MASK) 
      || (board_arry[idx] & FLAG_MASK)) {
    // This value is already shown or is flaged
    return false;
  } else if((board_arry[idx] & VALUE_MASK) == MINE) {
    // Oops, step over a mine
    gameOver(board_win);
    return true;
  } else {
    // If click number, just show itself
    if((board_arry[idx] & VALUE_MASK) != ZERO) {
      if(COL_SUPP) wattrset(board_win, COLOR_PAIR((board_arry[idx] 
              & VALUE_MASK) - ZERO));
      addChar(Y, X, board_arry[idx] & VALUE_MASK);
      board_arry[idx] |= SHOW_MASK;
      if(COL_SUPP) wattrset(board_win, COLOR_PAIR(DEFAULT_C));
      return false;
    }

    // Else find adjacent empty spaces and reveal them
    int counter = 0, i;
    int currX, currY, tempX, tempY;
    unsigned char *tempPtr;

    // Expand on empty spaces
    temp_arry[counter++] = Y;
    temp_arry[counter++] = X;
    while(counter > 0) {
      currX = temp_arry[--counter];
      currY = temp_arry[--counter];

      // Expand size
      if(counter + 10 >= tempSize) {
        tempSize *= 2;
        tempPtr = realloc(temp_arry, sizeof(unsigned char)*tempSize);
        if(tempPtr == NULL) {
          fprintf(stderr, "Can't allocate more space.\n");
          free(board_arry);
          free(temp_arry);
          free(info);
          resetTerminal();
          exit(1);
        }
        temp_arry = tempPtr;
        // fprintf(stderr, "Currsize: %d", tempSize);
      }

      // Show this square as empty
      addChar(currY, currX, ' ');
      board_arry[currY*W+currX] |= SHOW_MASK;

      // Update left 3 adjacent squares
      tempX = currX-1; tempY = currY-1;
      if(tempX >= 0) {
        for(i = 0; i < 3; i++, tempY++) {
          if(tempY < 0 || tempY >= H) continue;
          if(showSquare(tempY, tempX)) {
            temp_arry[counter++] = tempY;
            temp_arry[counter++] = tempX;
          }
        }
      }

      // Update bottom square
      tempX = currX; tempY = currY+1;
      if(tempY < H && showSquare(tempY, tempX)) {
        temp_arry[counter++] = tempY;
        temp_arry[counter++] = tempX;
      }

      // Update right 3 adjacent squares
      tempX = currX+1; tempY = currY-1;
      if(tempX < W) {
        for(i = 0; i < 3; i++, tempY++) {
          if(tempY < 0 || tempY >= H) continue;
          if(showSquare(tempY, tempX)) {
            temp_arry[counter++] = tempY;
            temp_arry[counter++] = tempX;
          }
        }
      }

      // Update top square
      tempX = currX; tempY = currY-1;
      if(tempY >= 0 && showSquare(tempY, tempX)) {
        temp_arry[counter++] = tempY;
        temp_arry[counter++] = tempX;       
      }
    }

    // Reset color
    if(COL_SUPP) wattrset(board_win, COLOR_PAIR(DEFAULT_C));
  }

  return false;
}

// Show a square if valid and return whether its a empty square or not
bool showSquare(int currY, int currX) {
  int tempIdx = currY*W+currX;
  unsigned char tempVal = board_arry[tempIdx];
  if(tempIdx < 0 || tempIdx >= H*W) {
    // For debug
    fprintf(stderr, "Invalid read %d, %d\n", currY, currX);
  }

  if(!(tempVal & SHOW_MASK) && !(tempVal & FLAG_MASK)) {
    if((tempVal & VALUE_MASK) != MINE) {
      if((tempVal & VALUE_MASK) != ZERO) {
        if(COL_SUPP) wattrset(board_win, COLOR_PAIR((tempVal & VALUE_MASK) - ZERO));
        addChar(currY, currX, (tempVal & VALUE_MASK));
        board_arry[tempIdx] |= SHOW_MASK;
      } else return true;
    }
  }

  // Reset color
  if(COL_SUPP) wattrset(board_win, COLOR_PAIR(DEFAULT_C));

  return false;
}

void addChar(int y, int x, chtype c) {
  mvwaddch(board_win, y*STEP_Y+1, x*STEP_X+2, c);
}

void printInfo() {
  move(H*STEP_Y+1, 0);
  clrtoeol();
  printw(info);
  touchwin(stdscr);
  refresh();
}

void flagMine() {
  int tempIdx = Y*W+X;
  if(!(board_arry[tempIdx] & SHOW_MASK)) {
    // Toggle flag
    if(board_arry[tempIdx] & FLAG_MASK) {
      if(COL_SUPP) wattroff(board_win, A_BOLD);
      addChar(Y, X, ACS_CKBOARD);
      board_arry[tempIdx] ^= FLAG_MASK;
      REMAIN_N++;

      // If unflag a mine
      if((board_arry[tempIdx] & VALUE_MASK) == MINE) CORRECT_N--;
    } else {
      if(COL_SUPP) wattron(board_win, A_BOLD);
      addChar(Y, X, FLAG);
      board_arry[tempIdx] |= FLAG_MASK;
      REMAIN_N--;

      // If correctly flag a mine
      if((board_arry[tempIdx] & VALUE_MASK) == MINE) CORRECT_N++;
    }
  }

  updateInfo();

  // Reset bold font
  if(COL_SUPP) wattroff(board_win, A_BOLD);

  // Test win condition
  if(CORRECT_N == N && REMAIN_N == 0) {
    sleep(0.5);
    gameWin();
  }
}

void updateInfo() {
  sprintf(info, infoFormat, REMAIN_N, CORRECT_N);
  printInfo();
}

void showAll() {
  for(int i = 0; i < W*H; i++) {
    if((board_arry[i] & FLAG_MASK) != FLAG_MASK) {
      if((board_arry[i] & VALUE_MASK) != ZERO) {
        if(COL_SUPP) {
          if((board_arry[i] & VALUE_MASK) != MINE)
            wattrset(board_win, COLOR_PAIR(board_arry[i] & VALUE_MASK - ZERO));
          else
            wattrset(board_win, (DEFAULT_C | A_BOLD));
        }
        addChar(i/W, i%W, board_arry[i] & VALUE_MASK);
      } else {
        if(COL_SUPP) wattrset(board_win, COLOR_PAIR(DEFAULT_C));
        addChar(i/W, i%W, ' ');
      }
    }
  }

  // Reset color
  if(COL_SUPP) wattrset(board_win, COLOR_PAIR(DEFAULT_C));
}

void gameWin() {
  showAll();
  GAME_END = true;
  GAME_END_PRESS = 1;

  // Inform the player
  sprintf(info, "Congrats! You survived the minefield!");
  printInfo();

  touchwin(stdscr);
  refresh();
}

void gameOver() {
  showAll();
  GAME_END = true;
  GAME_END_PRESS = 1;

  // Inform the player
  sprintf(info, "You triggered a mine! Better luck next time!");
  printInfo();

  touchwin(stdscr);
  refresh();
}

void initColorPairs() {
  // If terminal supports color
  if(has_colors()) {
    use_default_colors();
    if(start_color() != OK) return;
    COL_SUPP = true;
    init_pair(1, COLOR_GREEN, -1);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_CYAN, -1);
    init_pair(4, COLOR_MAGENTA, -1);
    init_pair(5, COLOR_RED, -1);
    init_pair(6, COLOR_RED, -1);
    init_pair(7, COLOR_RED, -1);
    init_pair(8, COLOR_RED, -1);
    init_pair(9, COLOR_RED, -1);
    init_pair(10, COLOR_WHITE, -1);
  }
}
