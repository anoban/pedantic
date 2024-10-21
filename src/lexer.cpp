#include <array>
#include <utility>

#include <prep.hpp>

// WHY DID NOT THE PLAN9 IMBECILES EVER THINK OF COMMENTING THEIR SOURCE CODE

/*
 *   Lexical FSM (Finite State Machine) encoding
 *   when in state state, and one of the characters
 *   in ch arrives, enter nextstate.
 *   States >= S_SELF are either final, or at least require special action.
 *   In 'fsmachine' there is a line for each state X charset X nextstate.
 *   List chars that overwrite previous entries later (e.g. ALPHABET
 *   can be overridden by '_' by a later entry; and character_classl::xx is the
 *   the universal set, and should always be first.
 *   States above S_SELF are represented in the big table as negative values.
 *   S_SELF and S_SELFB encode the resulting token type in the upper bits.
 *   These actions differ in that S_SELF doesn't have a lookahead char,
 *   S_SELFB does.
 *
 *   The encoding is blown out into a big table for time-efficiency.
 *   Entries have
 *      nextstate: 6 bits; ?\ marker: 1 bit; tokentype: 9 bits.
 */

#ifdef __cpp_lib_to_underlying // only available since C++23
using std::to_underlying;
#else
template<class _Ty>
[[nodiscard]] static inline constexpr typename std::underlying_type<_Ty>::type to_underlying(const _Ty& _enum) noexcept {
    return static_cast<typename std::underlying_type<_Ty>::type>(_enum);
}
#endif

static constexpr size_t FSM_MAX_STATES { 32 };

#define ACT(tok, act) ((tok << 7) + act)
#define QBSBIT        0100
#define GETACT(st)    (st >> 7) & 0x1ff

#define UTF2(c)       ((c) >= 0xA0 && (c) < 0xE0) /* 2-char UTF seq */
#define UTF3(c)       ((c) >= 0xE0 && (c) < 0xF0) /* 3-char UTF seq */

// character classes
enum CHARCLASS : char { WHITESPACE = 0x01, ALPHABET, NUMBER, EOFILE, MISC };

// valid states for the finite state machine
enum FSMSTATE : unsigned {
    START,
    NUM1,
    NUM2,
    NUM3,
    ID1,
    ST1,
    ST2,
    ST3,
    COM1,
    COM2,
    COM3,
    COM4,
    CC1,
    CC2,
    WS1,
    PLUS1,
    MINUS1,
    STAR1,
    SLASH1,
    PCT1,
    SHARP1,
    CIRC1,
    GT1,
    GT2,
    LT1,
    LT2,
    OR1,
    AND1,
    ASG1,
    NOT1,
    DOTS1,
    S_SELF = FSM_MAX_STATES,
    S_SELFB,
    S_EOF,
    S_NL,
    S_EOFSTR,
    S_STNL,
    S_COMNL,
    S_EOFCOM,
    S_COMMENT,
    S_EOB,
    S_WS,
    S_NAME
};

struct fsm {
        FSMSTATE state;     // if in this state
        char     ch[4];     // and see one of these characters
        FSMSTATE nextstate; // enter this state if positive
};

