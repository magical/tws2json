/* solution.c: Functions for reading and writing the solution files.
 *
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"err.h"
#include	"fileio.h"
#include	"solution.h"

/*
 * The following is a description of the solution file format. Note that
 * numeric values are always stored in little-endian order, consistent
 * with the data file.
 *
 * The header is at least eight bytes long, and contains the following
 * values:
 *
 * HEADER
 *  0-3   signature bytes (35 33 9B 99)
 *   4    ruleset (1=Lynx, 2=MS)
 *   5    other options (currently always zero)
 *   6    other options (currently always zero)
 *   7    count of bytes in remainder of header (currently always zero)
 *
 * After the header are level solutions, usually but not necessarily
 * in numerical order. Each solution begins with the following values:
 *
 * PER LEVEL
 *  0-3   offset to next solution (from the end of this field)
 *  4-5   level number
 *  6-9   level password (four ASCII characters in "cleartext")
 *  10    other flags (currently always zero)
 *  11    initial random slide direction and stepping value
 * 12-15  initial random number generator value
 * 16-19  time of solution in ticks
 * 20-xx  solution bytes
 *
 * If the offset field is 0, then none of the other fields are
 * present. (This permits the file to contain padding.) If the offset
 * field is 6, then the level's number and password are present but
 * without a saved game. Otherwise, the offset should never be less
 * than 16.
 *
 * Note that byte 11 contains the initial random slide direction in
 * the bottom three bits, and the initial stepping value in the next
 * three bits. The top two bits are unused. (The initial random slide
 * direction is always zero under the MS ruleset.)
 *
 * The solution bytes consist of a stream of values indicating the
 * moves of the solution. The values vary in size from one to five
 * bytes in length. The size of each value is always specified in the
 * first byte. There are five different formats for the values. (Note
 * that in the following byte diagrams, the bits are laid out
 * little-endian instead of the usual MSB-first.)
 *
 * The first format can be either one or two bytes long. The two-byte
 * form is shown here:
 *
 * #1: 01234567 89012345
 *     NNDDDTTT TTTTTTTT
 *
 * The two lowest bits, marked with Ns, contain either one (01) or two
 * (10), and indicate how many bytes are used. The next three bits,
 * marked with Ds, contain the direction of the move. The remaining
 * bits are marked with Ts, and these indicate the amount of time, in
 * ticks, between this move and the prior move, less one. (Thus, a
 * value of T=0 indicates a move that occurs on the tick immediately
 * following the previous move.) The very first move of a solution is
 * an exception: it is not decremented, as that would sometimes
 * require a negative value to be stored. If the one-byte version is
 * used, then T is only three bits in size; otherwise T is 11 bits
 * long.
 *
 * The second format is four bytes in length:
 *
 * #2: 01234567 89012345 67890123 45678901
 *     11DD0TTT TTTTTTTT TTTTTTTT TTTTTTTT
 *
 * This format allocates 27 bits for the time value. (The top four
 * bits will always be zero, however, as the game's timer is currently
 * limited to 23 bits.) Since there are only two bits available for
 * storing the direction, this format can only be used to store
 * orthogonal moves (i.e. it cannot be used to store a Lynx diagonal
 * move).
 *
 * The third format has the form:
 *
 * #3: 01234567
 *     00DDEEFF
 *
 * This value encodes three separate moves (DD, EE, and FF) packed
 * into a single byte. Each move has an implicit time value of four
 * ticks separating it from the prior move (i.e. T=3). Like the second
 * format, only two bits are used to store each move.
 *
 * The fourth and final format, like the first format, can vary in
 * size. It can be two, three, four, or five bytes long, depending on
 * how many bits are needed to store the time interval. It is shown
 * here in its largest form:
 *
 * #4: 01234567 89012345 67890123 45678901 23456789
 *     11NN1DDD DDDDDDTT TTTTTTTT TTTTTTTT TTTTTTTT
 *
 * The two bits marked NN indicate the size of the field in bytes,
 * less two (i.e., 00 for a two-byte value, 11 for a five-byte value).
 * Seven bits are used to indicate the move's direction, which allows
 * this field to store MS mouse moves. The time value is encoded
 * normally, and can be 2, 10, 18, or 26 bits long.
 */

/* The signature bytes of the solution files.
 */
#define	CSSIG		0x999B3335UL

/* Translate move directions between three-bit and four-bit
 * representations.
 *
 * 0 = NORTH	    = 0001 = 1
 * 1 = WEST	    = 0010 = 2
 * 2 = SOUTH	    = 0100 = 4
 * 3 = EAST	    = 1000 = 8
 * 4 = NORTH | WEST = 0011 = 3
 * 5 = SOUTH | WEST = 0110 = 6
 * 6 = NORTH | EAST = 1001 = 9
 * 7 = SOUTH | EAST = 1100 = 12
 */
