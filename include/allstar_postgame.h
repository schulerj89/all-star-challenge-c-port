#ifndef ALLSTAR_POSTGAME_H
#define ALLSTAR_POSTGAME_H

#include "allstar_types.h"

/*
 * Postgame screen spine, ported from $10A5..$10FF plus the $16xx/$17xx drawing
 * helpers it calls.
 *
 * Four entry stubs each stash a screen index in $FF8D and fall into the shared
 * tail at $10B7, which pages in bank 1, loads the postgame tiles unless the
 * mode is $01, and then dispatches twice: once through the table at $10D9 by
 * $FF8D, and for route 0 again through the table at $10E4 by the game mode in
 * $FF8F.  The tournament reaches this path from $0F63/$0F91 through $10A5.
 */

#define ALLSTAR_POSTGAME_NAME_1       0xC23Cu   /* player 1 name buffer      */
#define ALLSTAR_POSTGAME_NAME_2       0xC255u   /* player 2 name buffer      */
#define ALLSTAR_POSTGAME_SCORE_1      0xC133u   /* player 1 BCD score word   */
#define ALLSTAR_POSTGAME_SCORE_2      0xC135u   /* player 2 BCD score word   */
#define ALLSTAR_POSTGAME_DIGITS       0xC1FBu   /* $1726 writes three tiles  */
#define ALLSTAR_POSTGAME_DIGIT_BASE   0xC1u     /* tile code for '0'         */
#define ALLSTAR_POSTGAME_BLANK_TILE   0x00u
#define ALLSTAR_POSTGAME_START_MASK   0x08u     /* $FFAE bit 3               */

/* $10A5 / $10AD / $10B1 / $10B5: the value each stub puts in $FF8D. */
typedef enum {
    ALLSTAR_POSTGAME_ENTRY_RESULT = 0,   /* $10A5, also sets $C16F */
    ALLSTAR_POSTGAME_ENTRY_ONE    = 1,   /* $10AD */
    ALLSTAR_POSTGAME_ENTRY_SELECT = 2,   /* $10B1, from $28D9 */
    ALLSTAR_POSTGAME_ENTRY_THREE  = 3    /* $10B5 */
} AllStarPostgameEntry;

/* Targets of the $10D9 table, indexed by $FF8D. */
typedef enum {
    ALLSTAR_POSTGAME_ROUTE_BY_MODE = 0,  /* $10E1, dispatches again on $FF8F */
    ALLSTAR_POSTGAME_ROUTE_1343,
    ALLSTAR_POSTGAME_ROUTE_139B,
    ALLSTAR_POSTGAME_ROUTE_146F
} AllStarPostgameRoute;

typedef struct {
    uint8_t screen;        /* $FF8D as the stub left it                      */
    bool sets_result_flag; /* $10A5 alone writes $C16F                       */
    uint8_t bank;          /* $10B9-$10BB pages in bank 1                    */
    bool loads_tiles;      /* the $0444/$047E/$050F block, skipped when mode 1 */
    AllStarPostgameRoute route;
    uint16_t handler;      /* $10E4 target, or the $10D9 target for other routes */
} AllStarPostgameEnter;

/* What $10FA draws, in the order it draws it. */
typedef enum {
    ALLSTAR_POSTGAME_DRAW_PANEL = 0,   /* $1657, three stacked rows      */
    ALLSTAR_POSTGAME_DRAW_NAME_1,      /* $175B, leading spaces skipped  */
    ALLSTAR_POSTGAME_DRAW_SCORE_1,     /* $1770                          */
    ALLSTAR_POSTGAME_DRAW_NAME_2,      /* $1760, leading spaces skipped  */
    ALLSTAR_POSTGAME_DRAW_SCORE_2,     /* $1775                          */
    ALLSTAR_POSTGAME_DRAW_NAME_1_RAW,  /* $1751, no space skip           */
    ALLSTAR_POSTGAME_DRAW_NAME_2_RAW,  /* $1756, no space skip           */
    ALLSTAR_POSTGAME_DRAW_ATTEMPTS,    /* $11B4, the $FF98 byte via $177B */
    ALLSTAR_POSTGAME_DRAW_TOTAL_HI,    /* $1229, two tiles from $C1FB     */
    ALLSTAR_POSTGAME_DRAW_TOTAL_LO,    /* $1234, the next two tiles       */
    ALLSTAR_POSTGAME_DRAW_WORD_C137,   /* $123A, $1778 on $C137           */
    ALLSTAR_POSTGAME_DRAW_WORD_C139,   /* $1243, $1778 on $C139           */
    ALLSTAR_POSTGAME_DRAW_TEXT_GAME,   /* $1354, the $1394 string         */
    ALLSTAR_POSTGAME_DRAW_MATCH_DIGIT, /* $135D, $C0BE rendered as a tile */
    ALLSTAR_POSTGAME_DRAW_TEXT_VS      /* $1374, the $1399 string         */
} AllStarPostgameDrawKind;

