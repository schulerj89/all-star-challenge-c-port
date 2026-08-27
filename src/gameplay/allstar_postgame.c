#include "allstar_postgame.h"
#include "allstar_tournament.h"

/* $10D9: indexed by $FF8D. */
static const uint16_t POSTGAME_SCREEN_TABLE[4] = { 0x10E1u, 0x1343u, 0x139Bu, 0x146Fu };

/* $10E4: indexed by the game mode in $FF8F. */
static const uint16_t POSTGAME_MODE_TABLE[5] = { 0x10EEu, 0x1121u, 0x11D5u, 0x1209u, 0x12A6u };

/* $147B: indexed by $C181. */
static const uint16_t POSTGAME_147B_TABLE[4] = { 0x1483u, 0x14B6u, 0x1493u, 0x14BDu };

#define POSTGAME_SCREEN_TABLE_LENGTH  4
#define POSTGAME_MODE_TABLE_LENGTH    5
#define POSTGAME_147B_TABLE_LENGTH    4

/* $1657 writes these three 14-byte rows, one per screen row. */
static const uint16_t POSTGAME_PANEL_SOURCES[ALLSTAR_POSTGAME_PANEL_ROWS] = { 0x166Fu, 0x167Du, 0x168Bu };

const uint16_t* allstar_postgame_screen_table(int *count) {
    if (count) *count = POSTGAME_SCREEN_TABLE_LENGTH;
    return POSTGAME_SCREEN_TABLE;
}

const uint16_t* allstar_postgame_mode_table(int *count) {
    if (count) *count = POSTGAME_MODE_TABLE_LENGTH;
    return POSTGAME_MODE_TABLE;
}

/*
 * $10A5/$10AD/$10B1/$10B5 -> $10B7.  The stub picks the screen index, the tail
 * pages in bank 1 and loads $640F into $8C00 unless the mode is $01, then
 * $10D6 dispatches through $10D9.  Route 0 is $10E1, which reads $FF8F and
 * dispatches again through $10E4.
 */
void allstar_postgame_enter(AllStarPostgameEntry entry, uint8_t mode, AllStarPostgameEnter *out) {
    if (!out) return;

    out->screen = (uint8_t)entry;                                   /* $10B7 */
    out->sets_result_flag = (entry == ALLSTAR_POSTGAME_ENTRY_RESULT); /* $10A7 */
    out->bank = 1u;                                                 /* $10B9-$10BB */
    out->loads_tiles = (mode != 0x01u);                             /* $10BC-$10C0 */

    if ((int)entry >= POSTGAME_SCREEN_TABLE_LENGTH) {
        out->route = ALLSTAR_POSTGAME_ROUTE_BY_MODE;
        out->handler = 0;
        return;
    }

    out->route = (AllStarPostgameRoute)entry;
    if (out->route != ALLSTAR_POSTGAME_ROUTE_BY_MODE) {
        out->handler = POSTGAME_SCREEN_TABLE[(int)entry];           /* $10D8 */
        return;
    }

    /* $10E1: second dispatch, this time on the game mode. */
    if ((int)mode >= POSTGAME_MODE_TABLE_LENGTH) {
        out->handler = 0;
        return;
    }
    out->handler = POSTGAME_MODE_TABLE[(int)mode];                  /* $10E3 */
}

/* $10EE: a decided match tail-jumps into $10FA, a tie falls through to $12C9. */
AllStarPostgameResult allstar_postgame_result_route(uint8_t winner) {
    return (winner != 0) ? ALLSTAR_POSTGAME_RESULT_DRAW_ONLY
                         : ALLSTAR_POSTGAME_RESULT_DRAW_THEN_12C9;
}

/* $10FA */
int allstar_postgame_final_score_layout(AllStarPostgameDraw *out, int max) {
    static const AllStarPostgameDraw OPS[ALLSTAR_POSTGAME_LAYOUT_OPS] = {
        { ALLSTAR_POSTGAME_DRAW_PANEL,   0x03u, 0x05u },   /* $10FA */
        { ALLSTAR_POSTGAME_DRAW_NAME_1,  0x04u, 0x06u },   /* $1100 */
        { ALLSTAR_POSTGAME_DRAW_SCORE_1, 0x0Du, 0x06u },   /* $1106 */
        { ALLSTAR_POSTGAME_DRAW_PANEL,   0x03u, 0x0Bu },   /* $110C */
        { ALLSTAR_POSTGAME_DRAW_NAME_2,  0x04u, 0x0Cu },   /* $1112 */
        { ALLSTAR_POSTGAME_DRAW_SCORE_2, 0x0Du, 0x0Cu }    /* $1118 */
    };
    int i;
    int count = ALLSTAR_POSTGAME_LAYOUT_OPS;
    if (!out || max <= 0) return 0;
    if (count > max) count = max;
    for (i = 0; i < count; i++) out[i] = OPS[i];
    return count;
}