/*const*/
fsm fsmachine[] = {
    { FSMSTATE::START, { ::to_underlying(CHARCLASS::MISC) }, ACT(TKNTYPE::UNCLASS, FSMSTATE::S_SELF) },
    { FSMSTATE::START, { ' ', '\t', '\v', '\r' }, WS1 },
    { FSMSTATE::START, { CHARCLASS::NUMBER }, NUM1 },
    { FSMSTATE::START, { '.' }, NUM3 },
    { FSMSTATE::START, { CHARCLASS::ALPHABET }, ID1 },
    { FSMSTATE::START, { 'L' }, ST1 },
    { FSMSTATE::START, { '"' }, ST2 },
    { FSMSTATE::START, { '\'' }, CC1 },
    { FSMSTATE::START, { '/' }, COM1 },
    { FSMSTATE::START, { EOFC }, S_EOF },
    { FSMSTATE::START, { '\n' }, S_NL },
    { FSMSTATE::START, { '-' }, MINUS1 },
    { FSMSTATE::START, { '+' }, PLUS1 },
    { FSMSTATE::START, { '<' }, LT1 },
    { FSMSTATE::START, { '>' }, GT1 },
    { FSMSTATE::START, { '=' }, ASG1 },
    { FSMSTATE::START, { '!' }, NOT1 },
    { FSMSTATE::START, { '&' }, AND1 },
    { FSMSTATE::START, { '|' }, OR1 },
    { FSMSTATE::START, { '#' }, SHARP1 },
    { FSMSTATE::START, { '%' }, PCT1 },
    { FSMSTATE::START, { '[' }, ACT(SBRA, S_SELF) },
    { FSMSTATE::START, { ']' }, ACT(SKET, S_SELF) },
    { FSMSTATE::START, { '(' }, ACT(LP, S_SELF) },
    { FSMSTATE::START, { ')' }, ACT(RP, S_SELF) },
    { FSMSTATE::START, { '*' }, STAR1 },
    { FSMSTATE::START, { ',' }, ACT(COMMA, S_SELF) },
    { FSMSTATE::START, { '?' }, ACT(QUEST, S_SELF) },
    { FSMSTATE::START, { ':' }, ACT(COLON, S_SELF) },
    { FSMSTATE::START, { ';' }, ACT(SEMIC, S_SELF) },
    { FSMSTATE::START, { '{' }, ACT(CBRA, S_SELF) },
    { FSMSTATE::START, { '}' }, ACT(CKET, S_SELF) },
    { FSMSTATE::START, { '~' }, ACT(TILDE, S_SELF) },
    { FSMSTATE::START, { '^' }, CIRC1 },

    /* saw a digit */
    { NUM1, { C_XX }, ACT(NUMBER, S_SELFB) },
    { NUM1, { C_NUM, C_ALPH, '.' }, NUM1 },
    { NUM1, { 'E', 'e' }, NUM2 },
    { NUM1, { '_' }, ACT(NUMBER, S_SELFB) },

    /* saw possible start of exponent, digits-e */
    { NUM2, { C_XX }, ACT(NUMBER, S_SELFB) },
    { NUM2, { '+', '-' }, NUM1 },
    { NUM2, { C_NUM, C_ALPH }, NUM1 },
    { NUM2, { '_' }, ACT(NUMBER, S_SELFB) },

    /* saw a '.', which could be a number or an operator */
    { NUM3, { C_XX }, ACT(DOT, S_SELFB) },
    { NUM3, { '.' }, DOTS1 },
    { NUM3, { C_NUM }, NUM1 },

    { DOTS1, { C_XX }, ACT(UNCLASS, S_SELFB) },
    { DOTS1, { C_NUM }, NUM1 },
    { DOTS1, { '.' }, ACT(ELLIPS, S_SELF) },

    /* saw a letter or _ */
    { ID1, { C_XX }, ACT(NAME, S_NAME) },
    { ID1, { C_ALPH, C_NUM }, ID1 },

    /* saw L (start of wide string?) */
    { ST1, { C_XX }, ACT(NAME, S_NAME) },
    { ST1, { C_ALPH, C_NUM }, ID1 },
    { ST1, { '"' }, ST2 },
    { ST1, { '\'' }, CC1 },

    /* saw " beginning string */
    { ST2, { C_XX }, ST2 },
    { ST2, { '"' }, ACT(STRING, S_SELF) },
    { ST2, { '\\' }, ST3 },
    { ST2, { '\n' }, S_STNL },
    { ST2, { EOFC }, S_EOFSTR },

    /* saw \ in string */
    { ST3, { C_XX }, ST2 },
    { ST3, { '\n' }, S_STNL },
    { ST3, { EOFC }, S_EOFSTR },

    /* saw ' beginning character const */
    { CC1, { C_XX }, CC1 },
    { CC1, { '\'' }, ACT(CCON, S_SELF) },
    { CC1, { '\\' }, CC2 },
    { CC1, { '\n' }, S_STNL },
    { CC1, { EOFC }, S_EOFSTR },

    /* saw \ in ccon */
    { CC2, { C_XX }, CC1 },
    { CC2, { '\n' }, S_STNL },
    { CC2, { EOFC }, S_EOFSTR },

    /* saw /, perhaps start of comment */
    { COM1, { C_XX }, ACT(SLASH, S_SELFB) },
    { COM1, { '=' }, ACT(ASSLASH, S_SELF) },
    { COM1, { '*' }, COM2 },
    { COM1, { '/' }, COM4 },

    /* saw "/*", start of comment */
    { COM2, { C_XX }, COM2 },
    { COM2, { '\n' }, S_COMNL },
    { COM2, { '*' }, COM3 },
    { COM2, { EOFC }, S_EOFCOM },

    /* saw the * possibly ending a comment */
    { COM3, { C_XX }, COM2 },
    { COM3, { '\n' }, S_COMNL },
    { COM3, { '*' }, COM3 },
    { COM3, { '/' }, S_COMMENT },
    { COM3, { EOFC }, S_EOFCOM },

    /* // comment */
    { COM4, { C_XX }, COM4 },
    { COM4, { '\n' }, S_NL },
    { COM4, { EOFC }, S_EOFCOM },

    /* saw white space, eat it up */
    { WS1, { C_XX }, S_WS },
    { WS1, { ' ', '\t', '\v', '\r' }, WS1 },

    /* saw -, check --, -=, -> */
    { MINUS1, { C_XX }, ACT(MINUS, S_SELFB) },
    { MINUS1, { '-' }, ACT(MMINUS, S_SELF) },
    { MINUS1, { '=' }, ACT(ASMINUS, S_SELF) },
    { MINUS1, { '>' }, ACT(ARROW, S_SELF) },

    /* saw +, check ++, += */
    { PLUS1, { C_XX }, ACT(PLUS, S_SELFB) },
    { PLUS1, { '+' }, ACT(PPLUS, S_SELF) },
    { PLUS1, { '=' }, ACT(ASPLUS, S_SELF) },

    /* saw <, check <<, <<=, <= */
    { LT1, { C_XX }, ACT(LT, S_SELFB) },
    { LT1, { '<' }, LT2 },
    { LT1, { '=' }, ACT(LEQ, S_SELF) },
    { LT2, { C_XX }, ACT(LSH, S_SELFB) },
    { LT2, { '=' }, ACT(ASLSH, S_SELF) },

    /* saw >, check >>, >>=, >= */
    { GT1, { C_XX }, ACT(GT, S_SELFB) },
    { GT1, { '>' }, GT2 },
    { GT1, { '=' }, ACT(GEQ, S_SELF) },
    { GT2, { C_XX }, ACT(RSH, S_SELFB) },
    { GT2, { '=' }, ACT(ASRSH, S_SELF) },

    /* = */
    { ASG1, { C_XX }, ACT(ASGN, S_SELFB) },
    { ASG1, { '=' }, ACT(EQ, S_SELF) },

    /* ! */
    { NOT1, { C_XX }, ACT(NOT, S_SELFB) },
    { NOT1, { '=' }, ACT(NEQ, S_SELF) },

    /* & */
    { AND1, { C_XX }, ACT(AND, S_SELFB) },
    { AND1, { '&' }, ACT(LAND, S_SELF) },
    { AND1, { '=' }, ACT(ASAND, S_SELF) },

    /* | */
    { OR1, { C_XX }, ACT(OR, S_SELFB) },
    { OR1, { '|' }, ACT(LOR, S_SELF) },
    { OR1, { '=' }, ACT(ASOR, S_SELF) },

    /* # */
    { SHARP1, { C_XX }, ACT(SHARP, S_SELFB) },
    { SHARP1, { '#' }, ACT(DSHARP, S_SELF) },

    /* % */
    { PCT1, { C_XX }, ACT(PCT, S_SELFB) },
    { PCT1, { '=' }, ACT(ASPCT, S_SELF) },

    /* * */
    { STAR1, { C_XX }, ACT(STAR, S_SELFB) },
    { STAR1, { '=' }, ACT(ASSTAR, S_SELF) },

    /* ^ */
    { CIRC1, { C_XX }, ACT(CIRC, S_SELFB) },
    { CIRC1, { '=' }, ACT(ASCIRC, S_SELF) },
    -1
};

