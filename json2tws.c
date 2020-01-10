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

#include "solution.h"
#include "fileio.h"
#include "err.h"

#include "version.h"

#include "json.h"

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

int doit(json_value *json) {
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

	for (long i = 0; i < solutions->u.array.length; i++) {
		json_value* sol = solutions->u.array.values[i];
		json_value* moves = find_entry(sol, "moves");
		if (moves && moves->type == json_string) {
			printf("%s\n", moves->u.string.ptr);
		}
	}

	return 0;
}

int main(int argc, char** argv) {
	fileinfo file;
	bstring data;
	json_value* json;

	if (argc < 2) {
	    fprintf(stderr, "usage: json2tws file.json\n");
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

	int status = doit(json);

	json_value_free(json);
	return status;
}