/* $1657: push DE, write a row, pop and inc e, three times. */
void allstar_postgame_panel_rows(uint8_t d, uint8_t e, uint16_t *sources, uint8_t *rows) {
    int i;
    for (i = 0; i < ALLSTAR_POSTGAME_PANEL_ROWS; i++) {
        if (sources) sources[i] = POSTGAME_PANEL_SOURCES[i];
        if (rows) rows[i] = (uint8_t)(e + i);
    }
    (void)d;
}

/*
 * The name and score writers all converge on the same tail.  $1751 and $1756
 * load a name buffer and jump straight to $1766, which is `jp $06C0`.  $175B
 * and $1760 load the same buffers but route through $1763, which calls $1769
 * to skip leading spaces first.  $1770 and $1775 load a score pointer and fall
 * into $1778, which dereferences it before $177B converts and writes it.
 */
bool allstar_postgame_draw_detail(AllStarPostgameDrawKind kind, AllStarPostgameDrawDetail *out) {
    if (!out) return false;
    switch (kind) {
    case ALLSTAR_POSTGAME_DRAW_NAME_1_RAW:                          /* $1751 -> $1766 */
        out->routine = 0x1751u; out->source = ALLSTAR_POSTGAME_NAME_1;
        out->skip_spaces = false; out->is_score = false;
        return true;
    case ALLSTAR_POSTGAME_DRAW_NAME_2_RAW:                          /* $1756 -> $1766 */
        out->routine = 0x1756u; out->source = ALLSTAR_POSTGAME_NAME_2;
        out->skip_spaces = false; out->is_score = false;
        return true;
    case ALLSTAR_POSTGAME_DRAW_NAME_1:                              /* $175B -> $1763 */
        out->routine = 0x175Bu; out->source = ALLSTAR_POSTGAME_NAME_1;
        out->skip_spaces = true; out->is_score = false;
        return true;
    case ALLSTAR_POSTGAME_DRAW_NAME_2:                              /* $1760 -> $1763 */
        out->routine = 0x1760u; out->source = ALLSTAR_POSTGAME_NAME_2;
        out->skip_spaces = true; out->is_score = false;
        return true;
    case ALLSTAR_POSTGAME_DRAW_SCORE_1:                             /* $1770 -> $1778 */
        out->routine = 0x1770u; out->source = ALLSTAR_POSTGAME_SCORE_1;
        out->skip_spaces = false; out->is_score = true;
        return true;
    case ALLSTAR_POSTGAME_DRAW_SCORE_2:                             /* $1775 -> $1778 */
        out->routine = 0x1775u; out->source = ALLSTAR_POSTGAME_SCORE_2;
        out->skip_spaces = false; out->is_score = true;
        return true;
    case ALLSTAR_POSTGAME_DRAW_ATTEMPTS:                            /* $11B4 -> $177B */
        out->routine = 0x177Bu; out->source = ALLSTAR_POSTGAME_ATTEMPTS;
        out->skip_spaces = false; out->is_score = true;
        return true;
    case ALLSTAR_POSTGAME_DRAW_WORD_C137:                           /* $123A -> $1778 */
        out->routine = 0x1778u; out->source = ALLSTAR_POSTGAME_WORD_C137;
        out->skip_spaces = false; out->is_score = true;
        return true;
    case ALLSTAR_POSTGAME_DRAW_WORD_C139:                           /* $1243 -> $1778 */
        out->routine = 0x1778u; out->source = ALLSTAR_POSTGAME_WORD_C139;
        out->skip_spaces = false; out->is_score = true;
        return true;
    case ALLSTAR_POSTGAME_DRAW_TOTAL_HI:                            /* $1229 -> $053C */
    case ALLSTAR_POSTGAME_DRAW_TOTAL_LO:                            /* $1234 -> $053C */
        out->routine = 0x053Cu; out->source = ALLSTAR_POSTGAME_DIGITS;
        out->skip_spaces = false; out->is_score = false;
        return true;
    case ALLSTAR_POSTGAME_DRAW_TEXT_GAME:                           /* $1354 -> $06C0 */
        out->routine = 0x06C0u; out->source = ALLSTAR_POSTGAME_TEXT_GAME;
        out->skip_spaces = false; out->is_score = false;
        return true;
    case ALLSTAR_POSTGAME_DRAW_TEXT_VS:                             /* $1374 -> $06C0 */
        out->routine = 0x06C0u; out->source = ALLSTAR_POSTGAME_TEXT_VS;
        out->skip_spaces = false; out->is_score = false;
        return true;
    case ALLSTAR_POSTGAME_DRAW_MATCH_DIGIT:                         /* $135D -> $053C */
        out->routine = 0x053Cu; out->source = ALLSTAR_POSTGAME_NAME_SCRATCH;
        out->skip_spaces = false; out->is_score = false;
        return true;
    default:
        return false;                                               /* $1657 panel */
    }
}

