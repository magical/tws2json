/* tws2json.c: Convert a Tile World solution file to a JSON format.
 * 
 * Copyright © 2011 by Andrew Ekstedt, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include <stdio.h>
#include <stddef.h>
#include "solution.h"
#include "fileio.h"
#include "err.h"

const char *ruleset_names[] = {
	"",
	"lynx", /* Ruleset_Lynx */
	"ms", /* Ruleset_MS */
};

/**
 * Convert a list of moves to a textual representation.
 *
 * @returns 0 on success. 1 on failure.
 */
int compressjsonsolution(actlist *moves, char *buf) {
	action const  *move;
	int i, when, delta;

	when = 0;
	for (i = 0; i < moves->count; i++) {
		move = &moves->list[i];

		delta = move->when - when;

		if (when) {
			if (delta <= 0) {
				errmsg("error", "bad delta %d, move %d", delta, i);
				return 1;
			} else if (delta == 1) {
				*buf++ = ',';
			} else if (delta == 4) {
			} else if (delta == 8) {
				*buf++ = '.';
			} else if (delta % 4 == 0) {
				buf += sprintf(buf, "%d.", delta / 4);
			} else {
				buf += sprintf(buf, "%d,", delta);
			}
		}

		when = move->when;

		switch (move->dir) {
		case NORTH: *buf++ = 'U'; break;
		case WEST:  *buf++ = 'L'; break;
		case SOUTH: *buf++ = 'D'; break;
		case EAST:  *buf++ = 'R'; break;
		case NORTH|WEST: *buf++ = 'U'; *buf++ = 'l'; break;
		case NORTH|EAST: *buf++ = 'U'; *buf++ = 'r'; break;
		case SOUTH|WEST: *buf++ = 'D'; *buf++ = 'l'; break;
		case SOUTH|EAST: *buf++ = 'D'; *buf++ = 'r'; break;
		default:
			errmsg("error", "Can't encode move %d (%d)", i, move->dir);
			return 1;
		}
	}
	*buf++ = '\0';
	return 0;
}

int main(int argc, char *argv[])
{
	// read the solution file
	int ruleset;
	int flags;
	int extrasize;
	solutioninfo solution;
	fileinfo file;

	unsigned char extra[256];
	char movebuf[8192]; // XXX

	if (argc < 2) {
		return 1;
	}
	if (!fileopen(&file, argv[1], "rb", "file error")) {
		return 1;
	}

	if (!readsolutionheader(&file, &ruleset, &flags, &extrasize, extra)) {
		return 1;
	}

	// write json header
	printf("{\"class\":\"tws\",\n"
	       " \"ruleset\":\"%s\",\n"
	       " \"levelset\":\"%.*s\",\n"
	       " \"solutions\":[\n",
	       ruleset_names[ruleset],
	       extrasize, extra);

	solution.moves.allocated = 0;
	solution.moves.list = NULL;
	while (readsolution(&file, &solution)) {
		if (!solution.number) {
			continue;
		}
		// write json level
		if (!solution.moves.count) {
			//just the number and password
			printf("  {\"class\":\"solution\",\n"
			       "   \"number\":%d,\n"
			       "   \"password\":\"%.4s\"}",
			       solution.number,
			       solution.passwd);
		} else {
			if (compressjsonsolution(&solution.moves, movebuf)) {
				continue;
			}
			printf("  {\"class\":\"solution\",\n"
			       "   \"number\":%d,\n"
			       "   \"password\":\"%s\",\n"
			       "   \"rndslidedir\":%d,\n"
			       "   \"stepping\":%d,\n"
			       "   \"rndseed\":%lu,\n"
			       "   \"moves\":\"%s\"}",
			       solution.number,
			       solution.passwd,
			       solution.rndslidedir,
			       solution.stepping,
			       solution.rndseed,
			       movebuf);
		}
		printf(",\n");
		fflush(stdout);
	}
	printf("]}\n");

	return 0;
}
