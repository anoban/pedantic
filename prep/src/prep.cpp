#include <cstdarg>
#include <ctime>

#include <prep.hpp>

static constexpr size_t OUT_BUFF_SIZE { 16'384 };

char    outbuf[OUT_BUFF_SIZE];
char*   outp { outbuf };
source* cursource {};
int     nerrs {};
token   nltoken { token_type::NL, 0, 0, 0, 1, "\n" };
char*   curtime {};
int     incdepth {};
int     ifdepth {};
int     ifsatisfied[MAX_NESTED_IF_DEPTH] {};
int     skipping {};

int wmain(_In_opt_ int argc, _In_opt_count_(argc) wchar_t* argv[]) {
    token_row tr;
    long      t;
    char      ebuf[BUFSIZ];

    setbuf(stderr, ebuf);
    t       = time(NULL);
    curtime = ctime(t);
    maketokenrow(3, &tr);
    expandlex();
    setup(argc, argv);
    fixlex();
    iniths();
    genline();
    process(&tr);
    flushout();
    fflush(stderr);
    exits(nerrs ? "errors" : 0);

    return EXIT_SUCCESS;
}

void process(_In_ token_row* const tknrw) noexcept {
    int anymacros {};

    for (;;) {
        if (tknrw->tp >= tknrw->lp) {
            tknrw->tp = tknrw->lp  = tknrw->bp;
            outp                   = outbuf;
            anymacros             |= gettokens(tknrw, 1);
            tknrw->tp              = tknrw->bp;
        }
        if (tknrw->tp->type == END) {
            if (--incdepth >= 0) {
                if (cursource->ifdepth) error(ERROR, "Unterminated conditional in #include");
                unsetsource();
                cursource->line += cursource->lineinc;
                tknrw->tp        = tknrw->lp;
                genline();
                continue;
            }
            if (ifdepth) error(ERROR, "Unterminated #if/#ifdef/#ifndef");
            break;
        }
        if (tknrw->tp->type == SHARP) {
            tknrw->tp += 1;
            control(tknrw);
        } else if (!skipping && anymacros)
            expandrow(tknrw, NULL, NOT_IN_MACRO);
        if (skipping) setempty(tknrw);
        puttokens(tknrw);
        anymacros        = 0;
        cursource->line += cursource->lineinc;
        if (cursource->lineinc > 1) genline();
    }
}

void control(token_row* trp) noexcept {
    Nlist* np {};
    token* tp {};

    tp = trp->tp;
    if (tp->type != NAME) {
        if (tp->type == NUMBER) goto kline;
        if (tp->type != NL) error(ERROR, "Unidentifiable control line");
        return; /* else empty line */
    }
    if ((np = lookup(tp, 0)) == NULL || (np->flag & ISKW) == 0 && !skipping) {
        error(WARNING, "Unknown preprocessor control %t", tp);
        return;
    }
    if (skipping) {
        if ((np->flag & ISKW) == 0) return;
        switch (np->val) {
            case KENDIF :
                if (--ifdepth < skipping) skipping = 0;
                --cursource->ifdepth;
                setempty(trp);
                return;

            case KIFDEF :
            case KIFNDEF :
            case KIF :
                if (++ifdepth >= MAX_NESTED_IF_DEPTH) error(FATAL, "#if too deeply nested");
                ++cursource->ifdepth;
                return;

            case KELIF :
            case KELSE :
                if (ifdepth <= skipping) break;
                return;

            default : return;
        }
    }
    switch (np->val) {
        case KDEFINE : dodefine(trp); break;

        case KUNDEF :
            tp += 1;
            if (tp->type != NAME || trp->lp - trp->bp != 4) {
                error(ERROR, "Syntax error in #undef");
                break;
            }
            if ((np = lookup(tp, 0))) {
                if (np->flag & ISUNCHANGE) {
                    error(ERROR, "#defined token %t can't be undefined", tp);
                    return;
                }
                np->flag &= ~ISDEFINED;
            }
            break;

        case KPRAGMA : return;

        case KIFDEF :
        case KIFNDEF :
        case KIF :
            if (++ifdepth >= MAX_NESTED_IF_DEPTH) error(FATAL, "#if too deeply nested");
            ++cursource->ifdepth;
            ifsatisfied[ifdepth] = 0;
            if (eval(trp, np->val))
                ifsatisfied[ifdepth] = 1;
            else
                skipping = ifdepth;
            break;

        case KELIF :
            if (ifdepth == 0) {
                error(ERROR, "#elif with no #if");
                return;
            }
            if (ifsatisfied[ifdepth] == 2) error(ERROR, "#elif after #else");
            if (eval(trp, np->val)) {
                if (ifsatisfied[ifdepth])
                    skipping = ifdepth;
                else {
                    skipping             = 0;
                    ifsatisfied[ifdepth] = 1;
                }
            } else
                skipping = ifdepth;
            break;

        case KELSE :
            if (ifdepth == 0 || cursource->ifdepth == 0) {
                error(ERROR, "#else with no #if");
                return;
            }
            if (ifsatisfied[ifdepth] == 2) error(ERROR, "#else after #else");
            if (trp->lp - trp->bp != 3) error(ERROR, "Syntax error in #else");
            skipping             = ifsatisfied[ifdepth] ? ifdepth : 0;
            ifsatisfied[ifdepth] = 2;
            break;

        case KENDIF :
            if (ifdepth == 0 || cursource->ifdepth == 0) {
                error(ERROR, "#endif with no #if");
                return;
            }
            --ifdepth;
            --cursource->ifdepth;
            if (trp->lp - trp->bp != 3) error(WARNING, "Syntax error in #endif");
            break;

        case KERROR :
            trp->tp = tp + 1;
            error(ERROR, "#error directive: %r", trp);
            break;

        case KWARNING :
            trp->tp = tp + 1;
            error(WARNING, "#warning directive: %r", trp);
            break;

        case KLINE :
            trp->tp = tp + 1;
            expandrow(trp, "<line>", NOT_IN_MACRO);
            tp = trp->bp + 2;
kline:
            if (tp + 1 >= trp->lp || tp->type != NUMBER || tp + 3 < trp->lp ||
                (tp + 3 == trp->lp && ((tp + 1)->type != STRING) || *(tp + 1)->t == 'L')) {
                error(ERROR, "Syntax error in #line");
                return;
            }
            cursource->line = atol((char*) tp->t) - 1;
            if (cursource->line < 0 || cursource->line >= 32768) error(WARNING, "#line specifies number out of range");
            tp = tp + 1;
            if (tp + 1 < trp->lp) cursource->filename = (char*) newstring(tp->t + 1, tp->len - 2, 0);
            return;

        case KDEFINED : error(ERROR, "Bad syntax for control line"); break;

        case KINCLUDE :
            doinclude(trp);
            trp->lp = trp->bp;
            return;

        case KEVAL : eval(trp, np->val); break;

        default    : error(ERROR, "Preprocessor control `%t' not yet implemented", tp); break;
    }
    setempty(trp);
    return;
}
