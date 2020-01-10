#include <stddef.h>
#include <stdbool.h>

#include "err.h"
//#include "solution.h"
typedef struct actlist actlist;
extern void initmovelist(actlist *list);

#ifndef EOF
#define EOF -1
#endif


bool myisdigit(int c) { return '0' <= c && c <= '9'; }
bool isuppermove(int c) { return c == 'U' || c == 'D' || c == 'L' || c == 'R'; }
bool islowermove(int c) { return c == 'u' || c == 'd' || c == 'l' || c == 'r'; }
bool iswait(int c) { return c == '.' || c == ','; }
bool ismouse(int c) { return c == '*'; }

// states
enum parser_state {
        error,
        flush,
	new_move,
	digit_or_move,
	mouse,
	mouse1a,
	mouse1b,
	mouse2a,
	mouse2b,
	moveupper,
	movelower,
	diagonalupper,
	diagonallower,
};


int parsemoves(actlist* moves, const char* movestring, int len) {
    int count;
    int move;
    enum parser_state state = new_move;
    int x, y;
    int digit1, digit2;
    int dir1, dir2;

    initmovelist(moves);

    int pos = 0;
    for (;;) {
        int c;

        pos += 1;
        if (pos < len) {
            c = movestring[pos];
        } else {
            c = EOF;
        }

        if (0) {
        flush_and_retry:
            /* TODO: flush move */
            state = new_move;
        }
        switch (state) {

//    new_move -> digit_or_move [label="1-9"];
//    new_move -> mouse [label="*"];
//    new_move -> moveupper [label="UDLR"];
//    new_move -> movelower [label="udlr"];
//    new_move -> new_move [label="<space>"];
        case new_move:
            count = 1;
            if (myisdigit(c)) {
                count = c - '0';
                state = digit_or_move;
            } else if (ismouse(c)) {
                move = '*';
                state = mouse;
            } else if (isuppermove(c)) {
                move = c;
                state = moveupper;
            } else if (islowermove(c)) {
                move = c;
                state = movelower;
            } else if (iswait(c)) {
                move = c;
                state = flush;
            } else if (c == ' ') {
                /* pass */
            } else if (c == EOF) {
                /* pass */
            } else {
                state = error;
            }
            break;
//    digit_or_move -> digit_or_move [label="0-9"];
//    digit_or_move -> moveupper [label="UDLR"];
//    digit_or_move -> movelower [label="udlr"];
//    digit_or_move -> new_move [label=", .", style=bold];
        case digit_or_move:
            if (myisdigit(c)) {
                count = count*10 + (c - '0');
            } else if (isuppermove(c)) {
                move = c;
                state = moveupper;
            } else if (islowermove(c)) {
                move = c;
                state = movelower;
            } else if (iswait(c)) {
                move = c;
                state = flush;
            } else {
                state = error;
            }
            break;

//    moveupper -> diagonalupper [label="+"];
//    movelower -> end_move;
        case moveupper:
            if (c == '+') {
                state = diagonalupper;
            } else {
                goto flush_and_retry;
            }
            break;

//    movelower -> diagonallower [label="+"];
//    movelower -> end_move;
        case movelower:
            if (c == '+') {
                state = diagonallower;
            } else {
                goto flush_and_retry;
            }
            break;

//    diagonalupper -> new_move [label="UDLR", style=bold];
        case diagonalupper:
            if (isuppermove(c)) {
                move += c;
                state = flush;
            } else {
                state = error;
            }
            break;

//    diagonallower -> new_move [label="udlr", style=bold];
        case diagonallower:
            if (islowermove(c)) {
                move += c;
                state = flush;
            } else {
                state = error;
            }
            break;

//    mouse -> new_move [label="."];
//    mouse -> mouse1a [label="2-9"];
//    mouse -> mouse1b [label="UDLR"];
        case mouse:
            x = 0;
            y = 0;
            digit1 = 0;
            digit2 = 0;
            dir1 = 0;
            dir2 = 0;
            move = '*';
            if (c == '.') {
                x = 0;
                y = 0;
                state = flush;
            } else if (myisdigit(c)) {
                digit1 = c - '0';
                state = mouse1a;
            } else if (isuppermove(c)) {
                digit1 = 1;
                dir1 = c;
                state = mouse1b;
            } else {
                state = error;
            }
            break;

//    mouse1a -> mouse1b [label="UDLR"];
        case mouse1a:
            if (isuppermove(c)) {
                dir1 = c;
                state = mouse1b;
            } else {
                state = error;
            }
            break;

//    mouse1b -> mouse2a [label=";"];
//    mouse1b -> end_move;
        case mouse1b:
            if (c == ';') {
                state = mouse2a;
            } else {
                goto flush_and_retry;
            }
            break;

//    mouse2a -> mouse2b [label="2-9"];
//    mouse2a -> new_move [label="UDLR", style=bold];
        case mouse2a:
            if (myisdigit(c)) {
                digit2 = c - '0';
                state = mouse2b;
            } else if (isuppermove(c)) {
                digit2 = 1;
                dir2 = c;
                state = flush;
            } else {
                state = error;
            }
            break;
        default:
            errmsg("internal error", "invalid state while parsing");
        }

        if (state == error) {
            errmsg("error", "parse error at column %d", pos);
            break;
        }

        if (state == flush) {
            /* TODO: append move to actlist */
            state = new_move;
        }

        if (c == EOF) {
            break;
        }
    }

    return 0;
}
