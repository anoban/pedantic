#include <cstdio>

#include <prep.hpp>

/*
 * do a macro definition.  tp points to the name being defined in the line
 */
void dodefine(token_row* trp) {
    token*     tp;
    nlist*     np;
    token_row *def, *args;
    int        dots;

    dots = 0;
    tp   = trp->tp + 1;
    if (tp >= trp->lp || tp->type != NAME) {
        error(ERROR, "#defined token is not a name");
        return;
    }
    np = lookup(tp, 1);
    if (np->flag & UNCHANGEABLE) {
        error(ERROR, "#defined token %t can't be redefined", tp);
        return;
    }
    /* collect arguments */
    tp   += 1;
    args  = nullptr;
    if (tp < trp->lp && tp->type == LP && tp->wslen == 0) {
        /* macro with args */
        int narg  = 0;
        tp       += 1;
        args      = _new_obj<token_row>();
        maketokenrow(2, args);
        if (tp->type != RP) {
            int err = 0;
            for (;;) {
                token* atp;
                if (tp->type == ELLIPS)
                    dots++;
                else if (tp->type != NAME) {
                    err++;
                    break;
                }
                if (narg >= args->max) growtokenrow(args);
                for (atp = args->bp; atp < args->lp; atp++)
                    if (atp->len == tp->len && strncmp((char*) atp->t, (char*) tp->t, tp->len) == 0)
                        error(ERROR, "Duplicate macro argument");
                *args->lp++ = *tp;
                narg++;
                tp += 1;
                if (tp->type == RP) break;
                if (dots) error(ERROR, "arguments after '...' in macro");
                if (tp->type != COMMA) {
                    err++;
                    break;
                }
                tp += 1;
            }
            if (err) {
                error(ERROR, "Syntax error in macro parameters");
                return;
            }
        }
        tp += 1;
    }
    trp->tp = tp;
    if (((trp->lp) - 1)->type == NL) trp->lp -= 1;
    def = normtokenrow(trp);
    if (np->flag & DEFINED_VALUE) {
        if (comparetokens(def, np->vp) || (np->ap == nullptr) != (args == nullptr) || np->ap && comparetokens(args, np->ap))
            error(ERROR, "Macro redefinition of %t", trp->bp + 2);
    }
    if (args) {
        token_row* tap;
        tap = normtokenrow(args);
        free(args->bp);
        args = tap;
    }
    np->ap    = args;
    np->vp    = def;
    np->flag |= DEFINED_VALUE;
    if (dots) np->flag |= VARIADIC_MACRO;
}

/*
 * Definition received via -D or -U
 */
void doadefine(token_row* trp, int type) {
    nlist*               np;
    static unsigned char one[]       = "1";
    static token         onetoken[1] = {
        { NUMBER, 0, 0, 0, 1, one }
    };
    static token_row onetr = { onetoken, onetoken, onetoken + 1, 1 };

    trp->tp                = trp->bp;
    if (type == 'U') {
        if (trp->lp - trp->tp != 2 || trp->tp->type != NAME) goto syntax;
        if ((np = lookup(trp->tp, 0)) == nullptr) return;
        np->flag &= ~DEFINED_VALUE;
        return;
    }
    if (trp->tp >= trp->lp || trp->tp->type != NAME) goto syntax;
    np        = lookup(trp->tp, 1);
    np->flag |= DEFINED_VALUE;
    trp->tp  += 1;
    if (trp->tp >= trp->lp || trp->tp->type == END) {
        np->vp = &onetr;
        return;
    }
    if (trp->tp->type != ASGN) goto syntax;
    trp->tp += 1;
    if ((trp->lp - 1)->type == END) trp->lp -= 1;
    np->vp = normtokenrow(trp);
    return;
syntax:
    error(FATAL, "Illegal -D or -U argument %r", trp);
}

/*
 * Do macro expansion in a row of tokens.
 * Flag is nullptr if more input can be gathered.
 */
