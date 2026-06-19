/*
 * The cast and the things they carry, drawn once. See
 * [bbc_cast.h](bbc_cast.h) for the conventions every sprite here keeps.
 *
 * Every figure below was drawn the way a MODE 2 sprite was drawn in 1984: on
 * squared paper, one character to a fat pixel, facing right, with nothing in
 * it that a black room could swallow — no black boot on a black floor, no
 * outline, no shading but a second colour laid where a shadow would fall.
 */

#include "bbc_cast.h"

/* ==== Chuck =============================================================
 *
 * Red cap, cyan jacket, blue jeans, red boots: the one figure in the game in
 * cyan, so he is the first thing found on any screen. Faces right; the cap's
 * peak and the eye at the front of the face say which way. The near sleeve is
 * the blue column down the jacket, which is the only shading a jacket four
 * pixels wide can carry, and the brass pixel at the front of the belt is the
 * buckle.
 */

BBC_SPRITE_EXTERN(SPR_CHUCK_STAND,
    "...RRR..",
    "..RRRRRR",
    "..RWKW..",
    "...WWWW.",
    "...WWW..",
    "..CCCC..",
    "..CCCCC.",
    "..CBCCC.",
    "..CBCCC.",
    "..CBCCC.",
    "..BWBBY.",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..RR.RRR");

/* A run is WALK1, WALK2, WALK3, WALK2: stride, pass, the other stride, pass.
 * The arms swing against the legs, and the two strides differ in which knee
 * is driving, so the cycle travels instead of scissoring on the spot. */
BBC_SPRITE_EXTERN(SPR_CHUCK_WALK1,
    "...RRR..",
    "..RRRRRR",
    "..RWKW..",
    "...WWWW.",
    "...WWW..",
    "..CCCC..",
    ".CCCCCC.",
    "BBCCCCCW",
    "W.CCCC..",
    "..CCCC..",
    "..BBBBY.",
    ".BBB.BB.",
    ".BB...BB",
    "BB....BB",
    "B......B",
    "R......R");

BBC_SPRITE_EXTERN(SPR_CHUCK_WALK2,
    "...RRR..",
    "..RRRRRR",
    "..RWKW..",
    "...WWWW.",
    "...WWW..",
    "..CCCC..",
    "..CCCCC.",
    "..CBCCC.",
    "..CBCCC.",
    "..CCWCC.",
    "..BBBBY.",
    "..BBBB..",
    "..BBBB..",
    ".BB.BB..",
    ".R..BB..",
    "....RRR.");

BBC_SPRITE_EXTERN(SPR_CHUCK_WALK3,
    "...RRR..",
    "..RRRRRR",
    "..RWKW..",
    "...WWWW.",
    "...WWW..",
    "..CCCC..",
    "..CCCCC.",
    "..CCCCBB",
    "..CCCC.W",
    "..CCCC..",
    "..BBBBY.",
    "..BB.BBB",
    ".BB...BB",
    ".BB....B",
    "BB.....R",
    "RR......");

/* The beat after a landing: knees taken up, arms out for balance. Two rows
 * shorter than standing, which is all the squash a sixteen-row man can take
 * before he turns into rubber. */
BBC_SPRITE_EXTERN(SPR_CHUCK_LAND,
    "........",
    "........",
    "...RRR..",
    "..RRRRRR",
    "..RWKW..",
    "...WWWW.",
    "...WWW..",
    "..CCCC..",
    ".CCCCCC.",
    "WCCBCCCW",
    "..CBCC..",
    "..BBBBY.",
    ".BBB.BB.",
    ".BB..BB.",
    ".BB..BB.",
    "RRR..RRR");

BBC_SPRITE_EXTERN(SPR_CHUCK_JUMP,
    "...RRR.W",
    "..RRRRRC",
    "..RWKWC.",
    "...WWWW.",
    "...WWW..",
    "..CCCC..",
    ".CCCCC..",
    "WBCCCC..",
    "..CCCC..",
    "..BBBBY.",
    "..BBBBBB",
    ".BBB..BB",
    ".BB...R.",
    "..RR....",
    "........",
    "........");

BBC_SPRITE_EXTERN(SPR_CHUCK_FALL,
    "...RRR..",
    "W.RRRRRR",
    "C.RWKW.W",
    "CC.WWWWC",
    ".C.WWWCC",
    ".CCCCCC.",
    "..CCCC..",
    "..CCCC..",
    "..CCCC..",
    "..BBBBY.",
    "..BB.BB.",
    ".BB..BB.",
    ".BB...BB",
    "RR.....B",
    "......RR",
    "........");

/* The pistol is the yellow pixel past the hand: brass, like every round the
 * HUD counts, and the round leaves from the pixel past it. */
BBC_SPRITE_EXTERN(SPR_CHUCK_AIM,
    "...RRR..",
    "..RRRRRR",
    "..RWKW..",
    "...WWWW.",
    "...WWW..",
    "..CCCC..",
    "..CCCCCC",
    "..CBBBWY",
    "..CCCC..",
    "..CCCC..",
    "..BBBBY.",
    "..BB.BB.",
    "..BB.BB.",
    ".BB..BB.",
    ".BB..BB.",
    ".RR..RRR");

BBC_SPRITE_EXTERN(SPR_CHUCK_AIM_UP,
    "...RRR.Y",
    "..RRRRRW",
    "..RWKW.B",
    "...WWWWB",
    "...WWWB.",
    "..CCCCB.",
    "..CCCCC.",
    "..CCCCC.",
    "..CCCC..",
    "..CCCC..",
    "..BBBBY.",
    "..BB.BB.",
    "..BB.BB.",
    ".BB..BB.",
    ".BB..BB.",
    ".RR..RRR");

BBC_SPRITE_EXTERN(SPR_CHUCK_AIM_DOWN,
    "...RRR..",
    "..RRRRRR",
    "..RWKW..",
    "...WWWW.",
    "...WWW..",
    "..CCCC..",
    "..CCCCC.",
    "..CCCCCB",
    "..CCCC.B",
    "..CCCC.W",
    "..BBBBYY",
    "..BB.BB.",
    "..BB.BB.",
    ".BB..BB.",
    ".BB..BB.",
    ".RR..RRR");

/* The blade is white and runs on out of the fist, so the stroke reads as a
 * stab rather than as the pistol: the pistol is brass. */
BBC_SPRITE_EXTERN(SPR_CHUCK_KNIFE,
    "...RRR..",
    "..RRRRRR",
    "..RWKW..",
    "...WWWW.",
    "...WWW..",
    ".CCCCC..",
    "BCCCCBWW",
    "W.CCCC..",
    "..CCCC..",
    "..CCCC..",
    "..BBBBY.",
    "..BBBBB.",
    ".BB..BB.",
    ".BB...BB",
    "BB....BB",
    "RR.....R");

/* Underarm, at the end of the swing: the hand high and forward, empty, the far
 * arm thrown back against it. What left it is drawn by whoever threw it. */
BBC_SPRITE_EXTERN(SPR_CHUCK_THROW,
    "...RRR..",
    "..RRRRRR",
    "..RWKW.W",
    "...WWWWC",
    "...WWWC.",
    "..CCCC..",
    ".CCCCC..",
    "BBCCCC..",
    "W.CCCC..",
    "..CCCC..",
    "..BBBBY.",
    "..BBBBB.",
    ".BB..BB.",
    ".BB...BB",
    "BB....BB",
    "RR.....R");

/* Braced under the launcher: both hands up at the shoulder, feet apart.
 * SPR_BAZOOKA goes down first, with its left edge at the sprite's column
 * CHUCK_BAZOOKA_COL and its top at row CHUCK_BAZOOKA_ROW, and he is drawn over
 * it — so the tube passes behind his head, and the one thing a player aiming a
 * rocket needs to see, which way the man is looking, stays in front. */
BBC_SPRITE_EXTERN(SPR_CHUCK_SHOULDER,
    "...RRR..",
    "..RRRRRR",
    "..RWKW..",
    "...WWWW.",
    "...WWW..",
    "..CCCC..",
    "..CCCCC.",
    "..CBBBWW",
    "..CCCC..",
    "..CCCC..",
    "..BBBBY.",
    "..BB.BB.",
    ".BB..BB.",
    ".BB...BB",
    "BB....BB",
    "RR....RR");

/* Leaning back against the weight, both hands low and forward on whatever he
 * is hauling. Two frames, the legs trading, for a man walking backwards with
 * a collar in his fists. */
BBC_SPRITE_EXTERN(SPR_CHUCK_DRAG,
    "..RRR...",
    ".RRRRRR.",
    ".RWKW...",
    "..WWWW..",
    "..WWW...",
    "..CCCC..",
    "..CCCCC.",
    "..CCCCCC",
    "...CBBBW",
    "...CCC.W",
    "...BBBY.",
    "..BBBBB.",
    ".BB...BB",
    ".BB...BB",
    "BB.....B",
    "RR....RR");

BBC_SPRITE_EXTERN(SPR_CHUCK_DRAG2,
    "..RRR...",
    ".RRRRRR.",
    ".RWKW...",
    "..WWWW..",
    "..WWW...",
    "..CCCC..",
    "..CCCCC.",
    "..CCCCCC",
    "...CBBBW",
    "...CCC.W",
    "...BBBY.",
    "...BBB..",
    "...BBB..",
    "..BB.B..",
    "..BB.BB.",
    "..RR.RRR");

/* From behind, at a console: the back of the cap, the nape, both elbows out
 * and the hands up on the keys out of sight. Two frames, the elbows trading
 * as he types. */
BBC_SPRITE_EXTERN(SPR_CHUCK_HACK1,
    "........",
    "...RR...",
    "..RRRR..",
    "..RRRR..",
    "...WW...",
    "C.CCCC..",
    "CCCCCCCC",
    "..CCCC.C",
    "..CBBC..",
    "..CCCC..",
    "..BBBB..",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..RR.RR.");

BBC_SPRITE_EXTERN(SPR_CHUCK_HACK2,
    "........",
    "...RR...",
    "..RRRR..",
    "..RRRR..",
    "...WW...",
    "..CCCC.C",
    "CCCCCCCC",
    "C.CCCC..",
    "..CBBC..",
    "..CCCC..",
    "..BBBB..",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..RR.RR.");

/* From behind, on a ladder or a wall: one hand up on the rail and the
 * opposite knee up. Mirror it for the other half of the stroke; the head is
 * on the centre line, so the mirror moves only the limbs. */
BBC_SPRITE_EXTERN(SPR_CHUCK_CLIMB,
    ".W......",
    ".C......",
    ".C.RR...",
    ".CRRRR..",
    ".CRRRR..",
    ".C.WW...",
    ".CCCCCC.",
    "..CCCCCW",
    "..CBBC..",
    "..CCCC..",
    "..BBBB..",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.RR.",
    "..BB....",
    "..RR....");

/* On a rung, one hand on the rail and the other out to the side at the end
 * of a straight arm, with whatever it holds past the hand: the pistol, the
 * blade or a throw. Faces right; mirror it to fire left. */
BBC_SPRITE_EXTERN(SPR_CHUCK_CLIMB_AIM,
    "........",
    "........",
    "...RR...",
    "..RRRR..",
    ".WRRRR..",
    ".C.WW...",
    ".CCCCCCW",
    "..CCCC..",
    "..CBBC..",
    "..CCCC..",
    "..BBBB..",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..RR.RR.");

/* The arm straight up the shaft past the head. */
BBC_SPRITE_EXTERN(SPR_CHUCK_CLIMB_AIM_UP,
    "......W.",
    "......C.",
    "...RR.C.",
    "..RRRRC.",
    ".WRRRRC.",
    ".C.WW.C.",
    ".CCCCCC.",
    "..CCCC..",
    "..CBBC..",
    "..CCCC..",
    "..BBBB..",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..RR.RR.");

/* And straight down it, outside the legs. */
BBC_SPRITE_EXTERN(SPR_CHUCK_CLIMB_AIM_DOWN,
    "........",
    "........",
    "...RR...",
    "..RRRR..",
    ".WRRRR..",
    ".C.WW...",
    ".CCCCCC.",
    "..CCCC.C",
    "..CBBC.C",
    "..CCCC.C",
    "..BBBB.C",
    "..BB.BBW",
    "..BB.BB.",
    "..BB.BB.",
    "..BB.BB.",
    "..RR.RR.");

/* Flat out, nine rows tall for a crawl box of eighteen pixels: head up at the
 * front under the cap, elbows under him, boots trailing, and the bottom row on
 * the floor. Two frames, the elbow and the knee changing places. */
BBC_SPRITE_EXTERN(SPR_CHUCK_CRAWL1,
    "........",
    "........",
    "........",
    "........",
    ".....RR.",
    ".....RRR",
    "RBBCCWKW",
    ".BBCCCWW",
    "R.B.C.W.");

BBC_SPRITE_EXTERN(SPR_CHUCK_CRAWL2,
    "........",
    "........",
    "........",
    "........",
    ".....RR.",
    ".....RRR",
    "RBBCCWKW",
    "RBBCCCWW",
    "..B..CW.");

/* On his back where he fell, cap knocked off the back of his head. */
BBC_SPRITE_EXTERN(SPR_CHUCK_DEAD,
    "........",
    "RBB.CC.W",
    "RBBCCCWW",
    "..B..W.R");

/* The launcher: green tube, yellow sight and grip, the black mouth forward. */
BBC_SPRITE_EXTERN(SPR_BAZOOKA,
    "....Y.....",
    "GGGGGGGGGG",
    "GGGGGGGGGK",
    "GGGGGGGGGG",
    "......Y...");

/* The same tube carried up a ladder shaft, mouth at the top, and its mirror in
 * time pointed down it. Three by eleven. */
BBC_SPRITE_EXTERN(SPR_BAZOOKA_UP,
    "GKG.",
    "GGG.",
    "GGG.",
    "GGGY",
    "GGG.",
    "GGG.",
    "GGG.",
    "GGGY",
    "GGG.",
    "GGG.",
    "GGG.");

BBC_SPRITE_EXTERN(SPR_BAZOOKA_DOWN,
    "GGG.",
    "GGG.",
    "GGG.",
    "GGGY",
    "GGG.",
    "GGG.",
    "GGG.",
    "GGGY",
    "GGG.",
    "GGG.",
    "GKG.");

/* ==== Guards ============================================================
 *
 * Slots: '1' uniform, '2' helmet, '3' trousers and boots, '4' webbing. A
 * domed helmet rather than a cap, so a guard and Chuck are told apart by the
 * shape of the head as well as the colour; the red visor is the pixel that
 * says "enemy" across a room; and the rifle is white because it is the thing
 * the player has to watch. It is carried low at the front while he walks and
 * comes up along the line the round will take when he aims, and those two
 * drawings are as far apart as eight columns allow, because bringing it up is
 * the whole of the warning.
 */

BBC_SPRITE_EXTERN(SPR_GUARD_STAND,
    "..2222..",
    ".222222.",
    "..2WRR..",
    "...WWW..",
    "....W...",
    "..1111..",
    "..1W111.",
    "..11W11.",
    "..111W1.",
    "..1111W.",
    "..4444.W",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.333");

/* A patrol is WALK1, STAND, WALK2, STAND, and the muzzle stays on the floor a
 * stride ahead of him throughout. */
BBC_SPRITE_EXTERN(SPR_GUARD_WALK1,
    "..2222..",
    ".222222.",
    "..2WRR..",
    "...WWW..",
    "....W...",
    "..1111..",
    "..1W111.",
    "..11W11.",
    "..111W1.",
    "..1111W.",
    "..4444.W",
    "..33333.",
    ".33..33.",
    ".33...33",
    "33....33",
    "33.....3");

BBC_SPRITE_EXTERN(SPR_GUARD_WALK2,
    "..2222..",
    ".222222.",
    "..2WRR..",
    "...WWW..",
    "....W...",
    "..1111..",
    "..1W111.",
    "..11W11.",
    "..111W1.",
    "..1111W.",
    "..4444.W",
    "..3333..",
    "...333..",
    "..33.3..",
    ".33..33.",
    ".3...333");

/* Shouldered, level: the rifle runs out past his front edge at the height the
 * round will fly, and the round leaves from the pixel past its last one. */
BBC_SPRITE_EXTERN(SPR_GUARD_AIM,
    "..2222..",
    ".222222.",
    "..2WRR..",
    "...WWW..",
    "....W...",
    "..1111..",
    "..1WWWWW",
    "..111W..",
    "..1111..",
    "..1111..",
    "..4444..",
    "..33.33.",
    "..33.33.",
    ".33..33.",
    ".33..33.",
    ".33..333");

/* Up the column: the rifle stood on end in front of the face, the visor still
 * in sight beside it. */
BBC_SPRITE_EXTERN(SPR_GUARD_AIM_UP,
    "..2222.W",
    ".222222W",
    "..2WRR.W",
    "...WWW.W",
    "....W.WW",
    "..1111.W",
    "..11111W",
    "..1111..",
    "..1111..",
    "..1111..",
    "..4444..",
    "..33.33.",
    "..33.33.",
    ".33..33.",
    ".33..33.",
    ".33..333");

/* Down at the floor he is standing on, the muzzle past the boots. */
BBC_SPRITE_EXTERN(SPR_GUARD_AIM_DOWN,
    "..2222..",
    ".222222.",
    "..2WRR..",
    "...WWW..",
    "....W...",
    "..1111..",
    "..11111.",
    "..1111WW",
    "..1111.W",
    "..1111WW",
    "..4444.W",
    "..33.3.W",
    "..33.3.W",
    ".33..3.W",
    ".33..3.W",
    ".33..33W");

/* In conversation, the rifle slung on his back — its muzzle is the white past
 * the back shoulder — and the near hand out, turning a point over. Two frames,
 * the hand rising and falling. */
BBC_SPRITE_EXTERN(SPR_GUARD_TALK,
    "..2222..",
    ".222222.",
    ".W2WRR..",
    ".W.WWW..",
    ".W..W...",
    "..1111..",
    "..11111.",
    "..11111.",
    "..1111WW",
    "..1111..",
    "..4444..",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.333");

BBC_SPRITE_EXTERN(SPR_GUARD_TALK2,
    "..2222..",
    ".222222.",
    ".W2WRR..",
    ".W.WWW..",
    ".W..W...",
    "..1111..",
    "..11111.",
    "..11111W",
    "..1111.W",
    "..1111..",
    "..4444..",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.333");

/* On the net rather than to the man beside him: the forearm up across the
 * chest to the handset on the shoulder, and the whip standing clear of the
 * helmet, because a handset drawn against the head is lost in it. The rifle is
 * slung. */
BBC_SPRITE_EXTERN(SPR_GUARD_RADIO,
    ".......W",
    "..2222.W",
    ".222222W",
    ".W2WRR.W",
    ".W.WWWBW",
    "..1111B.",
    "..1111W.",
    "..1111..",
    "..1111..",
    "..1111..",
    "..4444..",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.333");

/* Reaching up for a wall switch, the rifle slung. */
BBC_SPRITE_EXTERN(SPR_GUARD_REACH,
    ".......W",
    "..2222.1",
    ".222222.",
    ".W2WRR1.",
    ".W.WWW1.",
    ".W..W1..",
    "..1111..",
    "..1111..",
    "..1111..",
    "..1111..",
    "..4444..",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33.333");

/* Both hands over his eyes and the rifle let go to hang on its sling, muzzle
 * at his boots: what a flash charge leaves a man doing, and the opposite of a
 * man standing still because he has lost you. */
BBC_SPRITE_EXTERN(SPR_GUARD_DAZZLED,
    "..2222..",
    ".222222.",
    "..2WWWW.",
    "..1WWW1.",
    "..1.W.1.",
    "..1111..",
    "..1111..",
    "..11W1..",
    "..11W1..",
    "..11W1..",
    "..44W4..",
    "..33W33.",
    "..33.W3.",
    ".33...33",
    ".33...33",
    "33....33");

/* From behind on a ladder, the rifle across his back. Mirror it for the other
 * half of the stroke. */
BBC_SPRITE_EXTERN(SPR_GUARD_CLIMB,
    ".W......",
    ".1.22...",
    ".12222..",
    ".12222..",
    ".1.WW...",
    ".111111.",
    "..1111.1",
    "..1W11.W",
    "..11W1..",
    "..111W..",
    "..4444..",
    "..33.33.",
    "..33.33.",
    "..33.33.",
    "..33....",
    "..33....");

/* Down, on his back, the helmet at the far end and the rifle gone from his
 * hands — nothing that says "armed" stays on a body, and the visor is dark. */
BBC_SPRITE_EXTERN(SPR_GUARD_DOWN,
    ".3.....2.",
    ".3...WW22",
    "33341111W",
    "3.....W..");

/* The heavy's plate and pauldrons: the outline wider than a guard's at the
 * shoulders, because a man who cannot be stomped has to be told from one who
 * can before the player jumps. Laid on any guard frame at row
 * HEAVY_PLATE_ROW, *under* it or over it alike — every pixel is outside the
 * columns a guard's torso fills. '2' is the plate. */
BBC_SPRITE_EXTERN(SPR_HEAVY_PLATE,
    "22....22",
    "22....22",
    ".2....2.",
    "........");

/* And the full helmet: the shell carried down over the ear and the nape. Laid
 * under a side-on frame at row 0, so the guard's own visor slit stays on top
 * of it — in the heavy's white the face and the helmet are one shell with a
 * red slit across it. */
BBC_SPRITE_EXTERN(SPR_HEAVY_HELM,
    "..2222..",
    ".222222.",
    ".22.....",
    ".22.....",
    "..2.....");

/* ==== The dog ===========================================================
 *
 * Slots: '1' coat, '2' nose and teeth. Eight by eight over a 24x16 box, nose
 * forward, ears pricked, tail out behind. A gallop is RUN1, RUN2: stretched
 * out and gathered up.
 */

BBC_SPRITE_EXTERN(SPR_DOG_STAND,
    "......1.",
    ".....111",
    "1.1111K2",
    ".111111.",
    ".11111..",
    ".1...11.",
    ".1...1..",
    "11..11..");

BBC_SPRITE_EXTERN(SPR_DOG_RUN1,
    "........",
    "......1.",
    "1....111",
    ".11111K2",
    ".111111.",
    ".1111...",
    "1.....11",
    "1.......");

BBC_SPRITE_EXTERN(SPR_DOG_RUN2,
    "........",
    "......1.",
    ".1...111",
    "..1111K2",
    ".11111..",
    ".11111..",
    "..1..1..",
    ".11.11..");

/* Jaws apart, lunging: the white is teeth. */
BBC_SPRITE_EXTERN(SPR_DOG_BITE,
    "........",
    "......1.",
    "1....111",
    ".11111K2",
    ".111111.",
    ".1111.WW",
    "1.....1.",
    "1......1");

/* The windup before a bite: down on its haunches, head low, hackles up and
 * the teeth showing. A dog crouched like this is about to close the gap, and
 * the crouch is the player's whole warning, so it is the furthest thing from
 * a dog standing that eight rows allow. */
BBC_SPRITE_EXTERN(SPR_DOG_GROWL,
    "........",
    "........",
    "1.1.1...",
    ".11111..",
    ".1111111",
    ".111.1K1",
    ".1.1.WW2",
    "11.11...");

BBC_SPRITE_EXTERN(SPR_DOG_DOWN,
    "........",
    ".....1K1",
    "11111112",
    ".1.1..11");

/* ==== The man with the mop ==============================================
 *
 * Magenta overalls and a blue cap: nobody else in the building wears either,
 * and he is the one figure that is never a threat. Between jobs the mop rides
 * in the bucket on his cart; at a job he sweeps with it, and leans on it.
 */

BBC_SPRITE_EXTERN(SPR_JANITOR_WALK1,
    "...BBB..",
    "..BBBBBB",
    "...WKW..",
    "...WWW..",
    "....W...",
    "..MMMM..",
    ".MMMMMM.",
    "WMMMMMMW",
    "..MMMM..",
    "..MMMM..",
    "..MMMM..",
    "..MMMMM.",
    ".MM..MM.",
    ".MM...MM",
    "MM....MM",
    "BB.....B");

BBC_SPRITE_EXTERN(SPR_JANITOR_WALK2,
    "...BBB..",
    "..BBBBBB",
    "...WKW..",
    "...WWW..",
    "....W...",
    "..MMMM..",
    "..MMMM..",
    "..MMMM..",
    "..MWMM..",
    "..MMMM..",
    "..MMMM..",
    "...MMM..",
    "...MMM..",
    "..MM.M..",
    "..MM.MM.",
    "..BB.BBB");

/* Leaning on the mop. */
BBC_SPRITE_EXTERN(SPR_JANITOR_PAUSE,
    "...BBB..",
    "..BBBBB.",
    "...WKW..",
    "...WWW.Y",
    "....W..Y",
    "..MMMMWY",
    "..MMMM.Y",
    "..MMMM.Y",
    "..MMMM.Y",
    "..MMMM.Y",
    "..MMMM.Y",
    "..MM.MMY",
    "..MM.MMY",
    "..MM.MMY",
    "..MM.MMY",
    "..BB.BWW");

/* Working: both hands on the pole and the mop head out on the floor ahead of
 * him, swung out and drawn back. */
BBC_SPRITE_EXTERN(SPR_JANITOR_MOP1,
    "...BBB..",
    "..BBBBB.",
    "...WKW..",
    "...WWW..",
    "....W...",
    "..MMMM..",
    "..MMMMW.",
    "..MMMMY.",
    "..MMMMWY",
    "..MMMM.Y",
    "..MMMM.Y",
    "..MM.MMY",
    "..MM.MMY",
    "..MM.MMY",
    "..MM.MWW",
    "..BB.WWW");

BBC_SPRITE_EXTERN(SPR_JANITOR_MOP2,
    "...BBB..",
    "..BBBBB.",
    "...WKW..",
    "...WWW..",
    "....W...",
    "..MMMM..",
    "..MMMMW.",
    "..MMMWY.",
    "..MMMMY.",
    "..MMMMY.",
    "..MMMMY.",
    "..MM.MY.",
    "..MM.YM.",
    "..MM.YM.",
    "..MMWWWW",
    "..BBWWWW");

/* His cart, trailing behind him with the push bar at his end: the blue bin
 * with its bag, the yellow mop bucket with the wringer on it and the mop
 * standing in it, the white deck on its castors. Faces right, which is the
 * way he pushes it from. Six by twelve. */
BBC_SPRITE_EXTERN(SPR_JANITOR_CART,
    ".Y...W",
    ".Y...W",
    ".YBBBW",
    ".YBBBW",
    ".YBBBW",
    "WWWBBW",
    "YYYBBW",
    "YYYBBW",
    "YYYBBW",
    "WWWWWW",
    ".W..W.",
    ".W..W.");

/* The same cart with the mop out of it, in his hands. */
BBC_SPRITE_EXTERN(SPR_JANITOR_CART_EMPTY,
    ".....W",
    ".....W",
    "..BBBW",
    "..BBBW",
    "..BBBW",
    "WWWBBW",
    "YYYBBW",
    "YYYBBW",
    "YYYBBW",
    "WWWWWW",
    ".W..W.",
    ".W..W.");

/* ==== Staff and visitors ================================================
 *
 * Slots: '1' top, '2' legs and shoes, '3' hair, '4' the case a visitor has
 * not thought to drop. Seven pixels wide on a box of six, and no hat on
 * anyone, which is what keeps a crowd of them from reading as men with guns.
 * Nobody here is drawn calm: STAND is the startle, hands up at the face.
 */

BBC_SPRITE_EXTERN(SPR_CIVILIAN_STAND,
    "..333..",
    ".33333.",
    "W3WKW.W",
    "W.WWWWW",
    ".1.W.1.",
    ".11111.",
    ".11111.",
    ".11111.",
    ".11111.",
    ".11111.",
    "..222..",
    "..2.2..",
    "..2.2..",
    "..2.2..",
    "..2.2..",
    ".22.22.");

/* Running for the doors with both hands flung up, pitched forward. */
BBC_SPRITE_EXTERN(SPR_CIVILIAN_RUN1,
    "W..333W",
    "1.33331",
    "1..WKW1",
    ".1.WWW1",
    "..11W1.",
    "..1111.",
    "..1111.",
    "..1111.",
    "..1111.",
    "..222..",
    "..2222.",
    ".22..2.",
    ".2...22",
    "22....2",
    "2......",
    "2......");

BBC_SPRITE_EXTERN(SPR_CIVILIAN_RUN2,
    ".W.333W",
    ".133331",
    "..1WKW1",
    "..1WWW1",
    "...1W1.",
    "..1111.",
    "..1111.",
    "..1111.",
    "..1111.",
    "..222..",
    "...22..",
    "...22..",
    "...222.",
    "...2.2.",
    "..22.2.",
    "....22.");

/* The visitor with the case, who runs holding it: the case is the joke and
 * the tell. Two frames. */
BBC_SPRITE_EXTERN(SPR_CIVILIAN_CASE1,
    "...333.",
    "..33333",
    "...WKW.",
    "...WWW.",
    "....W..",
    "..1111.",
    ".111111",
    "11111.W",
    "1.1111.",
    "1.1111.",
    "W.222..",
    "4442222",
    "444..2.",
    "4442..2",
    "22.....",
    "2......");

BBC_SPRITE_EXTERN(SPR_CIVILIAN_CASE2,
    "...333.",
    "..33333",
    "...WKW.",
    "...WWW.",
    "....W..",
    "..1111.",
    "..11111",
    "..111.W",
    "..1111.",
    ".11111.",
    ".W222..",
    "444222.",
    "4442...",
    "444222.",
    "...2.2.",
    "..22.2.");

/* Down on one knee where the run tripped, one hand braced on the floor. */
BBC_SPRITE_EXTERN(SPR_CIVILIAN_STUMBLE,
    ".......",
    ".......",
    ".......",
    ".......",
    ".......",
    "...333.",
    "..33333",
    "...WKW.",
    "...WWW.",
    "..1111.",
    ".111111",
    ".1111.1",
    ".1111..",
    "..222.W",
    "2222..W",
    "2..2...");

/* Behind the desk from the waist up — the counter is the world's to draw. */
BBC_SPRITE_EXTERN(SPR_RECEPTIONIST_IDLE,
    "..333..",
    ".33333.",
    ".3WWK3.",
    "..WWW..",
    "...W...",
    ".11W11.",
    "1111111",
    "1111111",
    "W11111W");

BBC_SPRITE_EXTERN(SPR_RECEPTIONIST_PHONE,
    "..333..",
    ".33333K",
    ".3WWKKK",
    "..WWWW.",
    "...W.W.",
    ".11W11.",
    "111111.",
    "1111111",
    "W111111");

BBC_SPRITE_EXTERN(SPR_RECEPTIONIST_ALARM,
    "W.....W",
    "1.333.1",
    "1333331",
    "..WWK..",
    "...W...",
    ".11W11.",
    ".11111.",
    "1111111",
    "1111111");

/* The front desk, standing to her post, whole: blue suit, white blouse, red
 * hair, the brass earpiece and lanyard that say which side of the counter this
 * is. Seven by sixteen. The counter in front of her is the world's. */
BBC_SPRITE_EXTERN(SPR_RECEPTIONIST_STAND,
    "..RRR..",
    ".RRRRR.",
    ".RYWKW.",
    ".R.WWW.",
    ".R..W..",
    "..BWB..",
    ".BBWBB.",
    ".BYWBB.",
    ".BYBBB.",
    ".BBWWB.",
    "..BBB..",
    "..BBB..",
    "..B.B..",
    "..B.B..",
    "..B.B..",
    ".RR.RR.");

/* Walking an errand with the folder tucked under the far arm. */
BBC_SPRITE_EXTERN(SPR_RECEPTIONIST_WALK1,
    "..RRR..",
    ".RRRRR.",
    ".RYWKW.",
    ".R.WWW.",
    ".R..W..",
    ".BBWBB.",
    "YYBWBB.",
    "YYYWBBW",
    "YYYBBB.",
    ".BBBBB.",
    "..BBB..",
    "..BBBB.",
    ".BB..B.",
    ".B...BB",
    "BB....B",
    "R.....R");

BBC_SPRITE_EXTERN(SPR_RECEPTIONIST_WALK2,
    "..RRR..",
    ".RRRRR.",
    ".RYWKW.",
    ".R.WWW.",
    ".R..W..",
    ".BBWBB.",
    "YYBWBB.",
    "YYYWBB.",
    "YYYBWB.",
    ".BBBBB.",
    "..BBB..",
    "...BB..",
    "...BB..",
    "..B.B..",
    "..B.BB.",
    "..RR.R.");

/* Reading the folder out, held up in front of her: whatever the errand is,
 * this is the only part of it the player ever sees. */
BBC_SPRITE_EXTERN(SPR_RECEPTIONIST_READ,
    "..RRR..",
    ".RRRRR.",
    ".RYWKW.",
    ".R.WWW.",
    ".R..W..",
    ".BBWYYY",
    ".BBWYWY",
    ".BYWYWY",
    ".BYBYYY",
    ".BBBBW.",
    "..BBB..",
    "..BBB..",
    "..B.B..",
    "..B.B..",
    "..B.B..",
    ".RR.RR.");

/* ==== Birds =============================================================
 *
 * A gull, wings up and wings down. Faces right.
 */

BBC_SPRITE_EXTERN(SPR_BIRD_UP,
    ".W....W",
    "..W..W.",
    "...WWW.",
    "WWWWWWY",
    "..WWW..",
    "...K...");

BBC_SPRITE_EXTERN(SPR_BIRD_DOWN,
    ".......",
    ".......",
    "...WWW.",
    "WWWWWWY",
    ".WWWWW.",
    "W.....W");

/* ==== What comes off the wall ===========================================
 *
 * Four by seven over the fourteen-pixel box, upright and on its side: a
 * thing in the air tumbles a quarter turn at a time, which is all the
 * rotation a BBC sprite ever had. The pot is terracotta and leaf, the bottle
 * glass, the brick the building's own masonry.
 */

BBC_SPRITE_EXTERN(SPR_THROWN_POT,
    ".GG.",
    "GGGG",
    ".GG.",
    "RRRR",
    "RRRR",
    ".RR.",
    ".RR.");

BBC_SPRITE_EXTERN(SPR_THROWN_POT_SIDE,
    "....",
    "....",
    "RR.G",
    "RRRG",
    "RRRG",
    "RR.G",
    "....");

BBC_SPRITE_EXTERN(SPR_THROWN_BOTTLE,
    ".W..",
    ".G..",
    ".G..",
    "GGG.",
    "GWG.",
    "GWG.",
    "GGG.");

BBC_SPRITE_EXTERN(SPR_THROWN_BOTTLE_SIDE,
    "....",
    "....",
    "GGG.",
    "GWWG",
    "GGG.",
    "....",
    "....");

BBC_SPRITE_EXTERN(SPR_THROWN_BRICK,
    "....",
    "RRRR",
    "RYRR",
    "RRRR",
    "RRYR",
    "RRRR",
    "....");

BBC_SPRITE_EXTERN(SPR_THROWN_BRICK_SIDE,
    "....",
    "....",
    "RRRR",
    "RRYR",
    "RRRR",
    "....",
    "....");

/* ==== Ellen =============================================================
 *
 * Yellow hair and the red coat: the one figure the whole night is about, and
 * the only one in red who is not a threat.
 */

BBC_SPRITE_EXTERN(SPR_ELLEN_STAND,
    "..YYY..",
    ".YYYYY.",
    ".YWKWY.",
    ".YWWWY.",
    "...W...",
    "..RRR..",
    ".RRRRR.",
    ".RRRRR.",
    ".WRRRW.",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    "..W.W..",
    "..W.W..",
    "..W.W..",
    ".KK.KK.");

BBC_SPRITE_EXTERN(SPR_ELLEN_WALK1,
    "..YYY..",
    ".YYYYY.",
    ".YWKWY.",
    ".YWWWY.",
    "...W...",
    "..RRR..",
    ".RRRRR.",
    "WRRRRRW",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    "RRRRRRR",
    ".W...W.",
    "W.....W",
    "W.....W",
    "K.....K");

BBC_SPRITE_EXTERN(SPR_ELLEN_WALK2,
    "..YYY..",
    ".YYYYY.",
    ".YWKWY.",
    ".YWWWY.",
    "...W...",
    "..RRR..",
    ".RRRRR.",
    ".RRWRR.",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    "..WW...",
    "...W...",
    "...WW..",
    "..KK...");

/* Taped at the wrists, hands in front of her. */
BBC_SPRITE_EXTERN(SPR_ELLEN_BOUND1,
    "..YYY..",
    ".YYYYY.",
    ".YWKWY.",
    ".YWWWY.",
    "...W...",
    "..RRR..",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRWW",
    ".RRRRR.",
    ".RRRRR.",
    "RRRRRRR",
    ".W...W.",
    "W.....W",
    "W.....W",
    "K.....K");

BBC_SPRITE_EXTERN(SPR_ELLEN_BOUND2,
    "..YYY..",
    ".YYYYY.",
    ".YWKWY.",
    ".YWWWY.",
    "...W...",
    "..RRR..",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRWW",
    ".RRRRR.",
    ".RRRRR.",
    ".RRRRR.",
    "..WW...",
    "...W...",
    "...WW..",
    "..KK...");

/* ==== Voss ==============================================================
 *
 * White hair, magenta coat to the knee, white shirt at the collar. Magenta is
 * the story's colour and nobody else in the game wears it as a coat.
 */

BBC_SPRITE_EXTERN(SPR_VOSS_STAND,
    "...WWW..",
    "..WWWWW.",
    "..WWKW..",
    "...WWW..",
    "....W...",
    "..MWWM..",
    ".MMWWMM.",
    ".MMMWMM.",
    ".MMMMMM.",
    ".WMMMMW.",
    ".MMMMMM.",
    ".MMMMMM.",
    "..MM.MM.",
    "..BB.BB.",
    "..BB.BB.",
    "..KK.KKK");

BBC_SPRITE_EXTERN(SPR_VOSS_WALK,
    "...WWW..",
    "..WWWWW.",
    "..WWKW..",
    "...WWW..",
    "....W...",
    "..MWWM..",
    ".MMWWMM.",
    ".MMMWMM.",
    "WMMMMMMW",
    ".MMMMMM.",
    ".MMMMMMM",
    "MMMMMMMM",
    ".BB...BB",
    ".BB...BB",
    "BB.....B",
    "KK....KK");

/* The pistol out, level. */
BBC_SPRITE_EXTERN(SPR_VOSS_AIM,
    "...WWW..",
    "..WWWWW.",
    "..WWKW..",
    "...WWW..",
    "....W...",
    "..MWWM..",
    ".MMWWMMM",
    ".MMMWMWY",
    ".MMMMMM.",
    ".MMMMMM.",
    ".MMMMMM.",
    ".MMMMMM.",
    "..MM.MM.",
    "..BB.BB.",
    "..BB.BB.",
    "..KK.KKK");

/* ==== The crew ==========================================================
 *
 * The men who take her at the kerb: '1' fatigues, '2' balaclava. Blue rather
 * than black, because a black figure on a BBC is a hole in the screen, and a
 * red slot for the eyes so a balaclava is a balaclava.
 */

BBC_SPRITE_EXTERN(SPR_AGENT_STAND,
    "...222..",
    "..2222..",
    "..2RR2..",
    "..2222..",
    "...22...",
    "..1111..",
    "..1111W.",
    "..11W11.",
    "..1W111.",
    "..W1111.",
    "..1111..",
    "..11.11.",
    "..11.11.",
    "..11.11.",
    "..11.11.",
    "..KK.KKK");

BBC_SPRITE_EXTERN(SPR_AGENT_WALK,
    "...222..",
    "..2222..",
    "..2RR2..",
    "..2222..",
    "...22...",
    "..1111..",
    "..1111W.",
    "..11W11.",
    "..1W111.",
    "..W1111.",
    "..1111..",
    "..11111.",
    ".11..11.",
    ".11...11",
    "11....11",
    "KK.....K");

BBC_SPRITE_EXTERN(SPR_AGENT_AIM,
    "...222..",
    "..2222..",
    "..2RR2..",
    "..2222..",
    "...22...",
    "..1111..",
    "..11WWWW",
    "..1111..",
    "..1111..",
    "..1111..",
    "..1111..",
    "..11.11.",
    "..11.11.",
    ".11..11.",
    ".11..11.",
    ".KK..KKK");

/* ==== What is carried, picked up and counted ============================ */

BBC_SPRITE_EXTERN(SPR_ICON_HEART,
    "RR.RR",
    "RRRRR",
    "RRRRR",
    ".RRR.",
    "..R..");

/* An empty heart is the outline of one: a socket, not a pale heart. */
BBC_SPRITE_EXTERN(SPR_ICON_HEART_EMPTY,
    "RR.RR",
    "R.R.R",
    "R...R",
    ".R.R.",
    "..R..");

/* One round: white tip, brass case. The HUD lays them side by side. */
BBC_SPRITE_EXTERN(SPR_ICON_ROUND,
    "W",
    "Y",
    "Y",
    "Y",
    "Y");

BBC_SPRITE_EXTERN(SPR_ICON_ROUND_SPENT,
    "B",
    "B",
    "B",
    "B",
    "B");

/* The key card: cyan, a white stripe, a yellow chip. */
BBC_SPRITE_EXTERN(SPR_ICON_CARD,
    "CCCC",
    "WWWW",
    "CCCC",
    "CYYC",
    "CYYC",
    "CCCC");

BBC_SPRITE_EXTERN(SPR_ICON_MEDKIT,
    ".WWW.",
    "WWRWW",
    "WRRRW",
    "WWRWW",
    "WWWWW");

/* A frag is an egg: round, green, the white pin and its ring on top. */
BBC_SPRITE_EXTERN(SPR_ICON_GRENADE,
    "W.W",
    "GGG",
    "GYG",
    "GGG",
    ".G.");

/* A charge is a can, not an egg — square-shouldered, white, banded in yellow
 * and capped in cyan — because one of the two is going to kill whoever is
 * standing next to it. */
BBC_SPRITE_EXTERN(SPR_ICON_FLASH,
    ".C.",
    "WWW",
    "YYY",
    "WWW",
    "WWW");

BBC_SPRITE_EXTERN(SPR_ICON_ROCKET,
    "Y.WWW.",
    "YWWWWR",
    "Y.WWW.");

/* A bolt of bright steel: small, white and plain, so it is never mistaken for
 * either of the two things above it that are about to go off. */
BBC_SPRITE_EXTERN(SPR_ICON_DECOY,
    "WC",
    "WW",
    "CW");

/* A sheet of the docket: white paper, black lines of type, the magenta stamp
 * that makes it the story's rather than the building's. */
BBC_SPRITE_EXTERN(SPR_ICON_EVIDENCE,
    "WWWW",
    "WKKW",
    "WWWW",
    "WKKW",
    "WWMW",
    "WWWW");

BBC_SPRITE_EXTERN(SPR_ICON_PISTOL,
    "WWWY",
    "WW..",
    "W...");

BBC_SPRITE_EXTERN(SPR_ICON_BAZOOKA,
    "...Y....",
    "GGGGGGGK",
    "GGGGGGGG",
    ".....Y..");

/* A round in flight: two pixels of brass for the player's, red for theirs is
 * the caller's choice of slot. */
BBC_SPRITE_EXTERN(SPR_SHOT,
    "11");

/* The flash at a muzzle: a star of white with a yellow heart. */
BBC_SPRITE_EXTERN(SPR_MUZZLE_FLASH,
    "W.W",
    ".Y.",
    "W.W");

/* A shot fired in play, where the flash has to be seen at once and from across
 * a room: five rows, because three rows of a fat pixel are six logical ones
 * and read as a bar rather than a star. The burst faces right, its long arm
 * along the line of fire; the rifle's has the enemy's red at its points. The
 * star is the same for a shot straight up or down, and is symmetrical so one
 * drawing serves both. */
BBC_SPRITE_EXTERN(SPR_MUZZLE_BURST,
    "W..W",
    ".YY.",
    "YWWW",
    ".YY.",
    "W..W");

BBC_SPRITE_EXTERN(SPR_MUZZLE_FLASH_RIFLE,
    "R..R",
    ".YY.",
    "YWWW",
    ".YY.",
    "R..R");

BBC_SPRITE_EXTERN(SPR_MUZZLE_STAR,
    "W.W",
    ".Y.",
    "YWY",
    ".Y.",
    "W.W");

/* ==== The standard paints ============================================== */

const BbcColour BBC_GUARD_PAINT[BBC_SPRITE_SLOTS] = {
    BBC_GREEN, BBC_GREEN, BBC_GREEN, BBC_YELLOW};
const BbcColour BBC_GUARD_HURT_PAINT[BBC_SPRITE_SLOTS] = {
    BBC_YELLOW, BBC_GREEN, BBC_GREEN, BBC_RED};
/* The heavy is red where a guard is green and white at the helmet and the
 * plate: the one man on the floor whose head a boot will not settle. */
const BbcColour BBC_HEAVY_PAINT[BBC_SPRITE_SLOTS] = {
    BBC_RED, BBC_WHITE, BBC_RED, BBC_YELLOW};
const BbcColour BBC_HEAVY_HURT_PAINT[BBC_SPRITE_SLOTS] = {
    BBC_MAGENTA, BBC_WHITE, BBC_RED, BBC_YELLOW};
/* A red dog with a yellow nose: brown on a BBC is red, and the yellow is
 * what finds the front of it. */
const BbcColour BBC_DOG_PAINT[BBC_SPRITE_SLOTS] = {
    BBC_RED, BBC_YELLOW, BBC_RED, BBC_RED};
const BbcColour BBC_AGENT_PAINT[BBC_SPRITE_SLOTS] = {
    BBC_BLUE, BBC_BLUE, BBC_BLUE, BBC_BLUE};
/* The office worker in his shirt sleeves, the woman off the front desk in
 * her suit, the visitor and his case, and a fourth for a crowd. Never cyan
 * (Chuck) and never green (the guards). */
const BbcColour BBC_CIVILIAN_PAINT[BBC_CIVILIAN_PAINTS][BBC_SPRITE_SLOTS] = {
    {BBC_WHITE, BBC_BLUE, BBC_YELLOW, BBC_RED},
    {BBC_BLUE, BBC_BLUE, BBC_RED, BBC_YELLOW},
    {BBC_YELLOW, BBC_RED, BBC_WHITE, BBC_WHITE},
    {BBC_MAGENTA, BBC_BLUE, BBC_YELLOW, BBC_WHITE},
};
const BbcColour BBC_PLAYER_SHOT_PAINT[BBC_SPRITE_SLOTS] = {
    BBC_YELLOW, BBC_YELLOW, BBC_YELLOW, BBC_YELLOW};
const BbcColour BBC_ENEMY_SHOT_PAINT[BBC_SPRITE_SLOTS] = {
    BBC_RED, BBC_RED, BBC_RED, BBC_RED};

const BbcSprite *const BBC_CAST_ALL[] = {
    &SPR_CHUCK_STAND, &SPR_CHUCK_WALK1, &SPR_CHUCK_WALK2, &SPR_CHUCK_WALK3,
    &SPR_CHUCK_LAND, &SPR_CHUCK_JUMP, &SPR_CHUCK_FALL, &SPR_CHUCK_AIM,
    &SPR_CHUCK_AIM_UP, &SPR_CHUCK_AIM_DOWN, &SPR_CHUCK_KNIFE, &SPR_CHUCK_THROW,
    &SPR_CHUCK_SHOULDER, &SPR_CHUCK_DRAG, &SPR_CHUCK_DRAG2, &SPR_CHUCK_HACK1,
    &SPR_CHUCK_HACK2, &SPR_CHUCK_CLIMB, &SPR_CHUCK_CLIMB_AIM,
    &SPR_CHUCK_CLIMB_AIM_UP, &SPR_CHUCK_CLIMB_AIM_DOWN, &SPR_CHUCK_CRAWL1,
    &SPR_CHUCK_CRAWL2, &SPR_CHUCK_DEAD, &SPR_BAZOOKA, &SPR_BAZOOKA_UP,
    &SPR_BAZOOKA_DOWN,
    &SPR_GUARD_STAND, &SPR_GUARD_WALK1, &SPR_GUARD_WALK2, &SPR_GUARD_AIM,
    &SPR_GUARD_AIM_UP, &SPR_GUARD_AIM_DOWN, &SPR_GUARD_TALK, &SPR_GUARD_TALK2,
    &SPR_GUARD_RADIO, &SPR_GUARD_REACH, &SPR_GUARD_DAZZLED, &SPR_GUARD_CLIMB,
    &SPR_GUARD_DOWN, &SPR_HEAVY_PLATE, &SPR_HEAVY_HELM,
    &SPR_DOG_STAND, &SPR_DOG_RUN1, &SPR_DOG_RUN2, &SPR_DOG_BITE, &SPR_DOG_GROWL,
    &SPR_DOG_DOWN,
    &SPR_JANITOR_WALK1, &SPR_JANITOR_WALK2, &SPR_JANITOR_PAUSE,
    &SPR_JANITOR_MOP1, &SPR_JANITOR_MOP2, &SPR_JANITOR_CART,
    &SPR_JANITOR_CART_EMPTY,
    &SPR_CIVILIAN_STAND, &SPR_CIVILIAN_RUN1, &SPR_CIVILIAN_RUN2,
    &SPR_CIVILIAN_CASE1, &SPR_CIVILIAN_CASE2, &SPR_CIVILIAN_STUMBLE,
    &SPR_RECEPTIONIST_IDLE, &SPR_RECEPTIONIST_PHONE, &SPR_RECEPTIONIST_ALARM,
    &SPR_RECEPTIONIST_STAND, &SPR_RECEPTIONIST_WALK1, &SPR_RECEPTIONIST_WALK2,
    &SPR_RECEPTIONIST_READ,
    &SPR_BIRD_UP, &SPR_BIRD_DOWN,
    &SPR_THROWN_POT, &SPR_THROWN_POT_SIDE, &SPR_THROWN_BOTTLE,
    &SPR_THROWN_BOTTLE_SIDE, &SPR_THROWN_BRICK, &SPR_THROWN_BRICK_SIDE,
    &SPR_ELLEN_STAND, &SPR_ELLEN_WALK1, &SPR_ELLEN_WALK2, &SPR_ELLEN_BOUND1,
    &SPR_ELLEN_BOUND2,
    &SPR_VOSS_STAND, &SPR_VOSS_WALK, &SPR_VOSS_AIM,
    &SPR_AGENT_STAND, &SPR_AGENT_WALK, &SPR_AGENT_AIM,
    &SPR_ICON_HEART, &SPR_ICON_HEART_EMPTY, &SPR_ICON_ROUND, &SPR_ICON_ROUND_SPENT,
    &SPR_ICON_CARD, &SPR_ICON_MEDKIT, &SPR_ICON_GRENADE, &SPR_ICON_FLASH,
    &SPR_ICON_ROCKET, &SPR_ICON_DECOY, &SPR_ICON_EVIDENCE, &SPR_ICON_PISTOL,
    &SPR_ICON_BAZOOKA, &SPR_SHOT, &SPR_MUZZLE_FLASH, &SPR_MUZZLE_BURST,
    &SPR_MUZZLE_FLASH_RIFLE, &SPR_MUZZLE_STAR,
};

const int BBC_CAST_COUNT =
    (int)(sizeof(BBC_CAST_ALL) / sizeof(BBC_CAST_ALL[0]));