/* first index is char, second is state */
/* increase #states to power of 2 to encourage use of shift */
short bigfsm[256][FSM_MAX_STATES];

void expandlex(void) {
    /*const*/ struct fsm* fp;
    int                   i, j, nstate;

    for (fp = fsmachine; fp->state >= 0; fp++) {
        for (i = 0; fp->ch[i]; i++) {
            nstate = fp->nextstate;
            if (nstate >= S_SELF) nstate = ~nstate;

            switch (fp->ch[i]) {
                case CHARCLASS::MISC : /* random characters */
                    for (j = 0; j < 256; j++) bigfsm[j][fp->state] = nstate;
                    continue;
                case CHARCLASS::ALPHABET :
                    for (j = 0; j <= 256; j++)
                        if ('a' <= j && j <= 'z' || 'A' <= j && j <= 'Z' || UTF2(j) || UTF3(j) || j == '_') bigfsm[j][fp->state] = nstate;
                    continue;
                case CHARCLASS::NUMBER :
                    for (j = '0'; j <= '9'; j++) bigfsm[j][fp->state] = nstate;
                    continue;
                default : bigfsm[fp->ch[i]][fp->state] = nstate;
            }
        }
    }
    /* install special cases for ? (trigraphs),  \ (splicing), runes, and EOB */
    for (i = 0; i < FSM_MAX_STATES; i++) {
        for (j = 0; j < 0xFF; j++)
            if (j == '?' || j == '\\' || UTF2(j) || UTF3(j)) {
                if (bigfsm[j][i] > 0) bigfsm[j][i] = ~bigfsm[j][i];
                bigfsm[j][i] &= ~QBSBIT;
            }
        bigfsm[EOB][i] = ~S_EOB;
        if (bigfsm[EOFC][i] >= 0) bigfsm[EOFC][i] = ~S_EOF;
    }
}

