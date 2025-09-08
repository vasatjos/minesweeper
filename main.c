#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

struct termios original_settings;

void disableCanonical() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_settings);
}

void enableCanonical() {
    tcgetattr(STDIN_FILENO, &original_settings);
    atexit(disableCanonical);

    struct termios canonical = original_settings;

    canonical.c_lflag &= ~(ECHO | ICANON);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &canonical);
}

#define MAX_MINE_PERCENTAGE 50

typedef enum { EMPTY, MINE } Cell;
typedef enum { OPEN, CLOSED, FLAGGED } CellState;

typedef struct {
    size_t rows, cols;
    Cell *cells;
    CellState *states;
    size_t cursorRow, cursorCol;
    size_t numMines, numClosed;
} Field;

Field fieldCreate() {
    Field field;
    field.rows = field.cols = 0;
    field.cursorRow = field.cursorCol = 0;
    field.cells = NULL;
    field.states = NULL;
    field.numMines = 0;
    field.numClosed = 0;
    return field;
}

void freeField(Field *const field) {
    free(field->cells);
    free(field->states);
    field->cells = NULL;
    field->states = NULL;
}

void fieldResize(Field *const field, const size_t rows, const size_t cols) {
    freeField(field);
    field->cells = (Cell *)malloc(sizeof(Cell) * rows * cols);
    field->states = (CellState *)malloc(sizeof(CellState) * rows * cols);
    field->rows = rows;
    field->cols = cols;
    for (size_t i = 0; i < rows * cols; i++) {
        field->cells[i] = EMPTY;
        field->states[i] = CLOSED;
    }
    field->numClosed = rows * cols;
}

Cell *cellAtIndex(const Field field, const size_t row, const size_t col) {
    if (row >= field.rows || col >= field.cols) {
        printf("ERROR: Index out of bounds.\n");
        exit(1);
    }
    return &field.cells[row * field.cols + col];
}

CellState *stateAtIndex(const Field field, const size_t row, const size_t col) {
    if (row >= field.rows || col >= field.cols) {
        printf("ERROR: Index out of bounds.\n");
        exit(1);
    }
    return &field.states[row * field.cols + col];
}

bool isAtCursor(const Field field, const size_t row, const size_t col) {
    return field.cursorRow == row && field.cursorCol == col;
}

void generateMines(Field *const field, const size_t minePercentage) {
    if (minePercentage > MAX_MINE_PERCENTAGE) {
        printf("ERROR: Mine percentage too high.\n");
        exit(1);
    }
    field->numMines = field->rows * field->cols * minePercentage / 100;
    size_t row = 0, col = 0;
    Cell *cell;

    for (size_t i = 0; i < field->rows * field->cols; i++)
        field->cells[i] = EMPTY;

    for (size_t i = 0; i < field->numMines; i++) {
        do {
            row = rand() % field->rows;
            col = rand() % field->cols;
        } while (*(cell = cellAtIndex(*field, row, col)) == MINE);
        *cell = MINE;
    }
}

Cell openAtCursor(Field *const field) {
    CellState *state = stateAtIndex(*field, field->cursorRow, field->cursorCol);
    if (*state == CLOSED) {
        *state = OPEN;
        field->numClosed--;
        return *cellAtIndex(*field, field->cursorRow, field->cursorCol);
    }
    return EMPTY;
}

void flagAtCursor(const Field field) {
    CellState *state = stateAtIndex(field, field.cursorRow, field.cursorCol);
    if (*state == CLOSED) {
        *state = FLAGGED;
        return;
    }
    if (*state == FLAGGED)
        *state = CLOSED;
}

size_t countNeighborMines(const Field field, const size_t row,
                          const size_t col) {
    size_t mines = 0;
    for (int row_delta = -1; row_delta <= 1; row_delta++) {
        for (int col_delta = -1; col_delta <= 1; col_delta++) {
            if (row_delta == 0 && col_delta == 0) // don't count middle square
                continue;
            size_t curr_row = row + row_delta;
            size_t curr_col = col + col_delta;
            if (curr_col >= field.cols || curr_row >= field.rows)
                continue; // size_t type handles both under and overflow
            if (*cellAtIndex(field, curr_row, curr_col) == MINE)
                mines++;
        }
    }
    return mines;
}