static int const diridx8[16] = {
    -1,  0,  1,  4,  2, -1,  5, -1,  3,  6, -1, -1,  7, -1, -1, -1
};
static int const idxdir8[8] = {
    NORTH, WEST, SOUTH, EAST,
    NORTH | WEST, SOUTH | WEST, NORTH | EAST, SOUTH | EAST
};

#define	isdirectmove(dir)	(directionalcmd(dir))
#define	ismousemove(dir)	(!isdirectmove(dir))
#define	isdiagonal(dir)		(isdirectmove(dir) && diridx8[dir] > 3)
#define	isorthogonal(dir)	(isdirectmove(dir) && diridx8[dir] <= 3)
#define	dirtoindex(dir)		(diridx8[dir])
#define	indextodir(dir)		(idxdir8[dir])

/*
 * Functions for manipulating move lists.
 */

/* Initialize or reinitialize list as empty.
 */
void initmovelist(actlist *list)
{
    if (!list->allocated || !list->list) {
	list->allocated = 16;
	xalloc(list->list, list->allocated * sizeof *list->list);
    }
    list->count = 0;
}

/* Append move to the end of list.
 */
void addtomovelist(actlist *list, action move)
{
    if (list->count >= list->allocated) {
	list->allocated *= 2;
	xalloc(list->list, list->allocated * sizeof *list->list);
    }
    list->list[list->count++] = move;
}

/* Make to an independent copy of from.
 */
void copymovelist(actlist *to, actlist const *from)
{
    if (!to->allocated || !to->list)
	to->allocated = 16;
    while (to->allocated < from->count)
	to->allocated *= 2;
    xalloc(to->list, to->allocated * sizeof *to->list);
    to->count = from->count;
    if (from->count)
	memcpy(to->list, from->list, from->count * sizeof *from->list);
}

/* Deallocate list.
 */
void destroymovelist(actlist *list)
{
    if (list->list)
	free(list->list);
    list->allocated = 0;
    list->list = NULL;
}

/*
 * Functions for handling the solution file header.
 */

/* Read the header bytes of the given solution file. flags receives
 * the option bytes (bytes 5-6). extra receives any bytes in the
 * header that this code doesn't recognize.
 */
int readsolutionheader(fileinfo *file, int *ruleset, int *flags,
		       int *extrasize, unsigned char *extra)
{
    unsigned long	sig;
    unsigned short	f;
    unsigned char	n;

    if (!filereadint32(file, &sig, "not a valid solution file"))
	return FALSE;
    if (sig != CSSIG)
	return fileerr(file, "not a valid solution file");
    if (!filereadint8(file, &n, "not a valid solution file"))
	return FALSE;
    if (!filereadint16(file, &f, "not a valid solution file"))
	return FALSE;
    *ruleset = (int)n;
    *flags = (int)f;

    if (!filereadint8(file, &n, "not a valid solution file"))
	return FALSE;
    *extrasize = n;
    if (n)
	if (!fileread(file, extra, *extrasize, "not a valid solution file"))
	    return FALSE;

    return TRUE;
}

/* Write the header bytes to the given solution file.
 */
/*
static int writesolutionheader(fileinfo *file, int ruleset, int flags,
			       int extrasize, unsigned char const *extra)
{
    return filewriteint32(file, CSSIG, NULL)
	&& filewriteint8(file, ruleset, NULL)
	&& filewriteint16(file, flags, NULL)
	&& filewriteint8(file, extrasize, NULL)
	&& filewrite(file, extra, extrasize, NULL);
}
*/

/* Write the name of the level set to the given solution file.
 */
/*
static int writesolutionsetname(fileinfo *file, char const *setname)
{
    char	zeroes[16] = "";
    int		n;

    n = strlen(setname) + 1;
    return filewriteint32(file, n + 16, NULL)
	&& filewrite(file, zeroes, 16, NULL)
	&& filewrite(file, setname, n, NULL);
}
*/

/*
 * Solution translation.
 */

/* Expand a level's solution data into an actual list of moves.
 */
