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

typedef struct jsoncompressinfo {
    char   *buf;
    size_t	bufpos;
    size_t	bufsize;

    action	lastmove;

    int	lastmovedir;
    int	lastmoveduration;

    int	rlemovedir;
    int	rlemoveduration;
    int	rlecount;
} jsoncompressinfo;

int jsoncompress_init(jsoncompressinfo *self);
int jsoncompress_flush(jsoncompressinfo *self);
int jsoncompress_finish(jsoncompressinfo *self, int solutiontime);
int jsoncompress_addmove(jsoncompressinfo *self, action move, int i);
int jsoncompress_rle_add(jsoncompressinfo *self, int dir, int duration);
int jsoncompress_rle_flush(jsoncompressinfo *self);

/**
 * Print the given direction to the movestring buffer.
 *
 * @param duration 1 or 4
 * @returns 0 on success. -1 on failure.
 */
int printdir(jsoncompressinfo *self, int dir, int duration)
{
    char *buf = self->buf + self->bufpos;

    if (duration == 1) {
	switch (dir) {
	case NORTH: *buf++ = 'u'; break;
	case WEST:  *buf++ = 'l'; break;
	case SOUTH: *buf++ = 'd'; break;
	case EAST:  *buf++ = 'r'; break;
	case NORTH|WEST: *buf++ = 'u'; *buf++ = '+'; *buf++ = 'l'; break;
	case NORTH|EAST: *buf++ = 'u'; *buf++ = '+'; *buf++ = 'r'; break;
	case SOUTH|WEST: *buf++ = 'd'; *buf++ = '+'; *buf++ = 'l'; break;
	case SOUTH|EAST: *buf++ = 'd'; *buf++ = '+'; *buf++ = 'r'; break;
	default: goto unknown;
	}
    } else if (duration == 4) {
	switch (dir) {
	case NORTH: *buf++ = 'U'; break;
	case WEST:  *buf++ = 'L'; break;
	case SOUTH: *buf++ = 'D'; break;
	case EAST:  *buf++ = 'R'; break;
	case NORTH|WEST: *buf++ = 'U'; *buf++ = '+'; *buf++ = 'L'; break;
	case NORTH|EAST: *buf++ = 'U'; *buf++ = '+'; *buf++ = 'R'; break;
	case SOUTH|WEST: *buf++ = 'D'; *buf++ = '+'; *buf++ = 'L'; break;
	case SOUTH|EAST: *buf++ = 'D'; *buf++ = '+'; *buf++ = 'R'; break;
	default: goto unknown;
	}
    }
    
    self->bufpos = buf - self->buf;
    return 0;

unknown:
    errmsg("error", "Unknown direction (%d)", dir);
    self->bufpos = buf - self->buf;
    return -1;
}

int printnum(jsoncompressinfo *self, int num)
{
    self->bufpos += sprintf(self->buf + self->bufpos, "%d", num);
    return 0;
}

/**
 * Initialize a jsoncompressinfo struct.
 */
int jsoncompress_init(jsoncompressinfo *self)
{
    if (self == NULL) {
	return -1;
    }
    self->buf = NULL;
    self->bufpos = 0;
    self->bufsize = 0;

    self->lastmove.dir = NIL;

    self->lastmovedir = NIL;
    self->lastmoveduration = 0; // 1 or 4.

    self->rlemovedir = NIL;
    self->rlemoveduration = 0;
    self->rlecount = 0;

    return 0;
}

// Flush means: get rid of any buffered state; flush all moves to the char buffer; we've got something new coming in the pipeline.
int jsoncompress_flush(jsoncompressinfo *self)
{
    int r = 0;

    jsoncompress_rle_flush(self);

    if (self->lastmovedir != NIL) {
	r = printdir(self, self->lastmovedir, self->lastmoveduration);
    }
    self->lastmovedir = NIL;
    self->lastmoveduration = 0;

    if (r < 0) {
	return r;
    }

    return 0;
}

/**
 *
 * If dir and duration differ from the stored dir and duration, the stored move is flushed.
 *
 * Does nothing if dir is NIL.
 */
