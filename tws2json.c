/* tws2json.c: Convert a Tile World solution file to a JSON format.
 * 
 * Copyright © 2011 by Andrew Ekstedt, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include <stdio.h>
#include <stddef.h>

#include "bstrlib.h"

#include "solution.h"
#include "fileio.h"
#include "err.h"

#include "version.h"

const char *ruleset_names[] = {
    "",
    "lynx", /* Ruleset_Lynx */
    "ms", /* Ruleset_MS */
};

typedef struct jsoncompressinfo {
    bstring	str;

    action	lastmove;
    int	lastmovedir;

    int	rlemovedir;
    int	rlemoveduration;
    int	rlecount;
} jsoncompressinfo;

int jsoncompress_init(jsoncompressinfo *self);
void jsoncompress_free(jsoncompressinfo *self);
int jsoncompress_flush(jsoncompressinfo *self);
int jsoncompress_finish(jsoncompressinfo *self, unsigned long solutiontime);
int jsoncompress_addmove(jsoncompressinfo *self, action move, int i);
int jsoncompress_rle_add(jsoncompressinfo *self, int dir, int duration);
int jsoncompress_rle_flush(jsoncompressinfo *self);

/**
 * Print the given direction to the movestring buffer.
 *
 * @param duration 1 or 4
 * @returns 0 on success. -1 on failure.
 */
static int printdir(jsoncompressinfo *self, int dir, int duration)
{
    int r = BSTR_OK;

    if (duration == 1) {
	switch (dir) {
	case NORTH: r = bconchar(self->str, 'u'); break;
	case WEST:  r = bconchar(self->str, 'l'); break;
	case SOUTH: r = bconchar(self->str, 'd'); break;
	case EAST:  r = bconchar(self->str, 'r'); break;
	case NORTH|WEST: r = bcatcstr(self->str, "u+l"); break;
	case NORTH|EAST: r = bcatcstr(self->str, "u+r"); break;
	case SOUTH|WEST: r = bcatcstr(self->str, "d+l"); break;
	case SOUTH|EAST: r = bcatcstr(self->str, "d+r"); break;
	default: goto unknown;
	}
    } else if (duration == 4) {
	switch (dir) {
	case NORTH: r = bconchar(self->str, 'U'); break;
	case WEST:  r = bconchar(self->str, 'L'); break;
	case SOUTH: r = bconchar(self->str, 'D'); break;
	case EAST:  r = bconchar(self->str, 'R'); break;
	case NORTH|WEST: r = bcatcstr(self->str, "U+L"); break;
	case NORTH|EAST: r = bcatcstr(self->str, "U+R"); break;
	case SOUTH|WEST: r = bcatcstr(self->str, "D+L"); break;
	case SOUTH|EAST: r = bcatcstr(self->str, "D+R"); break;
	default: goto unknown;
	}
    }
    if (r != BSTR_OK) {
	return -1;
    }

    return 0;

unknown:
    errmsg("error", "Unknown direction (%d)", dir);
    return -1;
}

static int printnum(jsoncompressinfo *self, int num)
{
    if (BSTR_OK != bformata(self->str, "%d", num)) {
	return -1;
    }
    return 0;
}

static int printwait(jsoncompressinfo *self, int count)
{
    int r;

    if (count == 0) {
    } else if (count == 1) {
	if (BSTR_OK != bconchar(self->str, ',')) {
	    return -1;
	}
    } else if (count == 2) {
	if (BSTR_OK != bcatcstr(self->str, ",,")) {
	    return -1;
	}
    } else if (count == 4) {
	if (BSTR_OK != bconchar(self->str, '.')) {
	    return -1;
	}
    } else {
	r = printnum(self, count);
	if (r < 0) {
	    return r;
	}
	if (BSTR_OK != bconchar(self->str, ',')) {
	    return -1;
	}
    }
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
    self->lastmove.dir = NIL;
    self->lastmovedir = NIL;

    self->rlemovedir = NIL;
    self->rlemoveduration = 0;
    self->rlecount = 0;

    self->str = bfromcstr("");
    if (self->str == NULL) {
	return -1;
    }

    return 0;
}

void jsoncompress_free(jsoncompressinfo *self)
{
    if (self == NULL) {
	return;
    }
    bdestroy(self->str);
}

