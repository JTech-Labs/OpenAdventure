#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "misc.h"
#include "main.h"
#include "share.h"
#include "funcs.h"

/*
 * Initialisation
 */

/*  Current limits:
 *     12500 words of message text (LINES, LINSIZ).
 *	885 travel options (TRAVEL, TRVSIZ).
 *	330 vocabulary words (KTAB, ATAB, TABSIZ).
 *	185 locations (LTEXT, STEXT, KEY, COND, ABB, ATLOC, LOCSND, LOCSIZ).
 *	100 objects (PLAC, PLACE, FIXD, FIXED, LINK (TWICE), PTEXT, PROP,
 *                    OBJSND, OBJTXT).
 *	 35 "action" verbs (ACTSPK, VRBSIZ).
 *	277 random messages (RTEXT, RTXSIZ).
 *	 12 different player classifications (CTEXT, CVAL, CLSMAX).
 *	 20 hints (HINTLC, HINTED, HINTS, HNTSIZ).
 *         5 "# of turns" threshholds (TTEXT, TRNVAL, TRNSIZ).
 *  There are also limits which cannot be exceeded due to the structure of
 *  the database.  (E.G., The vocabulary uses n/1000 to determine word type,
 *  so there can't be more than 1000 words.)  These upper limits are:
 *	1000 non-synonymous vocabulary words
 *	300 locations
 *	100 objects */