void fixlex(void) {
    /* do C++ comments? */
    if (Cplusplus == 0) bigfsm['/'][COM1] = bigfsm['x'][COM1];
}

/*
 * fill in a row of tokens from input, terminated by NL or END
 * First token is put at trp->lp.
 * Reset is non-zero when the input buffer can be "rewound."
 * The value is a flag indicating that possible macros have
 * been seen in the row.
 */
int gettokens(token_row* trp, int reset) {
    register int            c, state, oldstate;
    register unsigned char* ip;
    register token *        tp, *maxp;
    int                     runelen;
    source*                 s    = cursource;
    int                     nmac = 0;
    extern char             outbuf[];

    tp = trp->lp;
    ip = s->inp;
    if (reset) {
        s->lineinc = 0;
        if (ip >= s->inl) { /* nothing in buffer */
            s->inl = s->inb;
            fillbuf(s);
            ip = s->inp = s->inb;
        } else if (ip >= s->inb + (3 * s->ins / 4)) {
            memmove(s->inb, ip, 4 + s->inl - ip);
            s->inl = s->inb + (s->inl - ip);
            ip = s->inp = s->inb;
        }
    }
    maxp    = &trp->bp[trp->max];
    runelen = 1;
    for (;;) {
continue2:
        if (tp >= maxp) {
            trp->lp = tp;
            tp      = growtokenrow(trp);
            maxp    = &trp->bp[trp->max];
        }
        tp->type    = UNCLASS;
        tp->hideset = 0;
        tp->t       = ip;
        tp->wslen   = 0;
        tp->flag    = 0;
        state       = START;
        for (;;) {
            oldstate = state;
            c        = *ip;
            if ((state = bigfsm[c][state]) >= 0) {
                ip      += runelen;
                runelen  = 1;
                continue;
            }
            state = ~state;
reswitch:
            switch (state & 0177) {
                case S_SELF : ip += runelen; runelen = 1;
                case S_SELFB :
                    tp->type = GETACT(state);
                    tp->len  = ip - tp->t;
                    tp++;
                    goto continue2;

                case S_NAME : /* like S_SELFB but with nmac check */
                    tp->type  = NAME;
                    tp->len   = ip - tp->t;
                    nmac     |= quicklook(tp->t[0], tp->len > 1 ? tp->t[1] : 0);
                    tp++;
                    goto continue2;

                case S_WS :
                    tp->wslen = ip - tp->t;
                    tp->t     = ip;
                    state     = START;
                    continue;

                default :
                    if ((state & QBSBIT) == 0) {
                        ip      += runelen;
                        runelen  = 1;
                        continue;
                    }
                    state  &= ~QBSBIT;
                    s->inp  = ip;
                    if (c == '?') { /* check trigraph */
                        if (trigraph(s)) {
                            state = oldstate;
                            continue;
                        }
                        goto reswitch;
                    }
                    if (c == '\\') { /* line-folding */
                        if (foldline(s)) {
                            s->lineinc++;
                            state = oldstate;
                            continue;
                        }
                        goto reswitch;
                    }
                    if (UTF2(c)) {
                        runelen = 2;
                        goto reswitch;
                    }
                    if (UTF3(c)) {
                        runelen = 3;
                        goto reswitch;
                    }
                    error(WARNING, "Lexical botch in cpp");
                    ip      += runelen;
                    runelen  = 1;
                    continue;

                case S_EOB :
                    s->inp = ip;
                    fillbuf(cursource);
                    state = oldstate;
                    continue;

                case S_EOF :
                    tp->type = END;
                    tp->len  = 0;
                    s->inp   = ip;
                    if (tp != trp->bp && (tp - 1)->type != NL && cursource->fd != -1) error(WARNING, "No newline at end of file");
                    trp->lp = tp + 1;
                    return nmac;

                case S_STNL : error(ERROR, "Unterminated string or char const");
                case S_NL :
                    tp->t     = ip;
                    tp->type  = NL;
                    tp->len   = 1;
                    tp->wslen = 0;
                    s->lineinc++;
                    s->inp  = ip + 1;
                    trp->lp = tp + 1;
                    return nmac;

                case S_EOFSTR : error(FATAL, "EOF in string or char constant"); break;

                case S_COMNL :
                    s->lineinc++;
                    state    = COM2;
                    ip      += runelen;
                    runelen  = 1;
                    if (ip >= s->inb + (7 * s->ins / 8)) { /* very long comment */
                        memmove(tp->t, ip, 4 + s->inl - ip);
                        s->inl -= ip - tp->t;
                        ip      = tp->t + 1;
                    }
                    continue;

                case S_EOFCOM : error(WARNING, "EOF inside comment"); --ip;
                case S_COMMENT :
                    ++ip;
                    tp->t     = ip;
                    tp->t[-1] = ' ';
                    tp->wslen = 1;
                    state     = START;
                    continue;
            }
            break;
        }
        ip      += runelen;
        runelen  = 1;
        tp->len  = ip - tp->t;
        tp++;
    }
}