/* What one of those writers reads and how it preprocesses it. */
typedef struct {
    uint16_t routine;    /* the ROM entry this kind corresponds to */
    uint16_t source;     /* name buffer, or the BCD score word     */
    bool skip_spaces;    /* $1769 runs first                       */
    bool is_score;       /* goes through $1778 then $177B          */
} AllStarPostgameDrawDetail;

typedef struct {
    AllStarPostgameDrawKind kind;
    uint8_t d;             /* DE as the ROM loads it before each call */
    uint8_t e;
} AllStarPostgameDraw;

#define ALLSTAR_POSTGAME_LAYOUT_OPS   6
#define ALLSTAR_POSTGAME_PANEL_ROWS   3

/* What $10EE does with the $28E1 verdict. */
typedef enum {
    ALLSTAR_POSTGAME_RESULT_DRAW_ONLY = 0,  /* winner != 0: jr $10FA and return */
    ALLSTAR_POSTGAME_RESULT_DRAW_THEN_12C9  /* a tie: call $10FA then jp $12C9  */
} AllStarPostgameResult;

/* $1638 wait loop. */
typedef enum {
    ALLSTAR_POSTGAME_HOLD_WAITING = 0,
    ALLSTAR_POSTGAME_HOLD_INPUT,
    ALLSTAR_POSTGAME_HOLD_TIMEOUT
} AllStarPostgameHold;

const uint16_t* allstar_postgame_screen_table(int *count);   /* $10D9 */
const uint16_t* allstar_postgame_mode_table(int *count);     /* $10E4 */

/* $10A5..$10E3: run a stub and both dispatches for the given mode. */
void allstar_postgame_enter(AllStarPostgameEntry entry, uint8_t mode, AllStarPostgameEnter *out);

/* $10EE */
AllStarPostgameResult allstar_postgame_result_route(uint8_t winner);

/* $10FA: the six draw calls, with the exact DE the ROM loads for each. */
int allstar_postgame_final_score_layout(AllStarPostgameDraw *out, int max);

/* $1657: three rows at E, E+1, E+2 sharing column D. */
void allstar_postgame_panel_rows(uint8_t d, uint8_t e, uint16_t *sources, uint8_t *rows);

/* $1751/$1756/$175B/$1760/$1770/$1775: what each writer reads. */
bool allstar_postgame_draw_detail(AllStarPostgameDrawKind kind, AllStarPostgameDrawDetail *out);

/* $1769: index of the first byte that is not a space. */
uint8_t allstar_postgame_skip_spaces(const uint8_t *text, uint8_t length);

/* $1726: three tile codes for a BCD word, leading zeros blanked. */
void allstar_postgame_score_digits(uint16_t bcd_score, uint8_t *digits);

#define ALLSTAR_POSTGAME_SCORE_TILES  3
/* $177B: $1726 then a three-byte write from $C1FB; returns the tile count. */
int allstar_postgame_score_tiles(uint16_t bcd_score, uint8_t *tiles);

/* $146F: bank 2, $C170 = 1, then dispatch on $C181 through the $147B table. */
uint16_t allstar_postgame_route_146f(uint8_t c181, uint8_t *bank_out);

/* $1638: one frame of the hold loop. */
AllStarPostgameHold allstar_postgame_hold_step(uint16_t *frames, uint8_t hold_lock, uint8_t new_buttons);

/* ---- $1121: the Free Throw postgame screen (mode $01) ---- */

#define ALLSTAR_POSTGAME_READY_FLAG     0xF0u   /* written into a score high byte */
#define ALLSTAR_POSTGAME_ROLE_PLAYER_2  0x03u   /* $C199 */
#define ALLSTAR_POSTGAME_SCORE_1_HIGH   0xC134u
#define ALLSTAR_POSTGAME_SCORE_2_HIGH   0xC136u
#define ALLSTAR_POSTGAME_ATTEMPTS       0xFF98u
#define ALLSTAR_POSTGAME_SOUND_READY_1  0x19u
#define ALLSTAR_POSTGAME_SOUND_READY_2  0x18u
#define ALLSTAR_POSTGAME_SOUND_RESULT   0x09u
#define ALLSTAR_POSTGAME_FT_LAYOUT_OPS  3