/*  Description of the database format
 *
 *
 *  The data file contains several sections.  Each begins with a line containing
 *  a number identifying the section, and ends with a line containing "-1".
 *
 *  Section 1: Long form descriptions.  Each line contains a location number,
 *	a tab, and a line of text.  The set of (necessarily adjacent) lines
 *	whose numbers are X form the long description of location X.
 *  Section 2: Short form descriptions.  Same format as long form.  Not all
 *	places have short descriptions.
 *  Section 3: Travel table.  Each line contains a location number (X), a second
 *	location number (Y), and a list of motion numbers (see section 4).
 *	each motion represents a verb which will go to Y if currently at X.
 *	Y, in turn, is interpreted as follows.  Let M=Y/1000, N=Y mod 1000.
 *		If N<=300	it is the location to go to.
 *		If 300<N<=500	N-300 is used in a computed goto to
 *					a section of special code.
 *		If N>500	message N-500 from section 6 is printed,
 *					and he stays wherever he is.
 *	Meanwhile, M specifies the conditions on the motion.
 *		If M=0		it's unconditional.
 *		If 0<M<100	it is done with M% probability.
 *		If M=100	unconditional, but forbidden to dwarves.
 *		If 100<M<=200	he must be carrying object M-100.
 *		If 200<M<=300	must be carrying or in same room as M-200.
 *		If 300<M<=400	PROP(M % 100) must *not* be 0.
 *		If 400<M<=500	PROP(M % 100) must *not* be 1.
 *		If 500<M<=600	PROP(M % 100) must *not* be 2, etc.
 *	If the condition (if any) is not met, then the next *different*
 *	"destination" value is used (unless it fails to meet *its* conditions,
 *	in which case the next is found, etc.).  Typically, the next dest will
 *	be for one of the same verbs, so that its only use is as the alternate
 *	destination for those verbs.  For instance:
 *		15	110022	29	31	34	35	23	43
 *		15	14	29
 *	This says that, from loc 15, any of the verbs 29, 31, etc., will take
 *	him to 22 if he's carrying object 10, and otherwise will go to 14.
 *		11	303008	49
 *		11	9	50
 *	This says that, from 11, 49 takes him to 8 unless PROP(3)=0, in which
 *	case he goes to 9.  Verb 50 takes him to 9 regardless of PROP(3).
 *  Section 4: Vocabulary.  Each line contains a number (n), a tab, and a
 *	five-letter word.  Call M=N/1000.  If M=0, then the word is a motion
 *	verb for use in travelling (see section 3).  Else, if M=1, the word is
 *	an object.  Else, if M=2, the word is an action verb (such as "carry"
 *	or "attack").  Else, if M=3, the word is a special case verb (such as
 *	"dig") and N % 1000 is an index into section 6.  Objects from 50 to
 *	(currently, anyway) 79 are considered treasures (for pirate, closeout).
 *  Section 5: Object descriptions.  Each line contains a number (N), a tab,
 *	and a message.  If N is from 1 to 100, the message is the "inventory"
 *	message for object n.  Otherwise, N should be 000, 100, 200, etc., and
 *	the message should be the description of the preceding object when its
 *	prop value is N/100.  The N/100 is used only to distinguish multiple
 *	messages from multi-line messages; the prop info actually requires all
 *	messages for an object to be present and consecutive.  Properties which
 *	produce no message should be given the message ">$<".
 *  Section 6: Arbitrary messages.  Same format as sections 1, 2, and 5, except
 *	the numbers bear no relation to anything (except for special verbs
 *	in section 4).
 *  Section 7: Object locations.  Each line contains an object number and its
 *	initial location (zero (or omitted) if none).  If the object is
 *	immovable, the location is followed by a "-1".  If it has two locations
 *	(e.g. the grate) the first location is followed with the second, and
 *	the object is assumed to be immovable.
 *  Section 8: Action defaults.  Each line contains an "action-verb" number and
 *	the index (in section 6) of the default message for the verb.
 *  Section 9: Location attributes.  Each line contains a number (n) and up to
 *	20 location numbers.  Bit N (where 0 is the units bit) is set in
 *	COND(LOC) for each loc given.  The cond bits currently assigned are:
 *		0	Light
 *		1	If bit 2 is on: on for oil, off for water
 *		2	Liquid asset, see bit 1
 *		3	Pirate doesn't go here unless following player
 *		4	Cannot use "back" to move away
 *	Bits past 10 indicate areas of interest to "hint" routines:
 *		11	Trying to get into cave
 *		12	Trying to catch bird
 *		13	Trying to deal with snake
 *		14	Lost in maze
 *		15	Pondering dark room
 *		16	At witt's end
 *		17	Cliff with urn
 *		18	Lost in forest
 *		19	Trying to deal with ogre
 *		20	Found all treasures except jade
 *	COND(LOC) is set to 2, overriding all other bits, if loc has forced
 *	motion.
 *  Section 10: Class messages.  Each line contains a number (n), a tab, and a
 *	message describing a classification of player.  The scoring section
 *	selects the appropriate message, where each message is considered to
 *	apply to players whose scores are higher than the previous N but not
 *	higher than this N.  Note that these scores probably change with every
 *	modification (and particularly expansion) of the program.
 *  SECTION 11: Hints.  Each line contains a hint number (add 10 to get cond
 *	bit; see section 9), the number of turns he must be at the right loc(s)
 *	before triggering the hint, the points deducted for taking the hint,
 *	the message number (section 6) of the question, and the message number
 *	of the hint.  These values are stashed in the "hints" array.  HNTMAX is
 *	set to the max hint number (<= HNTSIZ).
 *  Section 12: Unused in this version.
 *  Section 13: Sounds and text.  Each line contains either 2 or 3 numbers.  If
 *	2 (call them N and S), N is a location and message ABS(S) from section
 *	6 is the sound heard there.  If S<0, the sound there drowns out all
 *	other noises.  If 3 numbers (call them N, S, and T), N is an object
 *	number and S+PROP(N) is the property message (from section 5) if he
 *	listens to the object, and T+PROP(N) is the text if he reads it.  If
 *	S or T is -1, the object has no sound or text, respectively.  Neither
 *	S nor T is allowed to be 0.
 *  Section 14: Turn threshholds.  Each line contains a number (N), a tab, and
 *	a message berating the player for taking so many turns.  The messages
 *	must be in the proper (ascending) order.  The message gets printed if
 *	the player exceeds N % 100000 turns, at which time N/100000 points
 *	get deducted from his score.
 *  Section 0: End of database. */

