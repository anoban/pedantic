#include <prep.hpp>

static char  wbuf[2 * OUTPUT_BUFFER_SIZE];
static char* wbp                         = wbuf;

// true for tokens that don't need whitespace when they get inserted by macro expansion
static constexpr bool whitespace_table[] = {
    false, // END */
    false, // UNCLASS */
    false, // NAME */
    false, // NUMBER */
    false, // STRING */
    false, // CCON */
    true,  // NL */
    false, // WS */
    false, // DSHARP */
    false, // EQ */
    false, // NEQ */
    false, // LEQ */
    false, // GEQ */
    false, // LSH */
    false, // RSH */
    false, // LAND */
    false, // LOR */
    false, // PPLUS */
    false, // MMINUS */
    false, // ARROW */
    true,  // SBRA */
    true,  // SKET */
    true,  // LP */
    true,  // RP */
    false, // DOT */
    false, // AND */
    false, // STAR */
    false, // PLUS */
    false, // MINUS */
    false, // TILDE */
    false, // NOT */
    false, // SLASH */
    false, // PCT */
    false, // LT */
    false, // GT */
    false, // CIRC */
    false, // OR */
    false, // QUEST */
    false, // COLON */
    false, // ASGN */
    true,  // COMMA */
    false, // SHARP */
    true,  // SEMIC */
    true,  // CBRA */
    true,  // CKET */
    false, // ASPLUS */
    false, // ASMINUS */
    false, // ASSTAR */
    false, // ASSLASH */
    false, // ASPCT */
    false, // ASCIRC */
    false, // ASLSH */
    false, // ASRSH */
    false, // ASOR */
    false, // ASAND */
    false, // ELLIPS */
    false, // DSHARP1 */
    false, // NAME1 */
    false, // DEFINED */
    false, // UMINUS */
};

void maketokenrow(int size, token_row* trp) noexcept {
    trp->max = size;
    if (size > 0)
        trp->bp = (token*) _checked_malloc(size * sizeof(token));
    else
        trp->bp = NULL;
    trp->tp = trp->bp;
    trp->lp = trp->bp;
}

token* growtokenrow(token_row* trp) {
    int ncur  = trp->tp - trp->bp;
    int nlast = trp->lp - trp->bp;

    trp->max  = 3 * trp->max / 2 + 1;
    trp->bp   = (token*) realloc(trp->bp, trp->max * sizeof(token));
    trp->lp   = &trp->bp[nlast];
    trp->tp   = &trp->bp[ncur];
    return trp->lp;
}

/*
 * Compare a row of tokens, ignoring the content of WS; return !=0 if different
 */
int comparetokens(token_row* tr1, token_row* tr2) {
    token *tp1, *tp2;

    tp1 = tr1->tp;
    tp2 = tr2->tp;
    if (tr1->lp - tp1 != tr2->lp - tp2) return 1;
    for (; tp1 < tr1->lp; tp1++, tp2++)
        if (tp1->type != tp2->type || (tp1->wslen == 0) != (tp2->wslen == 0) || tp1->len != tp2->len ||
            strncmp((char*) tp1->t, (char*) tp2->t, tp1->len) != 0)
            return 1;
    return 0;
}

/*
 * replace ntokens tokens starting at dest->tp with the contents of src.
 * tp ends up pointing just beyond the replacement.
 * Canonical whitespace is assured on each side.
 */
void insertrow(token_row* dest, size_t ntokens, token_row* src) {
    int nrtok  = rowlen(src);

    dest->tp  += ntokens;
    adjustrow(dest, nrtok - ntokens);
    dest->tp -= ntokens;
    movetokenrow(dest, src);
    makespace(dest);
    dest->tp += nrtok;
    makespace(dest);
}

/*
 * make sure there is WS before trp->tp, if tokens might merge in the output
 */
void makespace(token_row* trp) {
    unsigned char* tt;
    token*         tp = trp->tp;

    if (tp >= trp->lp) return;
    if (tp->wslen) {
        if (tp->flag & XPWS && (whitespace_table[tp->type] || trp->tp > trp->bp && whitespace_table[(tp - 1)->type])) {
            tp->wslen = 0;
            return;
        }
        tp->t[-1] = ' ';
        return;
    }
    if (whitespace_table[tp->type] || trp->tp > trp->bp && whitespace_table[(tp - 1)->type]) return;
    tt         = newstring(tp->t, tp->len, 1);
    *tt++      = ' ';
    tp->t      = tt;
    tp->wslen  = 1;
    tp->flag  |= XPWS;
}

/*
 * Copy an entire tokenrow into another, at tp.
 * It is assumed that there is enough space.
 *  Not strictly conforming.
 */
void movetokenrow(token_row* dtr, token_row* str) {
    int nby;

    /* nby = sizeof(token) * (src->lp - src->bp); */
    nby = (char*) str->lp - (char*) str->bp;
    memmove(dtr->tp, str->bp, nby);
}

/*
 * Move the tokens in a row, starting at tr->tp, rightward by nt tokens;
 * nt may be negative (left move).
 * The row may need to be grown.
 * Non-strictly conforming because of the (char *), but easily fixed
 */