/* $1769: walk forward while the byte is a space. */
uint8_t allstar_postgame_skip_spaces(const uint8_t *text, uint8_t length) {
    uint8_t i = 0;
    if (!text) return 0;
    while (i < length && text[i] == 0x20u) i++;
    return i;
}

/*
 * $1726.  H's low nibble is the hundreds digit, L holds tens and units as BCD.
 * A leading zero is blanked, but once a nonzero digit has been emitted the
 * zero tens digit prints as '0'.  The units digit always prints.
 */
void allstar_postgame_score_digits(uint16_t bcd_score, uint8_t *digits) {
    uint8_t high = (uint8_t)(bcd_score >> 8);
    uint8_t low = (uint8_t)(bcd_score & 0xFFu);
    uint8_t hundreds = (uint8_t)(high & 0x0Fu);
    uint8_t tens = (uint8_t)((low & 0xF0u) >> 4);
    uint8_t units = (uint8_t)(low & 0x0Fu);
    uint8_t printed = 0;

    if (!digits) return;

    if (hundreds != 0) {                                            /* $172C-$1733 */
        digits[0] = (uint8_t)(hundreds + ALLSTAR_POSTGAME_DIGIT_BASE);
        printed = 1;
    } else {
        digits[0] = ALLSTAR_POSTGAME_BLANK_TILE;
    }

    if (tens != 0 || printed) {                                     /* $1736-$1745 */
        digits[1] = (uint8_t)(tens + ALLSTAR_POSTGAME_DIGIT_BASE);
    } else {
        digits[1] = ALLSTAR_POSTGAME_BLANK_TILE;
    }

    digits[2] = (uint8_t)(units + ALLSTAR_POSTGAME_DIGIT_BASE);     /* $1749-$174E */
}

/* $177B: convert through $1726, then write three tiles from $C1FB via $053C. */
int allstar_postgame_score_tiles(uint16_t bcd_score, uint8_t *tiles) {
    allstar_postgame_score_digits(bcd_score, tiles);
    return ALLSTAR_POSTGAME_SCORE_TILES;
}

/* $146F */
uint16_t allstar_postgame_route_146f(uint8_t c181, uint8_t *bank_out) {
    if (bank_out) *bank_out = 2u;                                   /* $146F-$1471 */
    if ((int)c181 >= POSTGAME_147B_TABLE_LENGTH) return 0;
    return POSTGAME_147B_TABLE[c181];                               /* $147A */
}

/*
 * $1638.  Start exits the screen only while $FFEC is clear; otherwise the frame
 * counter in BC has to run out.  The ROM tests input before decrementing.
 */
AllStarPostgameHold allstar_postgame_hold_step(uint16_t *frames, uint8_t hold_lock, uint8_t new_buttons) {
    if (!frames) return ALLSTAR_POSTGAME_HOLD_TIMEOUT;
    if (hold_lock == 0 && (new_buttons & ALLSTAR_POSTGAME_START_MASK) != 0) {
        return ALLSTAR_POSTGAME_HOLD_INPUT;                         /* $1645-$1649 */
    }
    *frames = (uint16_t)(*frames - 1u);                             /* $164B */
    return (*frames == 0) ? ALLSTAR_POSTGAME_HOLD_TIMEOUT : ALLSTAR_POSTGAME_HOLD_WAITING;
}

/*
 * $1121..$1194.  In a link game each side flags itself ready by writing $F0
 * into its own score high byte, then spins until the other side's byte reads
 * $F0.  $C199 == $03 means this cartridge is player 2, so the roles of $C134
 * and $C136 swap.  $C199 == 0 is a solo game and skips the handshake entirely.
 */