/*  The various messages (sections 1, 2, 5, 6, etc.) may include certain
 *  special character sequences to denote that the program must provide
 *  parameters to insert into a message when the message is printed.  These
 *  sequences are:
 *	%S = The letter 'S' or nothing (if a given value is exactly 1)
 *	%W = A word (up to 10 characters)
 *	%L = A word mapped to lower-case letters
 *	%U = A word mapped to upper-case letters
 *	%C = A word mapped to lower-case, first letter capitalised
 *	%T = Several words of text, ending with a word of -1
 *	%1 = A 1-digit number
 *	%2 = A 2-digit number
 *	...
 *	%9 = A 9-digit number
 *	%B = Variable number of blanks
 *	%! = The entire message should be suppressed */

static bool quick_init(void);
static int raw_init(void);
static void report(void);
static void quick_save(void);
static int finish_init(void);
static void quick_io(void);

void initialise(void) {
	if (oldstyle)
		printf("Initialising...\n");
	if(!quick_init()){raw_init(); report(); quick_save();}
	finish_init();
}

static int raw_init(void) {
    //printf("Couldn't find adventure.data, using adventure.text...\n");

	FILE *OPENED=fopen("adventure.text","r" /* NOT binary */);
	if(!OPENED){printf("Can't read adventure.text!\n"); exit(0);}
	
/*  Clear out the various text-pointer arrays.  All text is stored in array
 *  lines; each line is preceded by a word pointing to the next pointer (i.e.
 *  the word following the end of the line).  The pointer is negative if this is
 *  first line of a message.  The text-pointer arrays contain indices of
 *  pointer-words in lines.  STEXT(N) is short description of location N.
 *  LTEXT(N) is long description.  PTEXT(N) points to message for PROP(N)=0.
 *  Successive prop messages are found by chasing pointers.  RTEXT contains
 *  section 6's stuff.  CTEXT(N) points to a player-class message.  TTEXT is for
 *  section 14.  We also clear COND (see description of section 9 for details). */

	/* 1001 */ for (I=1; I<=300; I++) {
	if(I <= 100)PTEXT[I]=0;
	if(I <= RTXSIZ)RTEXT[I]=0;
	if(I <= CLSMAX)CTEXT[I]=0;
	if(I <= 100)OBJSND[I]=0;
	if(I <= 100)OBJTXT[I]=0;
	if(I > LOCSIZ) goto L1001;
	STEXT[I]=0;
	LTEXT[I]=0;
	COND[I]=0;
	KEY[I]=0;
	LOCSND[I]=0;
L1001:	/*etc*/ ;
	} /* end loop */

	LINUSE=1;
	TRVS=1;
	CLSSES=0;
	TRNVLS=0;

/*  Start new data section.  Sect is the section number. */

L1002:	SECT=GETNUM(OPENED);
	OLDLOC= -1;
	switch (SECT) { case 0: return(0); case 1: goto L1004; case 2: goto
		L1004; case 3: goto L1030; case 4: goto L1040; case 5: goto L1004;
		case 6: goto L1004; case 7: goto L1050; case 8: goto L1060; case
		9: goto L1070; case 10: goto L1004; case 11: goto L1080; case 12:
		break; case 13: goto L1090; case 14: goto L1004; }
/*	      (0)  (1)  (2)  (3)  (4)  (5)  (6)  (7)  (8)  (9)
 *	     (10) (11) (12) (13) (14) */
	BUG(9);

/*  Sections 1, 2, 5, 6, 10, 14.  Read messages and set up pointers. */

L1004:	KK=LINUSE;
L1005:	LINUSE=KK;
	LOC=GETNUM(OPENED);
	if(LNLENG >= LNPOSN+70)BUG(0);
	if(LOC == -1) goto L1002;
	if(LNLENG < LNPOSN)BUG(1);
L1006:	KK=KK+1;
	if(KK >= LINSIZ)BUG(2);
	LINES[KK]=GETTXT(false,false,false,KK);
	if(LINES[KK] != -1) goto L1006;
	LINES[LINUSE]=KK;
	if(LOC == OLDLOC) goto L1005;
	OLDLOC=LOC;
	LINES[LINUSE]= -KK;
	if(SECT == 14) goto L1014;
	if(SECT == 10) goto L1012;
	if(SECT == 6) goto L1011;
	if(SECT == 5) goto L1010;
	if(LOC > LOCSIZ)BUG(10);
	if(SECT == 1) goto L1008;

	STEXT[LOC]=LINUSE;
	 goto L1005;

L1008:	LTEXT[LOC]=LINUSE;
	 goto L1005;

L1010:	if(LOC > 0 && LOC <= 100)PTEXT[LOC]=LINUSE;
	 goto L1005;

L1011:	if(LOC > RTXSIZ)BUG(6);
	RTEXT[LOC]=LINUSE;
	 goto L1005;

L1012:	CLSSES=CLSSES+1;
	if(CLSSES > CLSMAX)BUG(11);
	CTEXT[CLSSES]=LINUSE;
	CVAL[CLSSES]=LOC;
	 goto L1005;

L1014:	TRNVLS=TRNVLS+1;
	if(TRNVLS > TRNSIZ)BUG(11);
	TTEXT[TRNVLS]=LINUSE;
	TRNVAL[TRNVLS]=LOC;
	 goto L1005;

/*  The stuff for section 3 is encoded here.  Each "from-location" gets a
 *  contiguous section of the "TRAVEL" array.  Each entry in travel is
 *  NEWLOC*1000 + KEYWORD (from section 4, motion verbs), and is negated if
 *  this is the last entry for this location.  KEY(N) is the index in travel
 *  of the first option at location N. */

L1030:	LOC=GETNUM(OPENED);
	if(LOC == -1) goto L1002;
	NEWLOC=GETNUM(NULL);
	if(KEY[LOC] != 0) goto L1033;
	KEY[LOC]=TRVS;
	 goto L1035;
L1033:	TRVS--; TRAVEL[TRVS]= -TRAVEL[TRVS]; TRVS++;
L1035:	L=GETNUM(NULL);
	if(L == 0) goto L1039;
	TRAVEL[TRVS]=NEWLOC*1000+L;
	TRVS=TRVS+1;
	if(TRVS == TRVSIZ)BUG(3);
	 goto L1035;
L1039:	TRVS--; TRAVEL[TRVS]= -TRAVEL[TRVS]; TRVS++;
	 goto L1030;

/*  Here we read in the vocabulary.  KTAB(N) is the word number, ATAB(N) is
 *  the corresponding word.  The -1 at the end of section 4 is left in KTAB
 *  as an end-marker.  The words are given a minimal hash to make deciphering
 *  the core-image harder.  (We don't use gettxt's hash since that would force
 *  us to hash each input line to make comparisons work, and that in turn
 *  would make it harder to detect particular input words.) */

L1040:	J=10000;
	for (TABNDX=1; TABNDX<=TABSIZ; TABNDX++) {
	KTAB[TABNDX]=GETNUM(OPENED);
	if(KTAB[TABNDX] == -1) goto L1002;
	J=J+7;
	ATAB[TABNDX]=GETTXT(true,true,true,0)+J*J;
	} /* end loop */
	BUG(4);

/*  Read in the initial locations for each object.  Also the immovability info.
 *  plac contains initial locations of objects.  FIXD is -1 for immovable
 *  objects (including the snake), or = second loc for two-placed objects. */

L1050:	OBJ=GETNUM(OPENED);
	if(OBJ == -1) goto L1002;
	PLAC[OBJ]=GETNUM(NULL);
	FIXD[OBJ]=GETNUM(NULL);
	 goto L1050;

/*  Read default message numbers for action verbs, store in ACTSPK. */

L1060:	VERB=GETNUM(OPENED);
	if(VERB == -1) goto L1002;
	ACTSPK[VERB]=GETNUM(NULL);
	 goto L1060;

/*  Read info about available liquids and other conditions, store in COND. */

L1070:	K=GETNUM(OPENED);
	if(K == -1) goto L1002;
L1071:	LOC=GETNUM(NULL);
	if(LOC == 0) goto L1070;
	if(CNDBIT(LOC,K)) BUG(8);
	COND[LOC]=COND[LOC]+SETBIT(K);
	 goto L1071;

/*  Read data for hints. */

L1080:	HNTMAX=0;
L1081:	K=GETNUM(OPENED);
	if(K == -1) goto L1002;
	if(K <= 0 || K > HNTSIZ)BUG(7);
	for (I=1; I<=4; I++) {
	HINTS[K][I] =GETNUM(NULL);
	} /* end loop */
	HNTMAX=(HNTMAX>K ? HNTMAX : K);
	 goto L1081;

/*  Read the sound/text info, store in OBJSND, OBJTXT, LOCSND. */

L1090:	K=GETNUM(OPENED);
	if(K == -1) goto L1002;
	KK=GETNUM(NULL);
	I=GETNUM(NULL);
	if(I == 0) goto L1092;
	OBJSND[K]=(KK>0 ? KK : 0);
	OBJTXT[K]=(I>0 ? I : 0);
	 goto L1090;

L1092:	LOCSND[K]=KK;
	 goto L1090;
}

