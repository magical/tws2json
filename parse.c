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
	StateError,
	StateInit,
	StateMoveCount,
	StateMoveUpper,
	StateMoveLower,
	StateDiagonalUpper,
	StateDiagonalLower,
	StateMouse,
	StateMouse1a,
	StateMouse1b,
	StateMouse2a,
	StateMouse2b,
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

int parsemoves(actlist* moves, long* totaltime, const char* movestring, int len) {
	enum parser_state state = StateInit;
	long time = 0;
	int count;
	int move;
	int duration;
	int x, y, digit, dir;
	int pos;

	initmovelist(moves);

	for (pos = 0;;) {
		int c;

		if (pos < len) {
			c = movestring[pos];
			pos += 1;
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
		case StateInit:
			count = 1;
			if (myisdigit(c)) {
				count = c - '0';
				state = StateMoveCount;
			} else if (ismouse(c)) {
				state = StateMouse;
			} else if (isuppermove(c)) {
				move = letter2move(c);
				duration = 4;
				state = StateMoveUpper;
			} else if (islowermove(c)) {
				move = letter2move(c);
				duration = 1;
				state = StateMoveLower;
			} else if (iswait(c)) {
				if (c == '.') {
					duration = 4;
				} else {
					duration = 1;
				}
				emitwait(moves, &time, count, duration);
				state = StateInit;
			} else if (c == ' ') {
				/* pass */
			} else if (c == EOF) {
				/* pass */
			} else {
				state = StateError;
			}
			break;

		//    digit_or_move -> digit_or_move [label="0-9"];
		//    digit_or_move -> moveupper [label="UDLR"];
		//    digit_or_move -> movelower [label="udlr"];
		//    digit_or_move -> new_move [label=", .", style=bold];
		case StateMoveCount:
			if (myisdigit(c)) {
				count = count*10 + (c - '0');
			} else if (isuppermove(c)) {
				move = letter2move(c);
				duration = 4;
				state = StateMoveUpper;
			} else if (islowermove(c)) {
				move = letter2move(c);
				duration = 1;
				state = StateMoveLower;
			} else if (iswait(c)) {
				if (c == '.') {
					duration = 4;
				} else {
					duration = 1;
				}
				emitwait(moves, &time, count, duration);
				state = StateInit;
			} else {
				state = StateError;
			}
			break;

		//    moveupper -> diagonalupper [label="+"];
		//    movelower -> end_move;
		case StateMoveUpper:
			if (c == '+') {
				state = StateDiagonalUpper;
			} else {
				emitmove(moves, &time, count, move, duration);
				state = StateInit;
				goto retry;
			}
			break;

		//    movelower -> diagonallower [label="+"];
		//    movelower -> end_move;
		case StateMoveLower:
			if (c == '+') {
				state = StateDiagonalLower;
			} else {
				emitmove(moves, &time, count, move, duration);
				state = StateInit;
				goto retry;
			}
			break;

		//    diagonalupper -> new_move [label="UDLR", style=bold];
		case StateDiagonalUpper:
			if (isuppermove(c)) {
				move |= letter2move(c);
				emitmove(moves, &time, count, move, duration);
				state = StateInit;
			} else {
				state = StateError;
			}
			break;

		//    diagonallower -> new_move [label="udlr", style=bold];
		case StateDiagonalLower:
			if (islowermove(c)) {
				move |= letter2move(c);
				emitmove(moves, &time, count, move, duration);
				state = StateInit;
			} else {
				state = StateError;
			}
			break;

		//    mouse -> new_move [label="."];
		//    mouse -> mouse1a [label="2-9"];
		//    mouse -> mouse1b [label="UDLR"];
		case StateMouse:
			x = 0;
			y = 0;
			digit = 0;
			dir = 0;
			if (c == '.') {
				emitmouse(moves, &time, 0, 0);
				state = StateInit;
			} else if (myisdigit(c)) {
				digit = c - '0';
				state = StateMouse1a;
			} else if (isuppermove(c)) {
				dir = c;
				switch (dir) {
				case 'U': y = -1; break;
				case 'D': y = +1; break;
				case 'L': x = -1; break;
				case 'R': x = +1; break;
				}
				state = StateMouse1b;
			} else {
				state = StateError;
			}
			break;

		//    mouse1a -> mouse1b [label="UDLR"];
		case StateMouse1a:
			if (isuppermove(c)) {
				dir = c;
				switch (dir) {
				case 'U': y = -digit; break;
				case 'D': y = +digit; break;
				case 'L': x = -digit; break;
				case 'R': x = +digit; break;
				}
				state = StateMouse1b;
			} else {
				state = StateError;
			}
			break;

		//    mouse1b -> mouse2a [label=";"];
		//    mouse1b -> end_move;
		case StateMouse1b:
			if (c == ';') {
				state = StateMouse2a;
			} else {
				emitmouse(moves, &time, x, y);
				state = StateInit;
				goto retry;
			}
			break;

		//    mouse2a -> mouse2b [label="2-9"];
		//    mouse2a -> new_move [label="UDLR", style=bold];
		case StateMouse2a:
			if (myisdigit(c)) {
				digit = c - '0';
				state = StateMouse2b;
			} else if (isuppermove(c)) {
				digit = 1;
				if ((dir == 'U' || dir == 'D') && (c == 'U' || c == 'D')) {
					state = StateError;
				} else if ((dir == 'L' || dir == 'R') && (c == 'L' || c == 'R')) {
					state = StateError;
				} else {
					dir = c;
					switch (dir) {
					case 'U': y = -digit; break;
					case 'D': y = +digit; break;
					case 'L': x = -digit; break;
					case 'R': x = +digit; break;
					}
					emitmouse(moves, &time, x, y);
					state = StateInit;
				}
			} else {
				state = StateError;
			}
			break;

		//    mouse2b -> new_move [label="UDLR", style=bold];
		case StateMouse2b:
			if (isuppermove(c)) {
				if ((dir == 'U' || dir == 'D') && (c == 'U' || c == 'D')) {
					state = StateError;
				} else if ((dir == 'L' || dir == 'R') && (c == 'L' || c == 'R')) {
					state = StateError;
				} else {
					dir = c;
					switch (dir) {
					case 'U': y = -digit; break;
					case 'D': y = +digit; break;
					case 'L': x = -digit; break;
					case 'R': x = +digit; break;
					}
					emitmouse(moves, &time, x, y);
					state = StateInit;
				}
			} else {
				state = StateError;
			}
			break;

		case StateError:
			// handled below...
			break;

		default:
			errmsg("internal error", "invalid state while parsing");
			state = StateError;
		}

		if (state == StateError) {
			errmsg("error", "parse error at column %d", pos+1);
			break;
		}

		if (c == EOF) {
			break;
		}
	}

	*totaltime = time - 1;
	return 0;
}