typedef enum {
    ALLSTAR_POSTGAME_FT_DRAW = 0,   /* $112C: single player, straight to $1195 */
    ALLSTAR_POSTGAME_FT_SYNC,       /* $1152: the other side is already ready  */
    ALLSTAR_POSTGAME_FT_WAIT        /* $1168: announce, then poll until ready  */
} AllStarPostgameFreeThrowPath;

typedef struct {
    AllStarPostgameFreeThrowPath path;
    bool is_player_2;         /* $C199 == $03                                 */
    uint16_t ready_flag;      /* score high byte this side sets to $F0, or 0  */
    uint16_t poll_address;    /* byte polled until it reads $F0, or 0         */
    uint8_t announce_sound;   /* $19 or $18 on the wait path, else 0          */
} AllStarPostgameFreeThrow;

/* $1121..$1194: which of the three entry paths runs, and what it touches. */
void allstar_postgame_free_throw_entry(uint8_t role, uint8_t score_1_high, uint8_t score_2_high,
                                       AllStarPostgameFreeThrow *out);

/* $1195..$11D2: the three draw calls the screen makes. */
int allstar_postgame_free_throw_layout(bool is_player_2, AllStarPostgameDraw *out, int max);

/* ---- $11D5: the H-O-R-S-E postgame screen (mode $02) ---- */

#define ALLSTAR_POSTGAME_HORSE_MESSAGE     0x1203u  /* "IS OUT", last byte | $80 */
#define ALLSTAR_POSTGAME_HORSE_MESSAGE_LEN 0x06u
#define ALLSTAR_POSTGAME_NAME_SCRATCH      0xC1D3u

typedef struct {
    uint16_t loser_name;      /* $C23C or $C255, the name the message calls out */
    uint8_t winner;           /* $C17D, 2 when player 1 survives                */
    uint16_t message;         /* $1203                                          */
    uint8_t message_length;   /* 6                                              */
    uint8_t d;                /* $0207 */
    uint8_t e;
} AllStarPostgameHorse;

void allstar_postgame_horse(uint8_t eliminated_flag, AllStarPostgameHorse *out);

/*
 * $170D: copy a name into $C1D3, clear the bit-7 terminator, drop trailing
 * spaces, and leave exactly one space after the last real character.  Returns
 * the number of bytes written.
 */
uint8_t allstar_postgame_copy_name(const uint8_t *src, uint8_t src_length,
                                   uint8_t *out, uint8_t out_length);

/* ---- $1786: clear the background map ---- */

typedef struct {
    uint16_t base;      /* $9800 */
    uint8_t outer;      /* b */
    uint8_t inner;      /* c */
    uint8_t per_row;    /* tiles written between the $049F advances */
    uint8_t fill;       /* 0 */
} AllStarPostgameClear;

void allstar_postgame_clear_shape(AllStarPostgameClear *out);

/* ---- $1209: the Accuracy postgame screen (mode $03) ---- */

#define ALLSTAR_POSTGAME_ACCURACY_TOTAL  0xFF94u  /* word handed to bank 1 $780A */
#define ALLSTAR_POSTGAME_WORD_C137       0xC137u
#define ALLSTAR_POSTGAME_WORD_C139       0xC139u
#define ALLSTAR_POSTGAME_SOUND_ACCURACY  0x0Cu
#define ALLSTAR_POSTGAME_MUSIC_ACCURACY  0x84u
#define ALLSTAR_POSTGAME_ACC_LAYOUT_OPS  6

typedef struct {
    bool two_player;        /* $FF91 != $01 keeps the handshake            */
    bool is_player_2;
    uint16_t ready_flag;    /* score high byte this side sets to $F0       */
    uint16_t poll_address;  /* byte polled until it reads $F0              */
    uint8_t status_byte;    /* $C270; $1277 stores whatever A held, so $F0 */
    uint8_t music;          /* $DD73                                       */
} AllStarPostgameAccuracy;

void allstar_postgame_accuracy_entry(uint8_t role, uint8_t player_count,
                                     AllStarPostgameAccuracy *out);
int allstar_postgame_accuracy_layout(bool is_player_2, AllStarPostgameDraw *out, int max);

/* ---- $12A6: the Tournament postgame screen (mode $04) ---- */