int expandsolution(solutioninfo *solution, gamesetup const *game)
{
    unsigned char const	       *dataend;
    unsigned char const	       *p;
    action			act;
    int				n;

    if (game->solutionsize <= 16)
	return FALSE;

    solution->flags = game->solutiondata[6];
    solution->rndslidedir = indextodir(game->solutiondata[7] & 7);
    solution->stepping = (game->solutiondata[7] >> 3) & 7;
    solution->rndseed = game->solutiondata[8] | (game->solutiondata[9] << 8)
					      | (game->solutiondata[10] << 16)
					      | (game->solutiondata[11] << 24);

    initmovelist(&solution->moves);
    act.when = -1;
    p = game->solutiondata + 16;
    dataend = game->solutiondata + game->solutionsize;
    while (p < dataend) {
	switch (*p & 0x03) {
	  case 0:
	    act.dir = indextodir((*p >> 2) & 0x03);
	    act.when += 4;
	    addtomovelist(&solution->moves, act);
	    act.dir = indextodir((*p >> 4) & 0x03);
	    act.when += 4;
	    addtomovelist(&solution->moves, act);
	    act.dir = indextodir((*p >> 6) & 0x03);
	    act.when += 4;
	    addtomovelist(&solution->moves, act);
	    ++p;
	    break;
	  case 1:
	    act.dir = indextodir((*p >> 2) & 0x07);
	    act.when += ((*p >> 5) & 0x07) + 1;
	    addtomovelist(&solution->moves, act);
	    ++p;
	    break;
	  case 2:
	    if (p + 2 > dataend)
		goto truncated;
	    act.dir = indextodir((*p >> 2) & 0x07);
	    act.when += ((p[0] >> 5) & 0x07) + ((unsigned long)p[1] << 3) + 1;
	    addtomovelist(&solution->moves, act);
	    p += 2;
	    break;
	  case 3:
	    if (*p & 0x10) {
		n = (*p >> 2) & 0x03;
		if (p + 2 + n > dataend)
		    goto truncated;
		act.dir = ((p[0] >> 5) & 0x07) | ((p[1] & 0x3F) << 3);
		act.when += (p[1] >> 6) & 0x03;
		while (n--)
		    act.when += (unsigned long)p[2 + n] << (2 + n * 8);
		++act.when;
		p += 2 + ((*p >> 2) & 0x03);
	    } else {
		if (p + 4 > dataend)
		    goto truncated;
		act.dir = indextodir((*p >> 2) & 0x03);
		act.when += ((p[0] >> 5) & 0x07) | ((unsigned long)p[1] << 3)
						 | ((unsigned long)p[2] << 11)
						 | ((unsigned long)p[3] << 19);
		++act.when;
		p += 4;
	    }
	    addtomovelist(&solution->moves, act);
	    break;
	}
    }
    return TRUE;

  truncated:
    errmsg(NULL, "level %d: truncated solution data", game->number);
    initmovelist(&solution->moves);
    return FALSE;
}

/* Take the given solution and compress it, storing the compressed
 * data as part of the level's setup.
 */
int contractsolution(solutioninfo const *solution, gamesetup *game)
{
    action const       *move;
    unsigned char      *data;
    int			size, delta, when, i;

    free(game->solutiondata);
    game->solutionsize = 0;
    game->solutiondata = NULL;
    if (!solution->moves.count)
	return TRUE;

    size = 21;
    move = solution->moves.list + 1;
    for (i = 1 ; i < solution->moves.count ; ++i, ++move)
	size += !isorthogonal(move->dir) ? 5
			: move[0].when - move[-1].when <= (1 << 3) ? 1
			: move[0].when - move[-1].when <= (1 << 11) ? 2 : 4;
    data = malloc(size);
    if (!data) {
	errmsg(NULL, "failed to record level %d solution:"
		     " out of memory", game->number);
	return FALSE;
    }

    data[0] = game->number & 0xFF;
    data[1] = (game->number >> 8) & 0xFF;
    data[2] = game->passwd[0];
    data[3] = game->passwd[1];
    data[4] = game->passwd[2];
    data[5] = game->passwd[3];
    data[6] = solution->flags;
    data[7] = dirtoindex(solution->rndslidedir) | (solution->stepping << 3);
    data[8] = solution->rndseed & 0xFF;
    data[9] = (solution->rndseed >> 8) & 0xFF;
    data[10] = (solution->rndseed >> 16) & 0xFF;
    data[11] = (solution->rndseed >> 24) & 0xFF;
    data[12] = game->besttime & 0xFF;
    data[13] = (game->besttime >> 8) & 0xFF;
    data[14] = (game->besttime >> 16) & 0xFF;
    data[15] = (game->besttime >> 24) & 0xFF;

    when = -1;
    size = 16;
    move = solution->moves.list;
    for (i = 0 ; i < solution->moves.count ; ++i, ++move) {
	delta = -when - 1;
	when = move->when;
	delta += when;
	if (ismousemove(move->dir)
			|| (isdiagonal(move->dir) && delta >= (1 << 11))) {
	    data[size] = 0x13 | ((move->dir << 5) & 0xE0);
	    data[size + 1] = ((move->dir >> 3) & 0x3F) | ((delta & 0x03) << 6);
	    if (delta < (1 << 2)) {
		size += 2;
	    } else {
		data[size + 2] = (delta >> 2) & 0xFF;
		if (delta < (1 << 10)) {
		    data[size] |= 1 << 2;
		    size += 3;
		} else {
		    data[size + 3] = (delta >> 10) & 0xFF;
		    if (delta < (1 << 18)) {
			data[size] |= 2 << 2;
			size += 4;
		    } else {
			data[size + 4] = (delta >> 18) & 0xFF;
			data[size] |= 3 << 2;
			size += 5;
		    }
		}
	    }
	} else if (delta == 3 && i + 2 < solution->moves.count
			      && isorthogonal(move[0].dir)
			      && move[1].when - move[0].when == 4
			      && isorthogonal(move[1].dir)
			      && move[2].when - move[1].when == 4
			      && isorthogonal(move[2].dir)) {
	    data[size++] = (dirtoindex(move[0].dir) << 2)
			 | (dirtoindex(move[1].dir) << 4)
			 | (dirtoindex(move[2].dir) << 6);
	    move += 2;
	    i += 2;
	    when = move->when;
	} else if (delta < (1 << 3)) {
	    data[size++] = 0x01 | (dirtoindex(move->dir) << 2)
				| ((delta << 5) & 0xE0);
	} else if (delta < (1 << 11)) {
	    data[size++] = 0x02 | (dirtoindex(move->dir) << 2)
				| ((delta << 5) & 0xE0);
	    data[size++] = (delta >> 3) & 0xFF;
	} else {
	    data[size++] = 0x03 | (dirtoindex(move->dir) << 2)
				| ((delta << 5) & 0xE0);
	    data[size++] = (delta >> 3) & 0xFF;
	    data[size++] = (delta >> 11) & 0xFF;
	    data[size++] = (delta >> 19) & 0xFF;
	}
    }

    game->solutionsize = size;
    game->solutiondata = realloc(data, size);
    if (!game->solutiondata)
	game->solutiondata = data;
    return TRUE;
}

