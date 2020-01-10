/* solution.c: Functions for reading and writing the solution files.
 *
 * Copyright (C) 2001-2006 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_solution_h_
#define	_solution_h_

#include	"fileio.h"

/* The standard Boolean values.
 */
#ifndef	TRUE
#define	TRUE	1
#endif
#ifndef	FALSE
#define	FALSE	0
#endif

/* The four directions plus one non-direction.
 */
#define	NIL	0
#define	NORTH	1
#define	WEST	2
#define	SOUTH	4
#define	EAST	8

/* The gameplay timer's value is forced to remain within 23 bits.
 * Thus, gameplay of a single level cannot exceed 4 days 20 hours 30
 * minutes and 30.4 seconds.
 */
#define	MAXIMUM_TICK_COUNT	0x7FFFFF

/* A magic number used to indicate an undefined time value.
 */
#define	TIME_NIL		0x7FFFFFFF


/*
 * Miscellaneous definitions.
 */

/* The various rulesets the program can emulate.
 */
enum {
    Ruleset_None = 0,
    Ruleset_Lynx = 1,
    Ruleset_MS = 2,
    Ruleset_Count
};

/*
 * Definitions used in game play.
 */

/* A move is specified by its direction and when it takes place.
 */
typedef	struct action { unsigned int when:23, dir:9; } action;

/* A structure for managing the memory holding the moves of a game.
 */
typedef struct actlist {
    int			allocated;	/* number of elements allocated */
    int			count;		/* size of the actual array */
    action	       *list;		/* the array */
} actlist;

/* A structure holding all the data needed to reconstruct a solution.
 */
typedef	struct solutioninfo {
    actlist		moves;		/* the actual moves of the solution */
    unsigned long	rndseed;	/* the PRNG's initial seed */
    unsigned long	flags;		/* other flags (currently unused) */
    unsigned char	rndslidedir;	/* random slide's initial direction */
    signed char		stepping;	/* the timer offset */
} solutioninfo;

/* The range of relative mouse moves is a 19x19 square around Chip.
 * (Mouse moves are stored as a relative offset in order to fit all
 * possible moves in nine bits.)
 */
#define	MOUSERANGEMIN	-9
#define	MOUSERANGEMAX	+9
#define	MOUSERANGE	19

enum { CmdKeyMoveLast = NORTH | WEST | SOUTH | EAST };

/* True if cmd is a simple directional command, i.e. a single
 * orthogonal or diagonal move (or CmdNone).
 */
#define	directionalcmd(cmd)	(((cmd) & ~CmdKeyMoveLast) == 0)

/* The collection of data maintained for each level.
 */
typedef	struct gamesetup {
    int			number;		/* numerical ID of the level */
    int			time;		/* no. of seconds allotted */
    int			besttime;	/* time (in ticks) of best solution */
    int			sgflags;	/* saved-game flags (see below) */
    int			levelsize;	/* size of the level data */
    int			solutionsize;	/* size of the saved solution data */
    unsigned char      *leveldata;	/* the data defining the level */
    unsigned char      *solutiondata;	/* the player's best solution so far */
    unsigned long	levelhash;	/* the level data's hash value */
    char const	       *unsolvable;	/* why level is unsolvable, or NULL */
    char		name[256];	/* name of the level */
    char		passwd[256];	/* the level's password */
} gamesetup;

/* Flags associated with a saved game.
 */
#define	SGF_HASPASSWD		0x0001	/* player knows the level's password */
#define	SGF_REPLACEABLE		0x0002	/* solution is marked as replaceable */
#define	SGF_SETNAME		0x0004	/* internal to solution.c */


/*
 * Solution functions.
 */

/* Initialize or reinitialize list as empty.
 */
extern void initmovelist(actlist *list);

/* Append move to the end of list.
 */
extern void addtomovelist(actlist *list, action move);

/* Make to an independent copy of from.
 */
extern void copymovelist(actlist *to, actlist const *from);

/* Deallocate list.
 */
extern void destroymovelist(actlist *list);

/* Read the header bytes of the given solution file. flags receives
 * the option bytes (bytes 5-6). extra receives any bytes in the
 * header that this code doesn't recognize.
 */
extern int readsolutionheader(fileinfo *file, int *ruleset, int *flags,
		              int *extrasize, unsigned char *extra);

/* Read the data of a one complete solution from the given file into
 * a gamesetup structure.
 */
extern int readsolution(fileinfo *file, gamesetup* game);

/* Free all memory allocated for storing a solution.
 */
extern void clearsolution(gamesetup *game);

/* Expand a level's solution data into the actual solution, including
 * the full list of moves. FALSE is returned if the solution is
 * invalid or absent.
 */
extern int expandsolution(solutioninfo *solution, gamesetup const *game);

/* Take the given solution and compress it, storing the compressed
 * data as part of the level's setup. FALSE is returned if an error
 * occurs. (It is not an error to compress the null solution.)
 */
extern int contractsolution(solutioninfo const *solution, gamesetup *game);

#endif