/*  Finish constructing internal data format */

/*  Having read in the database, certain things are now constructed.  PROPS are
 *  set to zero.  We finish setting up COND by checking for forced-motion travel
 *  entries.  The PLAC and FIXD arrays are used to set up ATLOC(N) as the first
 *  object at location N, and LINK(OBJ) as the next object at the same location
 *  as OBJ.  (OBJ>100 indicates that FIXED(OBJ-100)=LOC; LINK(OBJ) is still the
 *  correct link to use.)  ABB is zeroed; it controls whether the abbreviated
 *  description is printed.  Counts modulo 5 unless "LOOK" is used. */

static int finish_init(void) {
	for (I=1; I<=100; I++) {
	PLACE[I]=0;
	PROP[I]=0;
	LINK[I]=0;
	{long x = I+100; LINK[x]=0;}
	} /* end loop */

	/* 1102 */ for (I=1; I<=LOCSIZ; I++) {
	ABB[I]=0;
	if(LTEXT[I] == 0 || KEY[I] == 0) goto L1102;
	K=KEY[I];
	if(MOD(IABS(TRAVEL[K]),1000) == 1)COND[I]=2;
L1102:	ATLOC[I]=0;
	} /* end loop */

/*  Set up the ATLOC and LINK arrays as described above.  We'll use the DROP
 *  subroutine, which prefaces new objects on the lists.  Since we want things
 *  in the other order, we'll run the loop backwards.  If the object is in two
 *  locs, we drop it twice.  This also sets up "PLACE" and "fixed" as copies of
 *  "PLAC" and "FIXD".  Also, since two-placed objects are typically best
 *  described last, we'll drop them first. */

	/* 1106 */ for (I=1; I<=100; I++) {
	K=101-I;
	if(FIXD[K] <= 0) goto L1106;
	DROP(K+100,FIXD[K]);
	DROP(K,PLAC[K]);
L1106:	/*etc*/ ;
	} /* end loop */

	for (I=1; I<=100; I++) {
	K=101-I;
	FIXED[K]=FIXD[K];
	if(PLAC[K] != 0 && FIXD[K] <= 0)DROP(K,PLAC[K]);
	} /* end loop */

/*  Treasures, as noted earlier, are objects 50 through MAXTRS (CURRENTLY 79).
 *  Their props are initially -1, and are set to 0 the first time they are
 *  described.  TALLY keeps track of how many are not yet found, so we know
 *  when to close the cave. */

	MAXTRS=79;
	TALLY=0;
	for (I=50; I<=MAXTRS; I++) {
	if(PTEXT[I] != 0)PROP[I]= -1;
	TALLY=TALLY-PROP[I];
	} /* end loop */

/*  Clear the hint stuff.  HINTLC(I) is how long he's been at LOC with cond bit
 *  I.  HINTED(I) is true iff hint I has been used. */

	for (I=1; I<=HNTMAX; I++) {
	HINTED[I]=false;
	HINTLC[I]=0;
	} /* end loop */

/*  Define some handy mnemonics.  These correspond to object numbers. */

	AXE=VOCWRD(12405,1);
	BATTER=VOCWRD(201202005,1);
	BEAR=VOCWRD(2050118,1);
	BIRD=VOCWRD(2091804,1);
	BLOOD=VOCWRD(212151504,1);
	BOTTLE=VOCWRD(215202012,1);
	CAGE=VOCWRD(3010705,1);
	CAVITY=VOCWRD(301220920,1);
	CHASM=VOCWRD(308011913,1);
	CLAM=VOCWRD(3120113,1);
	DOOR=VOCWRD(4151518,1);
	DRAGON=VOCWRD(418010715,1);
	DWARF=VOCWRD(423011806,1);
	FISSUR=VOCWRD(609191921,1);
	FOOD=VOCWRD(6151504,1);
	GRATE=VOCWRD(718012005,1);
	KEYS=VOCWRD(11052519,1);
	KNIFE=VOCWRD(1114090605,1);
	LAMP=VOCWRD(12011316,1);
	MAGZIN=VOCWRD(1301070126,1);
	MESSAG=VOCWRD(1305191901,1);
	MIRROR=VOCWRD(1309181815,1);
	OGRE=VOCWRD(15071805,1);
	OIL=VOCWRD(150912,1);
	OYSTER=VOCWRD(1525192005,1);
	PILLOW=VOCWRD(1609121215,1);
	PLANT=VOCWRD(1612011420,1);
	PLANT2=PLANT+1;
	RESER=VOCWRD(1805190518,1);
	ROD=VOCWRD(181504,1);
	ROD2=ROD+1;
	SIGN=VOCWRD(19090714,1);
	SNAKE=VOCWRD(1914011105,1);
	STEPS=VOCWRD(1920051619,1);
	TROLL=VOCWRD(2018151212,1);
	TROLL2=TROLL+1;
	URN=VOCWRD(211814,1);
	VEND=VOCWRD(1755140409,1);
	VOLCAN=VOCWRD(1765120301,1);
	WATER=VOCWRD(1851200518,1);

/*  Objects from 50 through whatever are treasures.  Here are a few. */

	AMBER=VOCWRD(113020518,1);
	CHAIN=VOCWRD(308010914,1);
	CHEST=VOCWRD(308051920,1);
	COINS=VOCWRD(315091419,1);
	EGGS=VOCWRD(5070719,1);
	EMRALD=VOCWRD(513051801,1);
	JADE=VOCWRD(10010405,1);
	NUGGET=VOCWRD(7151204,1);
	PEARL=VOCWRD(1605011812,1);
	PYRAM=VOCWRD(1625180113,1);
	RUBY=VOCWRD(18210225,1);
	RUG=VOCWRD(182107,1);
	SAPPH=VOCWRD(1901161608,1);
	TRIDNT=VOCWRD(2018090405,1);
	VASE=VOCWRD(22011905,1);

/*  These are motion-verb numbers. */

	BACK=VOCWRD(2010311,0);
	CAVE=VOCWRD(3012205,0);
	DPRSSN=VOCWRD(405161805,0);
	ENTER=VOCWRD(514200518,0);
	ENTRNC=VOCWRD(514201801,0);
	LOOK=VOCWRD(12151511,0);
	NUL=VOCWRD(14211212,0);
	STREAM=VOCWRD(1920180501,0);

/*  And some action verbs. */

	FIND=VOCWRD(6091404,2);
	INVENT=VOCWRD(914220514,2);
	LOCK=VOCWRD(12150311,2);
	SAY=VOCWRD(190125,2);
	THROW=VOCWRD(2008181523,2);

/*  Initialise the dwarves.  DLOC is loc of dwarves, hard-wired in.  ODLOC is
 *  prior loc of each dwarf, initially garbage.  DALTLC is alternate initial loc
 *  for dwarf, in case one of them starts out on top of the adventurer.  (No 2
 *  of the 5 initial locs are adjacent.)  DSEEN is true if dwarf has seen him.
 *  DFLAG controls the level of activation of all this:
 *	0	No dwarf stuff yet (wait until reaches Hall Of Mists)
 *	1	Reached Hall Of Mists, but hasn't met first dwarf
 *	2	Met first dwarf, others start moving, no knives thrown yet
 *	3	A knife has been thrown (first set always misses)
 *	3+	Dwarves are mad (increases their accuracy)
 *  Sixth dwarf is special (the pirate).  He always starts at his chest's
 *  eventual location inside the maze.  This loc is saved in CHLOC for ref.
 *  the dead end in the other maze has its loc stored in CHLOC2. */

	CHLOC=114;
	CHLOC2=140;
	for (I=1; I<=6; I++) {
	DSEEN[I]=false;
	} /* end loop */
	DFLAG=0;
	DLOC[1]=19;
	DLOC[2]=27;
	DLOC[3]=33;
	DLOC[4]=44;
	DLOC[5]=64;
	DLOC[6]=CHLOC;
	DALTLC=18;

/*  Other random flags and counters, as follows:
 *	ABBNUM	How often we should print non-abbreviated descriptions
 *	BONUS	Used to determine amount of bonus if he reaches closing
 *	CLOCK1	Number of turns from finding last treasure till closing
 *	CLOCK2	Number of turns from first warning till blinding flash
 *	CONDS	Min value for cond(loc) if loc has any hints
 *	DETAIL	How often we've said "not allowed to give more detail"
 *	DKILL	Number of dwarves killed (unused in scoring, needed for msg)
 *	FOOBAR	Current progress in saying "FEE FIE FOE FOO".
 *	HOLDNG	Number of objects being carried
 *	IGO	How many times he's said "go XXX" instead of "XXX"
 *	IWEST	How many times he's said "west" instead of "w"
 *	KNFLOC	0 if no knife here, loc if knife here, -1 after caveat
 *	LIMIT	Lifetime of lamp (not set here)
 *	MAXDIE	Number of reincarnation messages available (up to 5)
 *	NUMDIE	Number of times killed so far
 *	THRESH	Next #turns threshhold (-1 if none)
 *	TRNDEX	Index in TRNVAL of next threshhold (section 14 of database)
 *	TRNLUZ	# points lost so far due to number of turns used
 *	TURNS	Tallies how many commands he's given (ignores yes/no)
 *	Logicals were explained earlier */

	TURNS=0;
	TRNDEX=1;
	THRESH= -1;
	if(TRNVLS > 0)THRESH=MOD(TRNVAL[1],100000)+1;
	TRNLUZ=0;
	LMWARN=false;
	IGO=0;
	IWEST=0;
	KNFLOC=0;
	DETAIL=0;
	ABBNUM=5;
	for (I=0; I<=4; I++) {
	{long x = 2*I+81; if(RTEXT[x] != 0)MAXDIE=I+1;}
	} /* end loop */
	NUMDIE=0;
	HOLDNG=0;
	DKILL=0;
	FOOBAR=0;
	BONUS=0;
	CLOCK1=30;
	CLOCK2=50;
	CONDS=SETBIT(11);
	SAVED=0;
	CLOSNG=false;
	PANIC=false;
	CLOSED=false;
	CLSHNT=false;
	NOVICE=false;
	SETUP=1;

	/* if we can ever think of how, we should save it at this point */

	return(0); /* then we won't actually return from initialisation */
}