#define ALLSTAR_POSTGAME_FINAL_MATCH     0x07u
#define ALLSTAR_POSTGAME_SOUND_REPLAY    0x16u
#define ALLSTAR_POSTGAME_SOUND_CHAMPION  0x15u
#define ALLSTAR_POSTGAME_MUSIC_CHAMPION  0x8Eu
#define ALLSTAR_POSTGAME_REPLAY_FRAMES   0x00F0u
#define ALLSTAR_POSTGAME_REPLAY_ENTRY    0x0B9Au  /* the One-on-One body */
#define ALLSTAR_POSTGAME_REPLAY_RETURN   0x10A5u  /* re-enters the dispatch */
#define ALLSTAR_POSTGAME_ENTRANT_1       0xFFACu
#define ALLSTAR_POSTGAME_ENTRANT_2       0xFFC5u

typedef enum {
    ALLSTAR_POSTGAME_TR_RETURN = 0,   /* $12B4: decided, back to the driver   */
    ALLSTAR_POSTGAME_TR_REPLAY,       /* $12C9: a tie replays the whole match */
    ALLSTAR_POSTGAME_TR_CHAMPION      /* $12FB: the final produced a winner   */
} AllStarPostgameTournamentPath;

typedef struct {
    AllStarPostgameTournamentPath path;
    bool is_final;            /* $C0BE == $07                                  */
    bool draws_score_panel;   /* whether $10FA runs                            */
    bool writes_tie_flag;     /* the final always writes $C192                 */
    uint8_t tie_flag;         /* $C192: 1 only when the final was tied         */
    uint8_t sound;            /* $16 on replay, $15 for the champion, else 0   */
    uint16_t hold_frames;     /* BC handed to $1638 on the replay path         */
    uint16_t champion_source; /* $FFAC or $FFC5                                */
    uint8_t music;            /* $DD73 on the champion path                    */
} AllStarPostgameTournament;

void allstar_postgame_tournament(uint8_t match_count, uint8_t winner,
                                 AllStarPostgameTournament *out);

/*
 * $1323..$1334: walk a loaded player record to the surname.  Skip spaces from
 * offset 1, step once more, step twice past a '.', then back up one.
 */
uint8_t allstar_postgame_record_surname(const uint8_t *record, uint8_t length);

/* ---- $1343: the pre-match VS screen ($FF8D == $01) ---- */

#define ALLSTAR_POSTGAME_TEXT_GAME       0x1394u  /* "GAME", last byte | $80 */
#define ALLSTAR_POSTGAME_TEXT_VS         0x1399u  /* "VS",   last byte | $80 */
#define ALLSTAR_POSTGAME_VS_HOLD_FRAMES  0x00F0u
#define ALLSTAR_POSTGAME_VS_LAYOUT_OPS   5
#define ALLSTAR_POSTGAME_MODE_TOURNAMENT 0x04u

/* $135D-$1365: the match counter becomes one tile in $C1D3. */
uint8_t allstar_postgame_match_digit(uint8_t match_count);

/* $134E..$1382: the GAME line only appears in the tournament. */
int allstar_postgame_matchup_layout(uint8_t mode, AllStarPostgameDraw *out, int max);

/* ---- $139B: the bracket display ($FF8D == $02, reached from $28D9) ---- */

#define ALLSTAR_POSTGAME_BRACKET_HOLD_FRAMES 0x0384u
#define ALLSTAR_POSTGAME_BRACKET_MAX_SLOTS   8
#define ALLSTAR_POSTGAME_BRACKET_FULL        0x04u  /* $C17F showing all eight */

typedef struct {
    uint16_t slot;   /* the bracket byte handed to $1464 */
    uint8_t d;
    uint8_t e;
} AllStarPostgameBracketEntry;

typedef struct {
    bool draws;             /* $C17F == $01 returns without drawing  */
    uint8_t sound;          /* looked up in the $146B table          */
    uint8_t count;          /* eight entrants, or four semifinalists */
    uint16_t hold_frames;   /* $0384                                 */
    AllStarPostgameBracketEntry entries[ALLSTAR_POSTGAME_BRACKET_MAX_SLOTS];
} AllStarPostgameBracket;

void allstar_postgame_bracket(uint8_t stage, AllStarPostgameBracket *out);

/* ---- $147B family: the bracket chooser ($FF8D == $03, dispatched on $C181) ---- */

#define ALLSTAR_POSTGAME_CHOOSER_MASK      0xCBu  /* buttons $1533 accepts   */
#define ALLSTAR_POSTGAME_CHOOSER_CONFIRM   0x08u  /* $1537, Start            */
#define ALLSTAR_POSTGAME_CHOOSER_MAX_NAMES 4
#define ALLSTAR_POSTGAME_CHOOSER_CELLS     6
#define ALLSTAR_POSTGAME_CHOOSER_ROWS      3
#define ALLSTAR_POSTGAME_CHOOSER_COLUMN    0x0Bu