void expandrow(token_row* trp, char* flag, int inmacro) {
    token* tp;
    nlist* np;

    if (flag) setsource(flag, -1, "");
    for (tp = trp->tp; tp < trp->lp;) {
        if (tp->type != NAME || quicklook(tp->t[0], tp->len > 1 ? tp->t[1] : 0) == 0 || (np = lookup(tp, 0)) == nullptr ||
            (np->flag & (DEFINED_VALUE | BUILTIN)) == 0 || tp->hideset && check_hideset(tp->hideset, np)) {
            tp++;
            continue;
        }
        trp->tp = tp;
        if (np->val == KDEFINED) {
            tp->type = DEFINED;
            if ((tp + 1) < trp->lp && (tp + 1)->type == NAME)
                (tp + 1)->type = NAME1;
            else if ((tp + 3) < trp->lp && (tp + 1)->type == LP && (tp + 2)->type == NAME && (tp + 3)->type == RP)
                (tp + 2)->type = NAME1;
            else
                error(ERROR, "Incorrect syntax for `defined'");
            tp++;
            continue;
        }
        if (np->flag & BUILTIN)
            builtin(trp, np->val);
        else
            expand(trp, np, inmacro);
        tp = trp->tp;
    }
    if (flag) unsetsource();
}

/*
 * Expand the macro whose name is np, at token trp->tp, in the tokenrow.
 * Return trp->tp at the first token next to be expanded
 * (ordinarily the beginning of the expansion)
 */
void expand(token_row* trp, nlist* np, int inmacro) {
    token_row  ntr;
    int        ntokc, narg, i;
    token*     tp;
    token_row* atr[MAX_MACRO_ARGS + 1];
    int        hs;

    copytokenrow(&ntr, np->vp); /* copy macro value */
    if (np->ap == nullptr)      /* parameterless */
        ntokc = 1;
    else {
        ntokc = gatherargs(trp, atr, (np->flag & VARIADIC_MACRO) ? tokenrow_len(np->ap) : 0, &narg);
        if (narg < 0) { /* not actually a call (no '(') */
                        /* error(WARNING, "%d %r\n", narg, trp); */
            /* gatherargs has already pushed trp->tr to the next token */
            return;
        }
        if (narg != tokenrow_len(np->ap)) {
            error(ERROR, "Disagreement in number of macro arguments");
            trp->tp->hideset  = new_hideset(trp->tp->hideset, np);
            trp->tp          += ntokc;
            return;
        }
        substargs(np, &ntr, atr); /* put args into replacement */
        for (i = 0; i < narg; i++) {
            free(atr[i]->bp);
            free(atr[i]);
        }
    }
    if (!inmacro) doconcat(&ntr); /* execute ## operators */
    hs = new_hideset(trp->tp->hideset, np);
    for (tp = ntr.bp; tp < ntr.lp; tp++) { /* distribute hidesets */
        if (tp->type == NAME) {
            if (tp->hideset == 0)
                tp->hideset = hs;
            else
                tp->hideset = unionhideset(tp->hideset, hs);
        }
    }
    ntr.tp = ntr.bp;
    insertrow(trp, ntokc, &ntr);
    trp->tp -= tokenrow_len(&ntr);
    free(ntr.bp);
    return;
}

/*
 * Gather an arglist, starting in trp with tp pointing at the macro name.
 * Return total number of tokens passed, stash number of args found.
 * trp->tp is not changed relative to the tokenrow.
 */
