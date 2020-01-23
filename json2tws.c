/* tws2json.c: Convert a Tile World solution file to a JSON format.
 * 
 * Copyright © 2011 by Andrew Ekstedt, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "bstrlib.h"
#include "json.h"

#include "solution.h"
#include "fileio.h"
#include "err.h"

#include "version.h"

// parse.c
int parsemoves(actlist* moves, long* time, const char* movestring, int len);

void mystrlcpy(char* dest, const char* src, size_t size) {
	if (size == 0) {
		return;
	}
	size_t len = strlen(src);
	if (len+1 > size) {
		len = size - 1;
	}
	if (len) {
		memmove(dest, src, len);
	}
	dest[len] = '\0';
}

size_t read_callback(void *buf, size_t elsize, size_t nelem, void *fp) {
	return fread(buf, elsize, nelem, fp);
}

bstring filereadfull(fileinfo *file) {
	bstring str = bread(read_callback, file->fp);
	if (!str) {
		fileerr(file, "read error");
	}
	return str;
}

bool object_entry_eq(json_object_entry* entry, const char* str) {
	int len = strlen(str);
	return len == entry->name_length && strncmp(entry->name, str, len) == 0;
}

bool value_eq(json_value* v, const char* str) {
	return v->type == json_string && strcmp(v->u.string.ptr, str) == 0;
}

json_value* find_entry(json_value* v, const char* key) {
	if (v->type != json_object) {
		return NULL;
	}
	for (long i = 0; i < v->u.object.length; i++) {
		if (object_entry_eq(&v->u.object.values[i], key)) {
			return v->u.object.values[i].value;
		}
	}
	return NULL;
}

int getruleset(json_value* v) {
	if (v) {
		if (v->type == json_integer) {
			return v->u.integer;
		} else if (v->type == json_string) {
			if (value_eq(v, "ms")) {
				return Ruleset_MS;
			} else if (value_eq(v, "lynx")) {
				return Ruleset_Lynx;
			} else {
				warn("unknown ruleset \"%s\"", v->u.string.ptr);
			}
		}
	}
	return Ruleset_None;

}

void dump(json_value* moves, actlist* act) {
	if (moves && moves->type == json_string) {
		long time;
		printf("%s\n", moves->u.string.ptr);
		parsemoves(act, &time, moves->u.string.ptr, moves->u.string.length);
		for (int j = 0; j < act->count; j++) {
			if (act->list[j].dir >= 16) {
				int x = (act->list[j].dir - 16) % 19 - 9;
				int y = (act->list[j].dir - 16) / 19 - 9;
				printf("%8d mouse %d %d\n", act->list[j].when, x, y);
			} else {
				printf("%8d %d\n", act->list[j].when, act->list[j].dir);
			}
		}
	}
}

int doit(json_value *json, fileinfo *output) {
	if (json->type != json_object) {
		errmsg("error", "expected an object");
		return 1;
	}

	json_value* class = find_entry(json, "class");
	if (!class || !value_eq(class, "tws")) {
		errmsg("error", "expected object to have class \"tws\"");
		return 1;
	}

	json_value* solutions = find_entry(json, "solutions");
	if (!solutions || solutions->type != json_array) {
		errmsg("error", "no solutions");
	}

	int currentlevel = 0;
	json_value* jcurrentlevel = find_entry(json, "currentlevel");
	if (jcurrentlevel && jcurrentlevel->type == json_integer) {
		currentlevel = jcurrentlevel->u.integer;
	}

	int ruleset = getruleset(find_entry(json, "ruleset"));

	writesolutionheader(output, ruleset, currentlevel, 0, NULL);

	json_value* levelset = find_entry(json, "levelset");
	if (levelset && levelset->type == json_string) {
		writesolutionsetname(output, levelset->u.string.ptr);
	}

	actlist act = {};
	gamesetup game = {};
	solutioninfo solution = {};
	for (long i = 0; i < solutions->u.array.length; i++) {
		json_value* sol = solutions->u.array.values[i];
		if (sol->type != json_object) {
			warn("solution is not an object");
			continue;
		}

		// init data structures
		clearsolution(&game);
		game.number = 0;
		game.passwd[0] = '\0';
		solution.rndslidedir = 0;
		solution.rndseed = 0;
		solution.stepping = 0;
		solution.flags = 0;

		json_value* moves = NULL;
		bool have_password = false;
		bool valid = true;
		for (int j = 0; j < sol->u.object.length; j++) {
			json_object_entry* entry = &sol->u.object.values[j];
			if (object_entry_eq(entry, "class")) {
				if (!value_eq(entry->value, "solution")) {
					warn("expected solution entry to have class \"solution\"");
					valid = false;
					break;
				}
			} else if (object_entry_eq(entry, "number")) {
				if (entry->value->type == json_integer) {
					game.number = entry->value->u.integer;
				} else {
					warn("expected \"number\" to be a number");
				}
			} else if (object_entry_eq(entry, "password")) {
				if (entry->value->type == json_string) {
					if (entry->value->u.string.length != 4) {
						warn("password is not 4 characters");
					}
					mystrlcpy(game.passwd, entry->value->u.string.ptr, sizeof game.passwd);
					have_password = true;
				} else {
					warn("expected \"password\" to be a string");
				}
			} else if (object_entry_eq(entry, "rndslidedir")) {
				if (entry->value->type == json_integer) {
					solution.rndslidedir = entry->value->u.integer;
				} else {
					warn("expected \"rndslidedir\" to be an integer");
				}
			} else if (object_entry_eq(entry, "stepping")) {
				if (entry->value->type == json_integer) {
					solution.stepping = entry->value->u.integer;
				} else {
					warn("expected \"stepping\" to be an integer");
				}
			} else if (object_entry_eq(entry, "rndseed")) {
				if (entry->value->type == json_integer) {
					solution.rndseed = entry->value->u.integer;
				} else {
					warn("expected \"rndseed\" to be an integer");
				}
			} else if (object_entry_eq(entry, "moves")) {
				moves = entry->value;
			} else {
				warn("ignoring unknown field \"%*s\"", entry->name_length, entry->name);
			}
		}
		if (!valid) {
			warn("skipping solution");
			continue;
		}

		//dump(moves, &act);

		if (moves && moves->type == json_string) {
			long time = TIME_NIL;
			if (parsemoves(&act, &time, moves->u.string.ptr, moves->u.string.length) != 0) {
				errmsg("error", "level %d: error parsing moves", game.number);
				continue;
			}
			copymovelist(&solution.moves, &act);
			game.besttime = time;
			if (contractsolution(&solution, &game) == FALSE) {
				errmsg("error", "level %d: error building solution", game.number);
				continue;
			}
			printf("writing level %d %d\n", game.number, game.solutionsize);
			writesolution(output, &game);
			clearsolution(&game);
		} else if (have_password) {
			game.sgflags |= SGF_HASPASSWD;
			writesolution(output, &game);
		} else {
			warn("skipping solution %d because it has no moves and no password", i);
		}
	}
	destroymovelist(&act);

	return 0;
}

int main(int argc, char** argv) {
	fileinfo file, output;
	bstring data;
	json_value* json;

	if (argc < 3) {
	    fprintf(stderr, "usage: json2tws file.json output.tws\n");
	    return 1;
	}

	if (!fileopen(&file, argv[1], "rb", "file error")) {
		return 1;
	}

	data = filereadfull(&file);
	fileclose(&file, "file error");
	if (!data) {
		return 1;
	}

	json = json_parse(bdata(data), blength(data));
	if (!json) {
		errmsg("", "error parsing json");
		return 1;
	}

	if (!fileopen(&output, argv[2], "wb", "file error")) {
		return 1;
	}

	int status = doit(json, &output);

	fileclose(&output, "file error");
	json_value_free(json);
	return status;
}