/* have seen ?; handle the trigraph it starts (if any) else 0 */
int trigraph(source* s) noexcept {
    int c;

    while (s->inp + 2 >= s->inl && fillbuf(s) != EOF);
    if (s->inp[1] != '?') return 0;
    c = 0;
    switch (s->inp[2]) {
        case '='  : c = '#'; break;
        case '('  : c = '['; break;
        case '/'  : c = '\\'; break;
        case ')'  : c = ']'; break;
        case '\'' : c = '^'; break;
        case '<'  : c = '{'; break;
        case '!'  : c = '|'; break;
        case '>'  : c = '}'; break;
        case '-'  : c = '~'; break;
    }
    if (c) {
        *s->inp = c;
        memmove(s->inp + 1, s->inp + 3, s->inl - s->inp + 2);
        s->inl -= 2;
    }
    return c;
}

int foldline(source* s) noexcept {
    int ncr = 0;

recheck:
    while (s->inp + 1 >= s->inl && fillbuf(s) != EOF);
    if (s->inp[ncr + 1] == '\r') { /* nonstandardly, ignore CR before line-folding */
        ncr++;
        goto recheck;
    }
    if (s->inp[ncr + 1] == '\n') {
        memmove(s->inp, s->inp + 2 + ncr, s->inl - s->inp + 3 - ncr);
        s->inl -= 2 + ncr;
        return 1;
    }
    return 0;
}