int gatherargs(token_row* trp, token_row** atr, int dots, int* narg) {
    int       parens = 1;
    int       ntok   = 0;
    token *   bp, *lp;
    token_row ttr;
    int       ntokp;
    int       needspace;

    *narg = -1; /* means that there is no macro call */
    /* look for the ( */
    for (;;) {
        trp->tp++;
        ntok++;
        if (trp->tp >= trp->lp) {
            gettokens(trp, 0);
            if ((trp->lp - 1)->type == END) {
                /* error(WARNING, "reach END\n"); */
                trp->lp -= 1;
                if (*narg >= 0) trp->tp -= ntok;
                return ntok;
            }
        }
        if (trp->tp->type == LP) break;
        if (trp->tp->type != NL) return ntok;
    }
    *narg = 0;
    ntok++;
    ntokp = ntok;
    trp->tp++;
    /* search for the terminating ), possibly extending the row */
    needspace = 0;
    while (parens > 0) {
        if (trp->tp >= trp->lp) gettokens(trp, 0);
        if (needspace) {
            needspace = 0;
            makespace(trp);
        }
        if (trp->tp->type == END) {
            trp->lp -= 1;
            trp->tp -= ntok;
            error(ERROR, "EOF in macro arglist");
            return ntok;
        }
        if (trp->tp->type == NL) {
            trp->tp += 1;
            adjustrow(trp, -1);
            trp->tp -= 1;
            makespace(trp);
            needspace = 1;
            continue;
        }
        if (trp->tp->type == LP)
            parens++;
        else if (trp->tp->type == RP)
            parens--;
        trp->tp++;
        ntok++;
    }
    trp->tp -= ntok;
    /* Now trp->tp won't move underneath us */
    lp = bp = trp->tp + ntokp;
    for (; parens >= 0; lp++) {
        if (lp->type == LP) {
            parens++;
            continue;
        }
        if (lp->type == RP) parens--;
        if (lp->type == DSHARP) lp->type = DSHARP1; /* ## not special in arg */
        if ((lp->type == COMMA && parens == 0) || (parens < 0 && (lp - 1)->type != LP)) {
            if (lp->type == COMMA && dots && *narg == dots - 1) continue;
            if (*narg >= MAX_MACRO_ARGS - 1) error(FATAL, "Sorry, too many macro arguments");
            ttr.bp = ttr.tp = bp;
            ttr.lp          = lp;
            atr[(*narg)++]  = normtokenrow(&ttr);
            bp              = lp + 1;
        }
    }
    return ntok;
}

/*
 * substitute the argument list into the replacement string
 *  This would be simple except for ## and #
 */
void substargs(nlist* np, token_row* rtr, token_row** atr) {
    token_row tatr;
    token*    tp;
    int       ntok, argno;

    for (rtr->tp = rtr->bp; rtr->tp < rtr->lp;) {
        if (rtr->tp->type == SHARP) { /* string operator */
            tp       = rtr->tp;
            rtr->tp += 1;
            if ((argno = lookuparg(np, rtr->tp)) < 0) {
                error(ERROR, "# not followed by macro parameter");
                continue;
            }
            ntok    = 1 + (rtr->tp - tp);
            rtr->tp = tp;
            insertrow(rtr, ntok, stringify(atr[argno]));
            continue;
        }
        if (rtr->tp->type == NAME && (argno = lookuparg(np, rtr->tp)) >= 0) {
            if (rtr->tp < rtr->bp) error(ERROR, "access out of bounds");
            if ((rtr->tp + 1)->type == DSHARP || rtr->tp != rtr->bp && (rtr->tp - 1)->type == DSHARP)
                insertrow(rtr, 1, atr[argno]);
            else {
                copytokenrow(&tatr, atr[argno]);
                expandrow(&tatr, "<macro>", IN_MACRO);
                insertrow(rtr, 1, &tatr);
                free(tatr.bp);
            }
            continue;
        }
        rtr->tp++;
    }
}

/*
 * Evaluate the ## operators in a tokenrow
 */