int jsoncompress_rle_add(jsoncompressinfo *self, int dir, int duration)
{
    int r = 0;

    if (self == NULL) {
	return -1;
    }

    if (dir == NIL) {
	return 0;
    }

    // If the move is the same as the stored move, just update the count.
    if (self->rlemovedir != NIL) {
	if (self->rlemovedir == dir && self->rlemoveduration == duration) {
	    self->rlecount++;
	    return 0;
	}
    }

    // Otherwise, flush the old move and store the new move.
    r = jsoncompress_rle_flush(self);
    self->rlemovedir = dir;
    self->rlemoveduration = duration;
    self->rlecount = 1;
    if (r) {
	return r;
    }

    return 0;
}

/**
 * Write the stored move to the buffer.
 *
 * Does nothing if no move is stored
 */
int jsoncompress_rle_flush(jsoncompressinfo *self)
{
    int r = 0;

    if (self == NULL) {
	return -1;
    }

    if (self->rlemovedir != NIL) {
	if (1 < self->rlecount) {
	    r = printnum(self, self->rlecount);
	}
	if (0 <= r) {
	    r = printdir(self, self->rlemovedir, self->rlemoveduration);
	}
	self->rlemovedir = NIL;
	self->rlecount = 0;
    }

    if (r < 0) {
	return r;
    }

    return 0;
}

/**
 * Add a move to the stream.
 *
 * @returns -1 on failure.
 */

// The algorithm is pretty simple:
// 1. Expand the incoming stream of actions into a stream of moves.
// 2. Upconvert to 4-moves whenever possible.
// 3. RL-encode.
int jsoncompress_addmove(jsoncompressinfo *self, action move, int i)
{
    int r;
    int delta = 1;

    if (self == NULL) {
	return -1;
    }

    if (0 < i) {
	// the ticks between the previous move and the current move.
	delta = move.when - self->lastmove.when;
    }

    if (delta <= 0) {
	errmsg("error", "move %d: bad delta (%d)", i, delta);
	return -1;
    }

    // Attempt to upconvert previous move
    if (self->lastmovedir != NIL && self->lastmoveduration == 1 && 4 <= delta) {
	self->lastmoveduration = 4;
	delta -= 3;
    }

    // We are now finished monkeying with the previous move, so send it along.
    r = jsoncompress_rle_add(self, self->lastmovedir, self->lastmoveduration);
    self->lastmovedir = NIL;
    if (r < 0) {
	goto end;
    }

    // If we have any delta time left, flush the previous move and write it out.
    if (1 < delta) {
	r = jsoncompress_flush(self);
	if (r < 0) {
	    goto end;
	}
	r = printnum(self, delta - 1);
	self->buf[self->bufpos++] = ',';
    }

end:
    self->lastmove = move;
    self->lastmovedir = move.dir;
    self->lastmoveduration = 1;

    if (r < 0) {
	return r;
    }

    return 0;
}

/**
 * Finish the move stream.
 *
 * You must supply the total solution time, so appropriate waiting can be added.
 */
int jsoncompress_finish(jsoncompressinfo *self, int solutiontime)
{
    int r;

    if (self == NULL) {
	return -1;
    }

    //XXX Attempt to upconvert

    r = jsoncompress_flush(self);
    if (r < 0) {
	return r;
    }

    if (self->lastmove.when < solutiontime) {
	self->bufpos += sprintf(self->buf + self->bufpos, "%d,", 
	                        solutiontime - self->lastmove.when - 1);
    }
    self->buf[self->bufpos++] = '\0';
    return 0;
}

/**
 * Convert a list of moves to a textual representation.
 *
 * @returns 0 on success. 1 on failure.
 */
int compressjsonsolution(actlist *moves, int solutiontime, char *buf)
{
    jsoncompressinfo jsoncompress;
    int i, r;

    r = jsoncompress_init(&jsoncompress);
    if (r < 0) {
	return r;
    }
    jsoncompress.buf = buf;

    for (i = 0; i < moves->count; i++) {
	r = jsoncompress_addmove(&jsoncompress, moves->list[i], i);
	if (r < 0) {
	    return r;
	}
    }
    r = jsoncompress_finish(&jsoncompress, solutiontime);
    if (r < 0) {
	return r;
    }

    //*buf = jsoncompress->buf;
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