void adjustrow(token_row* trp, int nt) {
    int nby, size;

    if (nt == 0) return;
    size = (trp->lp - trp->bp) + nt;
    while (size > trp->max) growtokenrow(trp);
    /* nby = sizeof(token) * (trp->lp - trp->tp); */
    nby = (char*) trp->lp - (char*) trp->tp;
    if (nby) memmove(trp->tp + nt, trp->tp, nby);
    trp->lp += nt;
}

/*
 * Copy a row of tokens into the destination holder, allocating
 * the space for the contents.  Return the destination.
 */
token_row* copytokenrow(token_row* dtr, token_row* str) {
    int len = rowlen(str);

    maketokenrow(len, dtr);
    movetokenrow(dtr, str);
    dtr->lp += len;
    return dtr;
}

/*
 * Produce a copy of a row of tokens.  Start at trp->tp.
 * The value strings are copied as well.  The first token
 * has WS available.
 */
token_row* normtokenrow(token_row* trp) {
    token*     tp;
    token_row* ntrp = _new_obj<token_row>();
    int        len;

    len = trp->lp - trp->tp;
    if (len <= 0) len = 1;
    maketokenrow(len, ntrp);
    for (tp = trp->tp; tp < trp->lp; tp++) {
        *ntrp->lp = *tp;
        if (tp->len) {
            ntrp->lp->t    = newstring(tp->t, tp->len, 1);
            *ntrp->lp->t++ = ' ';
            if (tp->wslen) ntrp->lp->wslen = 1;
        }
        ntrp->lp++;
    }
    if (ntrp->lp > ntrp->bp) ntrp->bp->wslen = 0;
    return ntrp;
}

/*
 * Debugging
 */
void peektokens(token_row* trp, char* str) {
    token* tp;
    int    c;

    tp = trp->tp;
    flushout();
    if (str) fprintf(stderr, "%s ", str);
    if (tp < trp->bp || tp > trp->lp) fprintf(stderr, "(tp offset %d) ", tp - trp->bp);
    for (tp = trp->bp; tp < trp->lp && tp < trp->bp + 32; tp++) {
        if (tp->type != NL) {
            c              = tp->t[tp->len];
            tp->t[tp->len] = 0;
            fprintf(stderr, "%s", tp->t, tp->len);
            tp->t[tp->len] = c;
        }
        if (tp->type == NAME) {
            fprintf(stderr, tp == trp->tp ? "{*" : "{");
            prhideset(tp->hideset);
            fprintf(stderr, "} ");
        } else
            fprintf(stderr, tp == trp->tp ? "{%x*} " : "{%x} ", tp->type);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}

void puttokens(token_row* trp) {
    token*         tp;
    int            len;
    unsigned char* p;

    if (verbose) peektokens(trp, "");
    tp = trp->bp;
    for (; tp < trp->lp; tp++) {
        len = tp->len + tp->wslen;
        p   = tp->t - tp->wslen;
        while (tp < trp->lp - 1 && p + len == (tp + 1)->t - (tp + 1)->wslen) {
            tp++;
            len += tp->wslen + tp->len;
        }
        if (Mflag == 0) {
            if (len > OUTPUT_BUFFER_SIZE / 2) { /* handle giant token */
                if (wbp > wbuf) write(1, wbuf, wbp - wbuf);
                write(1, p, len);
                wbp = wbuf;
            } else {
                memcpy(wbp, p, len);
                wbp += len;
            }
        }
        if (wbp >= &wbuf[OUTPUT_BUFFER_SIZE]) {
            write(1, wbuf, OUTPUT_BUFFER_SIZE);
            if (wbp > &wbuf[OUTPUT_BUFFER_SIZE]) memcpy(wbuf, wbuf + OUTPUT_BUFFER_SIZE, wbp - &wbuf[OUTPUT_BUFFER_SIZE]);
            wbp -= OUTPUT_BUFFER_SIZE;
        }
    }
    trp->tp = tp;
    if (cursource->fd == 0) flushout();
}

void flushout(void) {
    if (wbp > wbuf) {
        write(1, wbuf, wbp - wbuf);
        wbp = wbuf;
    }
}

// turn a row into just a newline
void setempty(_Inout_ token_row* const trp) noexcept {
    trp->tp  = trp->bp;
    trp->lp  = trp->bp + 1;
    *trp->bp = nltoken;
}

/*
 * generate a number
 */
char* outnum(char* p, int n) {
    if (n >= 10) p = outnum(p, n / 10);
    *p++ = n % 10 + '0';
    return p;
}

/*
 * allocate and initialize a new string from string, of length length, at offset offset
 * Null terminated.
 */
char* newstring(_In_ const char* const string, _In_ const size_t length, _In_ const size_t offset) noexcept {
    char* str            = reinterpret_cast<char*>(::_checked_malloc(length + offset + 1));
    str[length + offset] = '\0';
    return ::strncpy((char*) str + offset, (char*) string, length) - offset;
}