void printOpenTile(const Field field, const size_t row, const size_t col) {
    if (*cellAtIndex(field, row, col) == MINE) {
        printf("@");
        return;
    }
    size_t neighboringMines = countNeighborMines(field, row, col);
    if (neighboringMines)
        printf("%zu", neighboringMines);
    else
        printf(" ");
}

void printField(const Field field) {
    for (size_t r = 0; r < field.rows; r++) {
        for (size_t c = 0; c < field.cols; c++) {
            if (isAtCursor(field, r, c))
                printf("[");
            else
                printf(" ");
            switch (*stateAtIndex(field, r, c)) {
            case FLAGGED:
                printf("\033[31mF\033[0m");
                break;
            case CLOSED:
                printf(".");
                break;
            case OPEN:
                printOpenTile(field, r, c);
                break;
            default:
                printf("ERROR: Invalid cell state.\n");
                exit(1);
            }
            if (isAtCursor(field, r, c))
                printf("]");
            else
                printf(" ");
        }
        printf("\n");
    }
}

void revealMines(Field *const field) {
    for (size_t i = 0; i < field->rows * field->cols; i++) {
        if (field->cells[i] == MINE)
            field->states[i] = OPEN;
    }
}

void generateAtCursor(Field *const field, const size_t minePercentage) {
    bool hasMineNeighbors;
    bool isMine;
    do {
        generateMines(field, minePercentage);
        hasMineNeighbors =
            countNeighborMines(*field, field->cursorRow, field->cursorCol) != 0;
        isMine =
            *cellAtIndex(*field, field->cursorRow, field->cursorCol) == MINE;
    } while (hasMineNeighbors || isMine);
}

bool performAction(Field *const field, const char action,
                   const size_t minePercentage, bool *first) {
    switch (action) {
    case 'w':
        if (field->cursorRow > 0)
            field->cursorRow--;
        break;
    case 's':
        if (field->cursorRow < field->rows - 1)
            field->cursorRow++;
        break;
    case 'a':
        if (field->cursorCol > 0)
            field->cursorCol--;
        break;
    case 'd':
        if (field->cursorCol < field->cols - 1)
            field->cursorCol++;
        break;
    case ' ':
        if (*first) {
            generateAtCursor(field, minePercentage);
            *first = false;
        }
        if (openAtCursor(field) == MINE) {
            revealMines(field);
            return false;
        }
        break;
    case 'f':
        flagAtCursor(*field);
        break;
    default:
        break;
    }
    if (field->numClosed == field->numMines)
        return false;
    return true;
}

bool isMineOpen(const Field field) {
    for (size_t i = 0; i < field.rows * field.cols; i++)
        if (field.cells[i] == MINE && field.states[i] == OPEN)
            return true;
    return false;
}

void printControls() {
    printf("\n------ MINESWEEPER ------\n");
    printf("Move: W, S, A, D\n");
    printf("Open a field: <SPACE>\n");
    printf("Flag a suspected mine: F\n");
    printf("-------------------------\n\n");
}

void printResult(const Field field) {
    printField(field);
    printf("\n");
    if (isMineOpen(field))
        printf("OOPS! You lost...\n");
    else
        printf("\033[32mCongratulations, you win!\033[0m\n");
}

void runGame(Field *const field) {
    const size_t ROWS = 10;
    const size_t COLS = 10;
    const size_t MINE_PERCENTAGE = 20;

    fieldResize(field, ROWS, COLS);

    bool running = true;
    bool first = true;
    char action = 0;
    while (running) {
        printField(*field);
        read(STDIN_FILENO, &action, 1);
        action = tolower(action);
        running = performAction(field, action, MINE_PERCENTAGE, &first);
        printf("%c[%zuA", 27, field->rows);
        printf("%c[%zuD", 27, field->cols * 3);
    }
}

int main(void) {
    enableCanonical();
    srand(time(NULL));

    printControls();

    Field field = fieldCreate();
    runGame(&field);
    printResult(field);

    freeField(&field);
    return EXIT_SUCCESS;
}