void allstar_postgame_free_throw_entry(uint8_t role, uint8_t score_1_high, uint8_t score_2_high,
                                       AllStarPostgameFreeThrow *out) {
    bool player_2;
    uint8_t remote_high;

    if (!out) return;

    player_2 = (role == ALLSTAR_POSTGAME_ROLE_PLAYER_2);
    out->is_player_2 = player_2;
    out->ready_flag = 0;
    out->poll_address = 0;
    out->announce_sound = 0;

    if (role == 0) {                                                /* $112B-$112C */
        out->path = ALLSTAR_POSTGAME_FT_DRAW;
        return;
    }

    /* $1132 / $1142: look at the other side's high byte. */
    remote_high = player_2 ? score_1_high : score_2_high;
    out->ready_flag = player_2 ? ALLSTAR_POSTGAME_SCORE_2_HIGH : ALLSTAR_POSTGAME_SCORE_1_HIGH;

    if (remote_high == ALLSTAR_POSTGAME_READY_FLAG) {                /* $1152 */
        out->path = ALLSTAR_POSTGAME_FT_SYNC;
        return;
    }

    out->path = ALLSTAR_POSTGAME_FT_WAIT;                            /* $1168 */
    out->poll_address = player_2 ? ALLSTAR_POSTGAME_SCORE_1_HIGH : ALLSTAR_POSTGAME_SCORE_2_HIGH;
    out->announce_sound = player_2 ? ALLSTAR_POSTGAME_SOUND_READY_2
                                   : ALLSTAR_POSTGAME_SOUND_READY_1;
}

/* $11A2..$11D2 */
int allstar_postgame_free_throw_layout(bool is_player_2, AllStarPostgameDraw *out, int max) {
    int count = ALLSTAR_POSTGAME_FT_LAYOUT_OPS;
    if (!out || max <= 0) return 0;
    if (count > max) count = max;

    if (count > 0) {                                                /* $11A2-$11B1 */
        out[0].kind = is_player_2 ? ALLSTAR_POSTGAME_DRAW_NAME_2_RAW
                                  : ALLSTAR_POSTGAME_DRAW_NAME_1_RAW;
        out[0].d = 0x05u;
        out[0].e = 0x04u;
    }
    if (count > 1) {                                                /* $11B4-$11BC */
        out[1].kind = ALLSTAR_POSTGAME_DRAW_ATTEMPTS;
        out[1].d = 0x11u;
        out[1].e = 0x07u;
    }
    if (count > 2) {                                                /* $11BF-$11D2 */
        out[2].kind = is_player_2 ? ALLSTAR_POSTGAME_DRAW_SCORE_2
                                  : ALLSTAR_POSTGAME_DRAW_SCORE_1;
        out[2].d = 0x11u;
        out[2].e = 0x0Au;
    }
    return count;
}

/*
 * $11D5.  $FFAB picks whose name the message calls out, and B ends up holding
 * the *other* player, which $C17D records as the survivor.
 */
void allstar_postgame_horse(uint8_t eliminated_flag, AllStarPostgameHorse *out) {
    if (!out) return;
    if (eliminated_flag == 0) {                                     /* $11D5-$11DD */
        out->loser_name = ALLSTAR_POSTGAME_NAME_1;
        out->winner = 2u;
    } else {                                                        /* $11DF-$11E2 */
        out->loser_name = ALLSTAR_POSTGAME_NAME_2;
        out->winner = 1u;
    }
    out->message = ALLSTAR_POSTGAME_HORSE_MESSAGE;                  /* $11EE */
    out->message_length = ALLSTAR_POSTGAME_HORSE_MESSAGE_LEN;
    out->d = 0x02u;                                                 /* $11FA */
    out->e = 0x07u;
}

/*
 * $170D.  Copy forward until a byte with bit 7 set, then walk back clearing
 * that bit and discarding trailing spaces, and finish with a single space.
 */
uint8_t allstar_postgame_copy_name(const uint8_t *src, uint8_t src_length,
                                   uint8_t *out, uint8_t out_length) {
    uint8_t written = 0;
    uint8_t i = 0;

    if (!src || !out || out_length == 0) return 0;

    /* $1710-$1715: the terminator itself is copied. */
    while (i < src_length && written < out_length) {
        uint8_t byte = src[i++];
        out[written++] = byte;
        if ((byte & 0x80u) != 0) break;
    }
    if (written == 0) return 0;

    /* $1717-$171E: strip bit 7 going backwards, then drop trailing spaces. */
    while (written > 0) {
        uint8_t byte = (uint8_t)(out[written - 1u] & 0x7Fu);
        out[written - 1u] = byte;
        if (byte != 0x20u) break;
        written--;
    }

    /* $1720-$1724: exactly one space follows the last real character. */
    if (written < out_length) out[written++] = 0x20u;
    return written;
}