int fillbuf(source* s) noexcept {
    int n;

    while ((char*) s->inl + s->ins / 8 > (char*) s->inb + s->ins) {
        int l = s->inl - s->inb;
        int p = s->inp - s->inb;
        if (l < 0) error(FATAL, "negative end of input!?");
        if (p < 0) error(FATAL, "negative input pointer!?");
        /* double the buffer size and try again */
        s->ins *= 2;
        s->inb  = _checked_realloc(s->inb, s->ins);
        s->inl  = s->inb + l;
        s->inp  = s->inb + p;
    }
    if (s->fd < 0 || (n = read(s->fd, (char*) s->inl, s->ins / 8)) <= 0) n = 0;
    if ((*s->inp & 0xff) == EOB) /* sentinel character appears in input */
        *s->inp = EOFC;
    s->inl    += n;
    s->inl[0] = s->inl[1] = s->inl[2] = s->inl[3] = EOB;
    if (n == 0) {
        s->inl[0] = s->inl[1] = s->inl[2] = s->inl[3] = EOFC;
        return EOF;
    }
    return 0;
}

/*
 * Push down to new source of characters.
 * If fd>0 and str==nullptr, then from a file `name';
 * if fd==-1 and str, then from the string.
 */
source* setsource(char* name, int fd, char* str) noexcept {
    source* s = _new_obj<source>();
    int     len;

    s->line     = 1;
    s->lineinc  = 0;
    s->fd       = fd;
    s->filename = name;
    s->next     = cursource;
    s->ifdepth  = 0;
    cursource   = s;
    /* slop at right for EOB */
    if (str) {
        len    = strlen(str);
        s->inb = _checked_malloc(len + 4);
        s->inp = s->inb;
        strncpy((char*) s->inp, str, len);
    } else {
        Dir*  d;
        int   junk;
        ulong length = 0;
        d            = dirfstat(fd);
        if (d != nil) {
            length = d->length;
            free(d);
        }
        junk = length;
        if (junk < INPUT_BUFFER_SIZE) junk = INPUT_BUFFER_SIZE;
        s->inb = _checked_malloc((junk) + 4);
        s->inp = s->inb;
        len    = 0;
    }

    s->ins    = INPUT_BUFFER_SIZE;
    s->inl    = s->inp + len;
    s->inl[0] = s->inl[1] = EOB;
    return s;
}

void unsetsource() noexcept {
    source* s = cursource;

    if (s->fd >= 0) {
        close(s->fd);
        free(s->inb);
    }
    cursource = s->next;
    free(s);
}
