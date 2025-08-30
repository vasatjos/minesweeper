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

typedef enum { EMPTY, MINE } ECell;
typedef enum { OPEN, CLOSED, FLAGGED } ECellState;

typedef struct {
    size_t rows, cols;
    ECell *cells;
    ECellState *states;
    size_t cursorRow, cursorCol;
} Field;

void fieldInit(Field *const field) {
    field->rows = field->cols = 0;
    field->cursorRow = field->cursorCol = 0;
    field->cells = NULL;
    field->states = NULL;
}

void freeField(Field *const field) {
    free(field->cells);
    free(field->states);
    field->cells = NULL;
    field->states = NULL;
}

ECell *cellAtIndex(const Field field, const size_t row, const size_t col) {
    if (row >= field.rows || col >= field.cols) {
        printf("ERROR: Index out of bounds.\n");
        exit(1);
    }
    return &field.cells[row * field.cols + col];
}

ECellState *stateAtIndex(const Field field, const size_t row,
                         const size_t col) {
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
    }
    size_t mineCount = field->rows * field->cols * minePercentage / 100;
    size_t row = 0, col = 0;
    ECell *cell;
    for (size_t i = 0; i < mineCount; i++) {
        do {
            row = rand() % field->rows;
            col = rand() % field->cols;
        } while (*(cell = cellAtIndex(*field, row, col)) == MINE);
        *cell = MINE;
    }
}

ECell openAtCursor(const Field field) {
    ECellState *state = stateAtIndex(field, field.cursorRow, field.cursorCol);
    if (*state == CLOSED)
        *state = OPEN;
    return *cellAtIndex(field, field.cursorRow, field.cursorCol);
}

void flagAtCursor(const Field field) {
    ECellState *state = stateAtIndex(field, field.cursorRow, field.cursorCol);
    if (*state == CLOSED) {
        *state = FLAGGED;
        return;
    }
    if (*state == FLAGGED)
        *state = CLOSED;
}

void fieldResize(Field *const field, const size_t rows, const size_t cols) {
    freeField(field);
    field->cells = (ECell *)malloc(sizeof(ECell) * rows * cols);
    field->states = (ECellState *)malloc(sizeof(ECellState) * rows * cols);
    field->rows = rows;
    field->cols = cols;
    for (size_t i = 0; i < rows * cols; i++) {
        field->cells[i] = EMPTY;
        field->states[i] = CLOSED;
    }
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

void printField(const Field field) {
    for (size_t r = 0; r < field.rows; r++) {
        for (size_t c = 0; c < field.cols; c++) {
            if (isAtCursor(field, r, c))
                printf("[");
            else
                printf(" ");
            switch (*stateAtIndex(field, r, c)) {
            case FLAGGED:
                printf("F");
                break;
            case CLOSED:
                printf(".");
                break;
            case OPEN:
                if (*cellAtIndex(field, r, c) == MINE) {
                    printf("@");
                    break;
                }
                size_t neighboringMines = countNeighborMines(field, r, c);
                if (neighboringMines)
                    printf("%zu", neighboringMines);
                else
                    printf(" ");
                break;
            default:
                printf("ERROR: Inavlid cell state.\n");
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

bool performAction(Field *const field, const char action) {
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
        if (openAtCursor(*field) == MINE) {
            // TODO: Open all mines
            return false;
        }
    case 'f':
        flagAtCursor(*field);
    }
    return true;
}
int main(void) {
    enableCanonical();
    srand(time(NULL));
    const size_t ROWS = 10;
    const size_t COLS = 10;
    const size_t MINE_PERCENTAGE = 20;

    Field field;
    fieldInit(&field);
    fieldResize(&field, ROWS, COLS);
    generateMines(&field, MINE_PERCENTAGE);

    bool running = true;
    char action = 0;
    while (running) {
        printField(field);
        read(STDIN_FILENO, &action, 1);
        action = tolower(action);
        running = performAction(&field, action);
    }
    printField(field);

    freeField(&field);
    return EXIT_SUCCESS;
}