/* The four $147B entry points, in table order. */
typedef enum {
    ALLSTAR_POSTGAME_CHOOSER_R1_BY_PICKER = 0,  /* $1483 */
    ALLSTAR_POSTGAME_CHOOSER_R1_RIGHT,          /* $14B6 */
    ALLSTAR_POSTGAME_CHOOSER_R2_BY_PICKER,      /* $1493 */
    ALLSTAR_POSTGAME_CHOOSER_R2_RIGHT           /* $14BD */
} AllStarPostgameChooserEntry;

typedef struct {
    uint16_t slot;   /* bracket byte, walked backwards from the list end */
    uint8_t d;
    uint8_t e;
} AllStarPostgameChooserName;

typedef struct {
    uint16_t list_end;    /* HL on entry                                    */
    bool pair_only;       /* B: false lists four names, true lists two      */
    uint8_t sound;        /* $0F, $10, $11 or $12                           */
    uint8_t count;
    AllStarPostgameChooserName names[ALLSTAR_POSTGAME_CHOOSER_MAX_NAMES];
} AllStarPostgameChooser;

/* $1483/$14B6/$1493/$14BD into the shared $14C2..$150C body. */
void allstar_postgame_chooser(AllStarPostgameChooserEntry entry, uint8_t player_count,
                              uint8_t picker, AllStarPostgameChooser *out);

/* $1521..$1531: whose new-input byte the loop reads. */
uint8_t allstar_postgame_chooser_buttons(uint8_t player_count, uint8_t picker,
                                         uint8_t player_1_new, uint8_t player_2_new);

typedef enum {
    ALLSTAR_POSTGAME_CHOOSER_IDLE = 0,
    ALLSTAR_POSTGAME_CHOOSER_TOGGLED,
    ALLSTAR_POSTGAME_CHOOSER_CONFIRMED
} AllStarPostgameChooserInput;

/* $151B..$1553: one pass of the selection loop. */
AllStarPostgameChooserInput allstar_postgame_chooser_step(uint8_t hold_lock, uint8_t buttons,
                                                          uint8_t *selection);

typedef struct {
    uint16_t source;   /* the row art the ROM points HL at */
    uint8_t d;
    uint8_t e;
} AllStarPostgameChooserCell;

/* $1554..$15AA: two three-row boxes that swap rows as the selection moves. */
int allstar_postgame_chooser_layout(uint8_t selection, AllStarPostgameChooserCell *out, int max);

/* ---- $1699..$16F1: the flashing winner banner ---- */

#define ALLSTAR_BANNER_WINS_TEXT   0x16F2u  /* $16D7, "WINS" + $84   */
#define ALLSTAR_BANNER_WINS_BYTES  0x0005u  /* $16DA, bc for $0496   */
#define ALLSTAR_BANNER_TIE_TEXT    0x16F7u  /* $16BB, "A TIE" + $84  */
#define ALLSTAR_BANNER_BLANK_TEXT  0x16FDu  /* $169C, fifteen spaces */
#define ALLSTAR_BANNER_NAME_BUFFER 0xC1D3u  /* $16E0                 */
#define ALLSTAR_BANNER_HORSE_MODE  0x02u    /* $16A4, $FF8F          */

/*
 * The caller passes `$FF8B & $08`, so the banner alternates with the frame
 * counter $276D increments: eight frames of the message, eight of the blank
 * line.  That is the flash.
 */
typedef enum {
    ALLSTAR_BANNER_BLANK = 0,  /* $169C, the off half of the flash */
    ALLSTAR_BANNER_TIE,        /* $16BB                            */
    ALLSTAR_BANNER_NAME_WINS   /* $16C4, "<name> WINS"             */
} AllStarBannerKind;

typedef struct {
    AllStarBannerKind kind;
    uint16_t source;      /* what HL is left pointing at for $06C0 */
    uint8_t d;            /* $06C0's row/column pair               */
    uint8_t e;
    uint16_t name_source; /* $16C4/$16CA, whose name is copied     */
    bool trims_spaces;    /* $16D1 calls $1769 in H-O-R-S-E only   */
    bool copies_name;     /* $16D4 calls $170D                     */
} AllStarBanner;

/*
 * $1699.  `flash` is the caller's `$FF8B & $08`; `mode` is $FF8F; `winner` is
 * the value $28E1 returns outside H-O-R-S-E, or $C17D inside it.  Zero means
 * nobody has won yet and the line reads as a tie.
 */
void allstar_postgame_banner_1699(uint8_t flash, uint8_t mode, uint8_t winner,
                                  AllStarBanner *out);

#endif /* ALLSTAR_POSTGAME_H */
