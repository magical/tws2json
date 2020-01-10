#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "err.h"
#include "solution.h"

bool myisdigit(int c) { return '0' <= c && c <= '9'; }
bool isuppermove(int c) { return c == 'U' || c == 'D' || c == 'L' || c == 'R'; }
bool islowermove(int c) { return c == 'u' || c == 'd' || c == 'l' || c == 'r'; }
bool iswait(int c) { return c == '.' || c == ','; }
bool ismouse(int c) { return c == '*'; }

// states
enum parser_state {
        error,
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

int letter2move(int c) {
    switch (c) {
    case 'U': return NORTH;
    case 'D': return SOUTH;
    case 'L': return WEST;
    case 'R': return EAST;
    case 'u': return NORTH;
    case 'd': return SOUTH;
    case 'l': return WEST;
    case 'r': return EAST;
    }
    return -1;
}

void emitmove(actlist* moves, long* time, int count, int move, int duration) {
    action a;
    int i;
    for (i = 0; i < count; i++) {
        a.when = *time;
        a.dir = move;
        addtomovelist(moves, a);
        *time += duration;
    }
}

void emitwait(actlist* moves, long* time, int count, int duration) {
    *time += count * duration;
}

void emitmouse(actlist* moves, long* time, int x, int y) {
    int move = 16 + ((y - MOUSERANGEMIN) * MOUSERANGE) + (x - MOUSERANGEMIN);
    action a;
    a.when = *time;
    a.dir = move;
    addtomovelist(moves, a);
    *time += 1;
}

int parsemoves(actlist* moves, const char* movestring, int len) {
    enum parser_state state = new_move;
    long time = 0;
    int count;
    int move;
    int duration;
    int x, y, digit, dir;
    int pos;

    initmovelist(moves);

    for (pos = 0;;) {
        int c;

        pos += 1;
        if (pos < len) {
            c = movestring[pos];
        } else {
            c = EOF;
        }

    retry:
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
                state = mouse;
            } else if (isuppermove(c)) {
                move = letter2move(c);
                duration = 4;
                state = moveupper;
            } else if (islowermove(c)) {
                move = letter2move(c);
                duration = 1;
                state = movelower;
            } else if (iswait(c)) {
                if (c == '.') {
                    duration = 4;
                } else {
                    duration = 1;
                }
                emitwait(moves, &time, count, duration);
                state = new_move;
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
                move = letter2move(c);
                duration = 4;
                state = moveupper;
            } else if (islowermove(c)) {
                move = letter2move(c);
                duration = 1;
                state = movelower;
            } else if (iswait(c)) {
                if (c == '.') {
                    duration = 4;
                } else {
                    duration = 1;
                }
                emitwait(moves, &time, count, duration);
                state = new_move;
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
                emitmove(moves, &time, count, move, duration);
                state = new_move;
                goto retry;
            }
            break;

//    movelower -> diagonallower [label="+"];
//    movelower -> end_move;
        case movelower:
            if (c == '+') {
                state = diagonallower;
            } else {
                emitmove(moves, &time, count, move, duration);
                state = new_move;
                goto retry;
            }
            break;

//    diagonalupper -> new_move [label="UDLR", style=bold];
        case diagonalupper:
            if (isuppermove(c)) {
                move |= letter2move(c);
                emitmove(moves, &time, count, move, duration);
                state = new_move;
            } else {
                state = error;
            }
            break;

//    diagonallower -> new_move [label="udlr", style=bold];
        case diagonallower:
            if (islowermove(c)) {
                move |= letter2move(c);
                emitmove(moves, &time, count, move, duration);
                state = new_move;
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
            digit = 0;
            dir = 0;
            if (c == '.') {
                emitmouse(moves, &time, 0, 0);
                state = new_move;
            } else if (myisdigit(c)) {
                digit = c - '0';
                state = mouse1a;
            } else if (isuppermove(c)) {
                dir = c;
                switch (dir) {
                case 'U': y = -1; break;
                case 'D': y = +1; break;
                case 'L': x = -1; break;
                case 'R': x = +1; break;
                }
                state = mouse1b;
            } else {
                state = error;
            }
            break;

//    mouse1a -> mouse1b [label="UDLR"];
        case mouse1a:
            if (isuppermove(c)) {
                dir = c;
                switch (dir) {
                case 'U': y = -digit; break;
                case 'D': y = +digit; break;
                case 'L': x = -digit; break;
                case 'R': x = +digit; break;
                }
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
                emitmouse(moves, &time, x, y);
                state = new_move;
                goto retry;
            }
            break;

//    mouse2a -> mouse2b [label="2-9"];
//    mouse2a -> new_move [label="UDLR", style=bold];
        case mouse2a:
            if (myisdigit(c)) {
                digit = c - '0';
                state = mouse2b;
            } else if (isuppermove(c)) {
                digit = 1;
                if ((dir == 'U' || dir == 'D') && (c == 'U' || c == 'D')) {
                    state = error;
                } else if ((dir == 'L' || dir == 'R') && (c == 'L' || c == 'R')) {
                    state = error;
                } else {
                    dir = c;
                    switch (dir) {
                    case 'U': y = -digit; break;
                    case 'D': y = +digit; break;
                    case 'L': x = -digit; break;
                    case 'R': x = +digit; break;
                    }
                    emitmouse(moves, &time, x, y);
                    state = new_move;
                }
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

        if (c == EOF) {
            break;
        }
    }

    return 0;
}