/* $1786 */
void allstar_postgame_clear_shape(AllStarPostgameClear *out) {
    if (!out) return;
    out->base = 0x9800u;                                            /* $1787 */
    out->outer = 0x06u;                                             /* $178A */
    out->inner = 0x06u;                                             /* $178C */
    out->per_row = 16u;                                             /* $1793-$17A2 */
    out->fill = 0x00u;                                              /* $1792 */
}

/*
 * $1209..$12A5.  The Accuracy screen draws the local name, a four-tile total
 * that bank 1's $780A renders from the $FF94 word, two more words, and the
 * local score.  A two-player game then runs the same $F0 handshake as $1121,
 * except that it always flags and always polls -- there is no $1152 shortcut.
 */
void allstar_postgame_accuracy_entry(uint8_t role, uint8_t player_count,
                                     AllStarPostgameAccuracy *out) {
    bool player_2;

    if (!out) return;

    player_2 = (role == ALLSTAR_POSTGAME_ROLE_PLAYER_2);
    out->is_player_2 = player_2;
    out->two_player = (player_count != 0x01u);                      /* $125E-$1261 */

    if (!out->two_player) {
        out->ready_flag = 0;
        out->poll_address = 0;
        out->status_byte = 0;
        out->music = 0;
        return;
    }

    out->ready_flag = player_2 ? ALLSTAR_POSTGAME_SCORE_2_HIGH       /* $1264-$1277 */
                               : ALLSTAR_POSTGAME_SCORE_1_HIGH;
    out->poll_address = player_2 ? ALLSTAR_POSTGAME_SCORE_1_HIGH
                                 : ALLSTAR_POSTGAME_SCORE_2_HIGH;
    /* $1277 stores A, which still holds the $F0 it just wrote to the flag. */
    out->status_byte = ALLSTAR_POSTGAME_READY_FLAG;
    out->music = ALLSTAR_POSTGAME_MUSIC_ACCURACY;                    /* $127A */
}

/* $120E..$125B */
int allstar_postgame_accuracy_layout(bool is_player_2, AllStarPostgameDraw *out, int max) {
    int count = ALLSTAR_POSTGAME_ACC_LAYOUT_OPS;
    if (!out || max <= 0) return 0;
    if (count > max) count = max;

    if (count > 0) {                                                /* $120E-$121D */
        out[0].kind = is_player_2 ? ALLSTAR_POSTGAME_DRAW_NAME_2_RAW
                                  : ALLSTAR_POSTGAME_DRAW_NAME_1_RAW;
        out[0].d = 0x05u; out[0].e = 0x03u;
    }
    if (count > 1) {                                                /* $1229-$1231 */
        out[1].kind = ALLSTAR_POSTGAME_DRAW_TOTAL_HI;
        out[1].d = 0x0Fu; out[1].e = 0x06u;
    }
    if (count > 2) {                                                /* $1234, inc d */
        out[2].kind = ALLSTAR_POSTGAME_DRAW_TOTAL_LO;
        out[2].d = 0x10u; out[2].e = 0x06u;
    }
    if (count > 3) {                                                /* $123A */
        out[3].kind = ALLSTAR_POSTGAME_DRAW_WORD_C137;
        out[3].d = 0x11u; out[3].e = 0x0Au;
    }
    if (count > 4) {                                                /* $1243 */
        out[4].kind = ALLSTAR_POSTGAME_DRAW_WORD_C139;
        out[4].d = 0x11u; out[4].e = 0x08u;
    }
    if (count > 5) {                                                /* $124C-$125B */
        out[5].kind = is_player_2 ? ALLSTAR_POSTGAME_DRAW_SCORE_2
                                  : ALLSTAR_POSTGAME_DRAW_SCORE_1;
        out[5].d = 0x11u; out[5].e = 0x0Cu;
    }
    return count;
}

/*
 * $12A6..$1342.  A decided match that is not the final simply returns to the
 * $0F2E driver.  A tie -- in any round -- does not advance the bracket: the
 * screen holds for $00F0 frames and then calls $0B9A to replay the whole match,
 * re-entering the dispatch at $10A5 afterwards.  That is why $284D and $286E
 * bail out on a tied $28E1 verdict.  Only match $07 can reach the champion
 * presentation, and on that path $10FA is not drawn at all.
 */
