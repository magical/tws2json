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
 * Print the given direction to a buffer.
 *
 * @returns 0 on success. 1 on failure.
 */
int printdir1(char **buf, int dir, int i)
{
	switch (dir) {
	case NORTH: *(*buf)++ = 'u'; break;
	case WEST:  *(*buf)++ = 'l'; break;
	case SOUTH: *(*buf)++ = 'd'; break;
	case EAST:  *(*buf)++ = 'r'; break;
	case NORTH|WEST: *(*buf)++ = 'u'; *(*buf)++ = '+'; *(*buf)++ = 'l'; break;
	case NORTH|EAST: *(*buf)++ = 'u'; *(*buf)++ = '+'; *(*buf)++ = 'r'; break;
	case SOUTH|WEST: *(*buf)++ = 'd'; *(*buf)++ = '+'; *(*buf)++ = 'l'; break;
	case SOUTH|EAST: *(*buf)++ = 'd'; *(*buf)++ = '+'; *(*buf)++ = 'r'; break;
	default:
		errmsg("error", "Can't encode move %d (%d)", i, dir);
		return 1;
	}
	return 0;
}

/**
 * Convert a list of moves to a textual representation.
 *
 * @returns 0 on success. 1 on failure.
 */
int compressjsonsolution(actlist *moves, int solutiontime, char *buf) {
	action const   *move;
	int i, when, delta;
	int lastdir = NIL;

	when = 0;
	for (i = 0; i < moves->count; i++) {
		move = &moves->list[i];

		if (i == 0) {
			delta = 1;
		} else {
			// ticks between the previous move and the current move.
			delta = move->when - when;
		}

		if (delta <= 0) {
			errmsg("error", "bad delta %d, move %d", delta, i);
			return 1;
		} else if (1 < delta) {
			buf += sprintf(buf, "%d,", delta - 1);
		}

		if (printdir1(&buf, move->dir, i)) {
			return 1;
		}

		when = move->when;
		lastdir = move->dir;
	}
	if (when < solutiontime) {
		buf += sprintf(buf, "%d,", solutiontime - when - 1);
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
			if (compressjsonsolution(&solution.moves, solution.besttime, movebuf)) {
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