// Flush means: get rid of any buffered state; flush all moves to the char buffer; we've got something new coming in the pipeline.
int jsoncompress_flush(jsoncompressinfo *self)
{
    int r = 0;

    r = jsoncompress_rle_flush(self);
    if (r < 0) {
	goto cleanup;
    }

    if (self->lastmovedir != NIL) {
	int lastmoveduration = 1; // XXX hmm
	r = printdir(self, self->lastmovedir, lastmoveduration);
	if (r < 0) {
	    goto cleanup;
	}
    }

cleanup:
    self->lastmovedir = NIL;

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
    if (r < 0) {
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
    long delta = 0;

    if (self == NULL) {
	return -1;
    }

    if (i > 0) {
	// the ticks between the previous move and the current move.
	delta = move.when - self->lastmove.when;
    } else {
	delta = move.when;
    }

    // Attempt to upconvert previous move
    int lastmoveduration = 0;
    if (self->lastmovedir != NIL) {
	if (delta >= 4) {
	    lastmoveduration = 4;
	    delta -= 4;
	} else if (delta >= 1) {
	    lastmoveduration = 1;
	    delta -= 1;
	} else {
	    errmsg("error", "move %d: bad delta (%d)", i, delta);
	    return -1;
	}
    }

    // We are now finished monkeying with the previous move, so send it along.
    r = jsoncompress_rle_add(self, self->lastmovedir, lastmoveduration);
    self->lastmovedir = NIL;
    if (r < 0) {
	goto end;
    }

    // If we have any delta time left, flush the move stream and write a wait.
    if (delta > 0) {
	r = jsoncompress_flush(self);
	if (r < 0) {
	    goto end;
	}
	r = printwait(self, delta);
	if (r < 0) {
	    goto end;
	}
    }

end:
    self->lastmove = move;
    self->lastmovedir = move.dir;

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
int jsoncompress_finish(jsoncompressinfo *self, unsigned long solutiontime)
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
	r = printwait(self, solutiontime - self->lastmove.when - 1);
	if (r < 0) {
	    return r;
	}
    }
    return 0;
}

/**
 * Convert a list of moves to a textual representation.
 *
 * @returns 0 on success. 1 on failure.
 */
int compressjsonsolution(actlist *moves, unsigned long solutiontime, bstring movestr)
{
    jsoncompressinfo jsoncompress;
    int i, r;

    r = jsoncompress_init(&jsoncompress);
    if (r < 0) {
	return r;
    }

    for (i = 0; i < moves->count; i++) {
	r = jsoncompress_addmove(&jsoncompress, moves->list[i], i);
	if (r < 0) {
	    goto cleanup;
	}
    }
    r = jsoncompress_finish(&jsoncompress, solutiontime);
    if (r < 0) {
	goto cleanup;
    }

    bassign(movestr, jsoncompress.str);
    jsoncompress_free(&jsoncompress);

    return 0;

cleanup:
    jsoncompress_free(&jsoncompress);
    return r;
}

int main(int argc, char *argv[])
{
	// read the solution file
	int ruleset;
	int currentlevel;
	int extrasize;
	solutioninfo solution = {};
	gamesetup game;
	fileinfo file;
	int first;
	int skipfirstread;
	int ok;

	unsigned char extra[256];
	bstring movestr = NULL;

	clearfileinfo(&file);

	if (argc < 2) {
	    fprintf(stderr, "usage: tws2json file.tws\n");
	    return 1;
	}
	if (!fileopen(&file, argv[1], "rb", "file error")) {
		return 1;
	}

	if (!readsolutionheader(&file, &ruleset, &currentlevel, &extrasize, extra)) {
		fileclose(&file, "error");
		return 1;
	}

	if (!(1 <= ruleset && ruleset <= 2)) {
		errmsg("error", "Unknown ruleset (%d)\n", ruleset);
		fileclose(&file, "error");
		return 1;
	}

	// there might be some additional metadata after the header
	// in a solution record for level 0
	memset(&game, 0, sizeof game);
	ok = readsolution(&file, &game);
	skipfirstread = 0;
	if (ok && game.number != 0) {
		skipfirstread = 1;
	}

	// write json header
	printf("{\"class\":\"tws\",\n");
	printf(" \"ruleset\":\"%s\",\n", ruleset_names[ruleset]);
	if (currentlevel != 0) {
		printf(" \"currentlevel\":%d,\n", currentlevel);
	}
	if (game.sgflags & SGF_SETNAME) {
		printf(" \"levelset\":\"%s\",\n", game.name);
	}
	printf(" \"generator\":\"tws2json/" VERSION "\",\n");
	printf(" \"solutions\":[\n");

	movestr = bfromcstr("");
	for (first = 1;; first = 0) {
		if (!(first && skipfirstread)) {
			clearsolution(&game);
			memset(&game, 0, sizeof game);
			ok = readsolution(&file, &game);
			if (!ok) {
				break;
			}
		}
		if (game.number == 0) {
			continue;
		}
		// Trailing commas are not allowed.
		if (!first) {
			printf(",\n");
			fflush(stdout);
		}
		// write json level
		if (game.solutionsize == 6) {
			//just the number and password
			printf("  {\"class\":\"solution\",\n"
			       "   \"number\":%u,\n"
			       "   \"password\":\"%.4s\"}",
			       game.number,
			       game.passwd);
		} else {
			ok = expandsolution(&solution, &game);
			if (!ok) {
				// TODO: print error message
				continue;
			}
			if (compressjsonsolution(&solution.moves, game.besttime, movestr)) {
				// TODO: print error message
				continue;
			}
			printf("  {\"class\":\"solution\",\n"
			       "   \"number\":%u,\n"
			       "   \"password\":\"%s\",\n"
			       "   \"rndslidedir\":%d,\n"
			       "   \"stepping\":%d,\n"
			       "   \"rndseed\":%lu,\n"
			       "   \"moves\":\"%s\"}",
			       game.number,
			       game.passwd,
			       (int)solution.rndslidedir,
			       (int)solution.stepping,
			       solution.rndseed,
			       bdatae(movestr, "<out of memory>"));
		}
	}
	printf("\n]}\n");

	clearsolution(&game);
	destroymovelist(&solution.moves);
	bdestroy(movestr);
	fileclose(&file, "error");

	return 0;
}