void allstar_postgame_tournament(uint8_t match_count, uint8_t winner,
                                 AllStarPostgameTournament *out) {
    if (!out) return;

    out->is_final = (match_count == ALLSTAR_POSTGAME_FINAL_MATCH);   /* $12A9 */
    out->writes_tie_flag = out->is_final;                            /* $12B8 */
    out->tie_flag = 0;
    out->sound = 0;
    out->hold_frames = 0;
    out->champion_source = 0;
    out->music = 0;

    if (!out->is_final) {
        out->draws_score_panel = true;                               /* $12AD */
        if (winner != 0) {                                           /* $12B3-$12B4 */
            out->path = ALLSTAR_POSTGAME_TR_RETURN;
            return;
        }
        out->path = ALLSTAR_POSTGAME_TR_REPLAY;                      /* $12B5 */
        out->sound = ALLSTAR_POSTGAME_SOUND_REPLAY;
        out->hold_frames = ALLSTAR_POSTGAME_REPLAY_FRAMES;
        return;
    }

    if (winner != 0) {                                               /* $12BF */
        out->path = ALLSTAR_POSTGAME_TR_CHAMPION;
        out->draws_score_panel = false;
        out->sound = ALLSTAR_POSTGAME_SOUND_CHAMPION;                /* $12FF */
        out->champion_source = (winner == 1u) ? ALLSTAR_POSTGAME_ENTRANT_1
                                              : ALLSTAR_POSTGAME_ENTRANT_2;
        out->music = ALLSTAR_POSTGAME_MUSIC_CHAMPION;                /* $133B */
        return;
    }

    out->path = ALLSTAR_POSTGAME_TR_REPLAY;                          /* $12C1-$12C6 */
    out->tie_flag = 1u;
    out->draws_score_panel = true;
    out->sound = ALLSTAR_POSTGAME_SOUND_REPLAY;
    out->hold_frames = ALLSTAR_POSTGAME_REPLAY_FRAMES;
}

/* $1323..$1334 */
uint8_t allstar_postgame_record_surname(const uint8_t *record, uint8_t length) {
    uint8_t i = 1u;

    if (!record || length == 0) return 0;

    while (i < length && record[i] == 0x20u) i++;                   /* $1324-$132A */
    if (i < length) i++;                                            /* $132C */
    if (i < length && record[i] == 0x2Eu) {                         /* $132D-$1333 */
        i = (uint8_t)(i + 2u);
    }
    if (i > 0) i--;                                                 /* $1334 */
    if (i >= length) i = (uint8_t)(length - 1u);
    return i;
}

/* $135D-$1365: $C0BE plus the digit base, stored as a single tile in $C1D3. */
uint8_t allstar_postgame_match_digit(uint8_t match_count) {
    return (uint8_t)(match_count + ALLSTAR_POSTGAME_DIGIT_BASE);
}

/*
 * $1343..$1393.  The GAME line is drawn only when $FF8F is the tournament;
 * every mode gets the two names and the VS between them.
 */
int allstar_postgame_matchup_layout(uint8_t mode, AllStarPostgameDraw *out, int max) {
    int count = 0;
    if (!out || max <= 0) return 0;

    if (mode == ALLSTAR_POSTGAME_MODE_TOURNAMENT) {                 /* $134E-$1352 */
        if (count < max) {                                          /* $1354 */
            out[count].kind = ALLSTAR_POSTGAME_DRAW_TEXT_GAME;
            out[count].d = 0x06u; out[count].e = 0x0Du; count++;
        }
        if (count < max) {                                          /* $1366 */
            out[count].kind = ALLSTAR_POSTGAME_DRAW_MATCH_DIGIT;
            out[count].d = 0x0Cu; out[count].e = 0x0Du; count++;
        }
    }
    if (count < max) {                                              /* $136E-$1371 */
        out[count].kind = ALLSTAR_POSTGAME_DRAW_NAME_1_RAW;
        out[count].d = 0x05u; out[count].e = 0x05u; count++;
    }
    if (count < max) {                                              /* $1374-$137A */
        out[count].kind = ALLSTAR_POSTGAME_DRAW_TEXT_VS;
        out[count].d = 0x08u; out[count].e = 0x07u; count++;
    }
    if (count < max) {                                              /* $137D-$1380 */
        out[count].kind = ALLSTAR_POSTGAME_DRAW_NAME_2_RAW;
        out[count].d = 0x05u; out[count].e = 0x09u; count++;
    }
    return count;
}