/*
 * File I/O for level solutions.
 */

/* Read the data of a one complete solution from the given file into
 * a gamesetup structure.
 */
int readsolution(fileinfo *file, gamesetup *game)
{
    unsigned long	size;

    game->number = 0;
    game->sgflags = 0;
    game->besttime = TIME_NIL;

    game->solutionsize = 0;
    game->solutiondata = NULL;

    if (!file->fp)
	return TRUE;

    if (!filereadint32(file, &size, NULL) || size == 0xFFFFFFFF)
	return FALSE;
    if (!size)
	return TRUE;
    game->solutionsize = size;
    game->solutiondata = filereadbuf(file, size, "unexpected EOF");
    if (!game->solutiondata || (size <= 16 && size != 6)) {
	free(game->solutiondata);
	game->solutionsize = 0;
	game->solutiondata = NULL;
	return fileerr(file, "invalid data in solution file");
    }
    game->number = (game->solutiondata[1] << 8) | game->solutiondata[0];
    memcpy(game->passwd, game->solutiondata + 2, 4);
    game->passwd[4] = '\0';
    game->sgflags |= SGF_HASPASSWD;
    if (size == 6)
	return TRUE;

    game->besttime = game->solutiondata[12] | (game->solutiondata[13] << 8)
					    | (game->solutiondata[14] << 16)
					    | (game->solutiondata[15] << 24);

    if (!game->number && !*game->passwd) {
	size -= 16;
	game->sgflags |= SGF_SETNAME;
	if (size > 255)
	    size = 255;
	memcpy(game->name, game->solutiondata + 16, size);
	game->name[size] = '\0';
	free(game->solutiondata);
	game->solutionsize = 0;
	game->solutiondata = NULL;
    }

    return TRUE;
}

/* Write the data of one complete solution from the appropriate fields
 * of game to the given file.
 */
/*
static int writesolution(fileinfo *file, gamesetup const *game)
{
    if (game->solutionsize) {
	if (!filewriteint32(file, game->solutionsize, "write error")
			|| !filewrite(file, game->solutiondata,
				      game->solutionsize, "write error"))
	return FALSE;
    } else if (game->sgflags & SGF_HASPASSWD) {
	if (!filewriteint32(file, 6, "write error")
			|| !filewriteint16(file, game->number, "write error")
			|| !filewrite(file, game->passwd, 4, "write error"))
	    return FALSE;
    }

    return TRUE;
}*/

/* Free all memory allocated for storing a solution.
 */
void clearsolution(gamesetup *game)
{
    free(game->solutiondata);
    game->besttime = TIME_NIL;
    game->sgflags = 0;
    game->solutionsize = 0;
    game->solutiondata = NULL;
}