/*  Report on amount of arrays actually used, to permit reductions. */

static void report(void) {
	for (K=1; K<=LOCSIZ; K++) {
	KK=LOCSIZ+1-K;
	if(LTEXT[KK] != 0) goto L1997;
	/*etc*/ ;
	} /* end loop */

	OBJ=0;
L1997:	for (K=1; K<=100; K++) {
	if(PTEXT[K] != 0)OBJ=OBJ+1;
	} /* end loop */

	for (K=1; K<=TABNDX; K++) {
	if(KTAB[K]/1000 == 2)VERB=KTAB[K]-2000;
	} /* end loop */

	for (K=1; K<=RTXSIZ; K++) {
	J=RTXSIZ+1-K;
	if(RTEXT[J] != 0) goto L1993;
	/*etc*/ ;
	} /* end loop */

L1993:	SETPRM(1,LINUSE,LINSIZ);
	SETPRM(3,TRVS,TRVSIZ);
	SETPRM(5,TABNDX,TABSIZ);
	SETPRM(7,KK,LOCSIZ);
	SETPRM(9,OBJ,100);
	SETPRM(11,VERB,VRBSIZ);
	SETPRM(13,J,RTXSIZ);
	SETPRM(15,CLSSES,CLSMAX);
	SETPRM(17,HNTMAX,HNTSIZ);
	SETPRM(19,TRNVLS,TRNSIZ);
	//RSPEAK(267);
	TYPE0();
}