/*
 * $139B..$1463.  $C17F selects the stage: $01 returns without drawing, $04
 * lists all eight round 1 entrants, anything else lists the four
 * semifinalists.  Both lists interleave the left and right sides so a match
 * pair lands on adjacent rows, and every slot goes through $1464 to load the
 * player record before $1766 draws it.
 */
void allstar_postgame_bracket(uint8_t stage, AllStarPostgameBracket *out) {
    /* $146B: indexed by $C17F, so slot 0 is never reached. */
    static const uint8_t BRACKET_SOUND[4] = { 0x00u, 0x0Eu, 0x00u, 0x0Du };
    static const uint16_t FULL_SLOTS[8] = {
        ALLSTAR_TR_R1_LEFT,          ALLSTAR_TR_R1_RIGHT,
        ALLSTAR_TR_R1_LEFT  + 1u,    ALLSTAR_TR_R1_RIGHT + 1u,
        ALLSTAR_TR_R1_LEFT  + 2u,    ALLSTAR_TR_R1_RIGHT + 2u,
        ALLSTAR_TR_R1_LEFT  + 3u,    ALLSTAR_TR_R1_RIGHT + 3u
    };
    static const uint16_t SEMI_SLOTS[4] = {
        ALLSTAR_TR_R2_LEFT,          ALLSTAR_TR_R2_RIGHT,
        ALLSTAR_TR_R2_LEFT  + 1u,    ALLSTAR_TR_R2_RIGHT + 1u
    };
    const uint16_t *slots;
    uint8_t first_row;
    uint8_t i;

    if (!out) return;

    out->draws = false;
    out->sound = 0;
    out->count = 0;
    out->hold_frames = 0;
    for (i = 0; i < ALLSTAR_POSTGAME_BRACKET_MAX_SLOTS; i++) {
        out->entries[i].slot = 0;
        out->entries[i].d = 0;
        out->entries[i].e = 0;
    }

    if (stage == 0x01u) return;                                     /* $13A9-$13AA */
    if (stage == 0 || stage > 0x04u) return;

    out->draws = true;
    out->sound = BRACKET_SOUND[stage - 1u];                         /* $13AB-$13B2 */
    out->hold_frames = ALLSTAR_POSTGAME_BRACKET_HOLD_FRAMES;        /* $1459 */

    if (stage == ALLSTAR_POSTGAME_BRACKET_FULL) {                   /* $13C0-$13C2 */
        slots = FULL_SLOTS;
        out->count = 8u;
        first_row = 0x01u;                                          /* $13CA */
    } else {
        slots = SEMI_SLOTS;
        out->count = 4u;
        first_row = 0x05u;                                          /* $142C */
    }

    for (i = 0; i < out->count; i++) {
        out->entries[i].slot = slots[i];
        out->entries[i].d = 0x01u;
        out->entries[i].e = (uint8_t)(first_row + (uint8_t)(i * 2u));
    }
}

/*
 * $1483/$14B6/$1493/$14BD.  Each entry point picks a bracket list end in HL and
 * a flag in B, then falls into the shared body.  B decides how many names get
 * drawn: zero lists four, walking HL backwards into rows 10, 8, 6 and 4; one
 * lists just the pair in rows 6 and 4.  $C184 selects which side the two
 * "by picker" entries look at, and the sound differs only for one player.
 */
void allstar_postgame_chooser(AllStarPostgameChooserEntry entry, uint8_t player_count,
                              uint8_t picker, AllStarPostgameChooser *out) {
    bool picker_is_first;
    bool one_player;
    uint8_t i;

    if (!out) return;

    picker_is_first = (picker == 0x01u);
    one_player = (player_count == 0x01u);

    switch (entry) {
    case ALLSTAR_POSTGAME_CHOOSER_R1_BY_PICKER:                     /* $1483 */
        out->list_end = picker_is_first ? 0xC0C2u : 0xC0C6u;
        out->pair_only = false;
        out->sound = one_player ? 0x0Fu : (picker_is_first ? 0x11u : 0x12u);
        break;
    case ALLSTAR_POSTGAME_CHOOSER_R1_RIGHT:                         /* $14B6 */
        out->list_end = 0xC0C6u;
        out->pair_only = false;
        out->sound = one_player ? 0x10u : (picker_is_first ? 0x11u : 0x12u);
        break;
    case ALLSTAR_POSTGAME_CHOOSER_R2_BY_PICKER:                     /* $1493 */
        out->list_end = picker_is_first ? 0xC0CCu : 0xC0CEu;
        out->pair_only = true;
        out->sound = one_player ? 0x0Fu : (picker_is_first ? 0x11u : 0x12u);
        break;
    default:                                                        /* $14BD */
        out->list_end = 0xC0CEu;
        out->pair_only = true;
        out->sound = one_player ? 0x10u : (picker_is_first ? 0x11u : 0x12u);
        break;
    }

    out->count = out->pair_only ? 2u : 4u;                          /* $14DC-$14DD */
    for (i = 0; i < out->count; i++) {
        /* $14DF..$150A: rows count down by two as HL counts down by one. */
        out->names[i].slot = (uint16_t)(out->list_end - i);
        out->names[i].d = 0x03u;
        out->names[i].e = (uint8_t)((out->count * 2u) + 2u - (uint8_t)(i * 2u));
    }
    for (i = out->count; i < ALLSTAR_POSTGAME_CHOOSER_MAX_NAMES; i++) {
        out->names[i].slot = 0;
        out->names[i].d = 0;
        out->names[i].e = 0;
    }
}