void doconcat(token_row* trp) {
    token *   ltp, *ntp;
    token_row ntr;
    int       len;

    for (trp->tp = trp->bp; trp->tp < trp->lp; trp->tp++) {
        if (trp->tp->type == DSHARP1)
            trp->tp->type = DSHARP;
        else if (trp->tp->type == DSHARP) {
            char tt[128];
            ltp = trp->tp - 1;
            ntp = trp->tp + 1;
            if (ltp < trp->bp || ntp >= trp->lp) {
                error(ERROR, "## occurs at border of replacement");
                continue;
            }
            len = ltp->len + ntp->len;
            strncpy((char*) tt, (char*) ltp->t, ltp->len);
            strncpy((char*) tt + ltp->len, (char*) ntp->t, ntp->len);
            tt[len] = '\0';
            setsource("<##>", -1, tt);
            maketokenrow(3, &ntr);
            gettokens(&ntr, 1);
            unsetsource();
            if (ntr.lp - ntr.bp != 2 || ntr.bp->type == UNCLASS) error(WARNING, "Bad token %r produced by ##", &ntr);
            ntr.lp  = ntr.bp + 1;
            trp->tp = ltp;
            makespace(&ntr);
            insertrow(trp, (ntp - ltp) + 1, &ntr);
            free(ntr.bp);
            trp->tp--;
        }
    }
}

/*
 * tp is a potential parameter name of macro mac;
 * look it up in mac's arglist, and if found, return the
 * corresponding index in the argname array.  Return -1 if not found.
 */
int lookuparg(nlist* mac, token* tp) {
    token* ap;

    if (tp->type != NAME || mac->ap == nullptr) return -1;
    if ((mac->flag & VARIADIC_MACRO) && strcmp((char*) tp->t, "__VA_ARGS__") == 0) return tokenrow_len(mac->ap) - 1;
    for (ap = mac->ap->bp; ap < mac->ap->lp; ap++)
        if (ap->len == tp->len && strncmp((char*) ap->t, (char*) tp->t, ap->len) == 0) return ap - mac->ap->bp;
    return -1;
}

/*
 * Return a quoted version of the tokenrow (from # arg)
 */
#define STRLEN 512
token_row* stringify(token_row* vp) {
    static token     t  = { STRING };
    static token_row tr = { &t, &t, &t + 1, 1 };
    token*           tp;
    unsigned char    s[STRLEN];
    unsigned char *  sp = s, *cp;
    int              i, instring;

    *sp++ = '"';
    for (tp = vp->bp; tp < vp->lp; tp++) {
        instring = tp->type == STRING || tp->type == CCON;
        if (sp + 2 * tp->len >= &s[STRLEN - 10]) {
            error(ERROR, "Stringified macro arg is too long");
            break;
        }
        if (tp->wslen /* && (tp->flag&XPWS)==0 */) *sp++ = ' ';
        for (i = 0, cp = tp->t; i < tp->len; i++) {
            if (instring && (*cp == '"' || *cp == '\\')) *sp++ = '\\';
            *sp++ = *cp++;
        }
    }
    *sp++ = '"';
    *sp   = '\0';
    sp    = s;
    t.len = strlen((char*) sp);
    t.t   = newstring(sp, t.len, 0);
    return &tr;
}

/*
 * expand a builtin name
 */
void builtin(token_row* trp, int biname) {
    char*   op;
    token*  tp;
    source* s;

    tp = trp->tp;
    trp->tp++;
    /* need to find the real source */
    s = cursource;
    while (s && s->fd == -1) s = s->next;
    if (s == nullptr) s = cursource;
    /* most are strings */
    tp->type = STRING;
    if (tp->wslen) {
        *outp++   = ' ';
        tp->wslen = 1;
    }
    op    = outp;
    *op++ = '"';
    switch (biname) {
        case KLINENO :
            tp->type = NUMBER;
            op       = outnum(op - 1, s->line);
            break;

        case KFILE :
            strcpy(op, s->filename);
            op += strlen(s->filename);
            break;

        case KDATE :
            strncpy(op, current_time + 4, 7);
            strncpy(op + 7, current_time + 24, 4); /* Plan 9 asctime disobeys standard */
            op += 11;
            break;

        case KTIME :
            strncpy(op, current_time + 11, 8);
            op += 8;
            break;

        default : error(ERROR, "cpp botch: unknown internal macro"); return;
    }
    if (tp->type == STRING) *op++ = '"';
    tp->t   = (unsigned char*) outp;
    tp->len = op - outp;
    outp    = op;
}
