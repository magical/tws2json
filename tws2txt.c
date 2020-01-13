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

	clearfileinfo(&file);

	if (argc < 2) {
	    fprintf(stderr, "usage: tws2txt file.tws\n");
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
		if (game.solutionsize == 6) {
			continue;
		}
		ok = expandsolution(&solution, &game);
		if (!ok) {
			// TODO: print error message
			continue;
		}
		printf("level %d\n", game.number);
		actlist act = solution.moves;
		for (int j = 0; j < act.count; j++) {
			if (act.list[j].dir >= 16) {
				int x = (act.list[j].dir - 16) % 19 - 9;
				int y = (act.list[j].dir - 16) / 19 - 9;
				printf("%8d mouse %d %d\n", act.list[j].when, x, y);
			} else {
				printf("%8d %d\n", act.list[j].when, act.list[j].dir);
			}
		}
	}

	clearsolution(&game);
	destroymovelist(&solution.moves);
	fileclose(&file, "error");

	return 0;
}