/* $1521..$1531 */
uint8_t allstar_postgame_chooser_buttons(uint8_t player_count, uint8_t picker,
                                         uint8_t player_1_new, uint8_t player_2_new) {
    if (player_count == 0x01u) return player_1_new;                 /* $1524-$1527 */
    if (picker == 0x01u) return player_1_new;                       /* $1529-$152D */
    return player_2_new;                                            /* $152F */
}

/* $151B..$1553 */
AllStarPostgameChooserInput allstar_postgame_chooser_step(uint8_t hold_lock, uint8_t buttons,
                                                          uint8_t *selection) {
    uint8_t accepted;

    if (!selection) return ALLSTAR_POSTGAME_CHOOSER_IDLE;
    if (hold_lock != 0) return ALLSTAR_POSTGAME_CHOOSER_IDLE;       /* $151C-$151F */

    accepted = (uint8_t)(buttons & ALLSTAR_POSTGAME_CHOOSER_MASK);  /* $1533 */
    if (accepted == 0) return ALLSTAR_POSTGAME_CHOOSER_IDLE;

    if ((accepted & ALLSTAR_POSTGAME_CHOOSER_CONFIRM) != 0) {       /* $1537 */
        return ALLSTAR_POSTGAME_CHOOSER_CONFIRMED;
    }

    *selection = (uint8_t)(*selection ^ 0x01u);                     /* $153B-$1540 */
    return ALLSTAR_POSTGAME_CHOOSER_TOGGLED;
}

/*
 * $1554..$15AA.  Two three-row boxes at column $0B.  The selected one sits at
 * row $0C and the other at row $0F, and each box's middle row swaps art with
 * the selection, so both the position and the highlight move together.
 */
int allstar_postgame_chooser_layout(uint8_t selection, AllStarPostgameChooserCell *out, int max) {
    uint8_t first_row;
    uint8_t second_row;
    int count = 0;
    int i;

    if (!out || max <= 0) return 0;

    first_row = (selection == 0) ? 0x0Cu : 0x0Fu;                   /* $1554-$155A */
    second_row = (selection == 0) ? 0x0Fu : 0x0Cu;                  /* $1581-$1587 */

    {
        const uint16_t box_one[ALLSTAR_POSTGAME_CHOOSER_ROWS] = {
            0x15ABu,                                                /* $155E */
            (selection == 0) ? 0x15B5u : 0x15BFu,                   /* $1567-$1570 */
            0x15C9u                                                 /* $1578 */
        };
        const uint16_t box_two[ALLSTAR_POSTGAME_CHOOSER_ROWS] = {
            0x15B0u,                                                /* $158B */
            (selection == 0) ? 0x15C4u : 0x15BAu,                   /* $1594-$159D */
            0x15CEu                                                 /* $15A5 */
        };
        for (i = 0; i < ALLSTAR_POSTGAME_CHOOSER_ROWS && count < max; i++) {
            out[count].source = box_one[i];
            out[count].d = ALLSTAR_POSTGAME_CHOOSER_COLUMN;
            out[count].e = (uint8_t)(first_row + i);
            count++;
        }
        for (i = 0; i < ALLSTAR_POSTGAME_CHOOSER_ROWS && count < max; i++) {
            out[count].source = box_two[i];
            out[count].d = ALLSTAR_POSTGAME_CHOOSER_COLUMN;
            out[count].e = (uint8_t)(second_row + i);
            count++;
        }
    }
    return count;
}