static long init_reading, init_cksum;
static FILE *f;

static void quick_item(long*);
static void quick_array(long*, long);

static bool quick_init(void) {
	extern char *getenv();
	char *adv = getenv("ADVENTURE");
	f = NULL;
	if(adv)f = fopen(adv,READ_MODE);
	if(f == NULL)f = fopen("adventure.data",READ_MODE);
	if(f == NULL)return(false);
	init_reading = true;
	init_cksum = 1;
	quick_io();
	if(fread(&K,sizeof(long),1,f) == 1) init_cksum -= K; else init_cksum = 1;
	fclose(f);
	if(init_cksum != 0)printf("Checksum error!\n");
	return(init_cksum == 0);
}

static void quick_save(void) {
	//printf("Writing adventure.data...\n");
	f = fopen("adventure.data",WRITE_MODE);
	if(f == NULL){printf("Can't open file!\n"); return;}
	init_reading = false;
	init_cksum = 1;
	quick_io();
	fwrite(&init_cksum,sizeof(long),1,f);
	fclose(f);
}

static void quick_io(void) {
	quick_item(&LINUSE);
	quick_item(&TRVS);
	quick_item(&CLSSES);
	quick_item(&TRNVLS);
	quick_item(&TABNDX);
	quick_item(&HNTMAX);
	quick_array(PTEXT,100);
	quick_array(RTEXT,RTXSIZ);
	quick_array(CTEXT,CLSMAX);
	quick_array(OBJSND,100);
	quick_array(OBJTXT,100);
	quick_array(STEXT,LOCSIZ);
	quick_array(LTEXT,LOCSIZ);
	quick_array(COND,LOCSIZ);
	quick_array(KEY,LOCSIZ);
	quick_array(LOCSND,LOCSIZ);
	quick_array(LINES,LINSIZ);
	quick_array(CVAL,CLSMAX);
	quick_array(TTEXT,TRNSIZ);
	quick_array(TRNVAL,TRNSIZ);
	quick_array(TRAVEL,TRVSIZ);
	quick_array(KTAB,TABSIZ);
	quick_array(ATAB,TABSIZ);
	quick_array(PLAC,100);
	quick_array(FIXD,100);
	quick_array(ACTSPK,VRBSIZ);
	quick_array((long *)HINTS,(HNTMAX+1)*5-1);
}

static void quick_item(W)long *W; {
	if(init_reading && fread(W,sizeof(long),1,f) != 1)return;
	init_cksum = MOD(init_cksum*13+(*W),60000000);
	if(!init_reading)fwrite(W,sizeof(long),1,f);
}

static void quick_array(A,N)long *A, N; { long I;
	if(init_reading && fread(A,sizeof(long),N+1,f) != N+1)printf("Read error!\n");
	for(I=1;I<=N;I++)init_cksum = MOD(init_cksum*13+A[I],60000000);
	if(!init_reading && fwrite(A,sizeof(long),N+1,f)!=N+1)printf("Write error!\n");
}
