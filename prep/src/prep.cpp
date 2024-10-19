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
    token_row    tr;
    const time_t now { time(nullptr) };
    char         ebuf[OUT_BUFF_SIZE];

    setbuf(stderr, ebuf);

    curtime = ctime(&now);
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

        if (tknrw->tp->type == token_type::END) {
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

void control(token_row* tknrw) noexcept {
    Nlist* np {};
    token* tknptr {};

    tknptr = tknrw->tp;
    if (tknptr->type != NAME) {
        if (tknptr->type == NUMBER) goto kline;
        if (tknptr->type != NL) error(ERROR, "Unidentifiable control line");
        return; /* else empty line */
    }

    if ((np = lookup(tknptr, 0)) == NULL || (np->flag & ISKW) == 0 && !skipping) {
        error(WARNING, "Unknown preprocessor control %t", tknptr);
        return;
    }

    if (skipping) {
        if ((np->flag & ISKW) == 0) return;
        switch (np->val) {
            case KENDIF :
                if (--ifdepth < skipping) skipping = 0;
                --cursource->ifdepth;
                setempty(tknrw);
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
        case KDEFINE : dodefine(tknrw); break;

        case KUNDEF :
            tknptr += 1;
            if (tknptr->type != NAME || tknrw->lp - tknrw->bp != 4) {
                error(ERROR, "Syntax error in #undef");
                break;
            }
            if ((np = lookup(tknptr, 0))) {
                if (np->flag & ISUNCHANGE) {
                    error(ERROR, "#defined token %t can't be undefined", tknptr);
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
            if (eval(tknrw, np->val))
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
            if (eval(tknrw, np->val)) {
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
            if (tknrw->lp - tknrw->bp != 3) error(ERROR, "Syntax error in #else");
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
            if (tknrw->lp - tknrw->bp != 3) error(WARNING, "Syntax error in #endif");
            break;

        case KERROR :
            tknrw->tp = tknptr + 1;
            error(ERROR, "#error directive: %r", tknrw);
            break;

        case KWARNING :
            tknrw->tp = tknptr + 1;
            error(WARNING, "#warning directive: %r", tknrw);
            break;

        case KLINE :
            tknrw->tp = tknptr + 1;
            expandrow(tknrw, "<line>", NOT_IN_MACRO);
            tknptr = tknrw->bp + 2;
kline:
            if (tknptr + 1 >= tknrw->lp || tknptr->type != NUMBER || tknptr + 3 < tknrw->lp ||
                (tknptr + 3 == tknrw->lp && ((tknptr + 1)->type != STRING) || *(tknptr + 1)->t == 'L')) {
                error(ERROR, "Syntax error in #line");
                return;
            }
            cursource->line = atol((char*) tknptr->t) - 1;
            if (cursource->line < 0 || cursource->line >= 32768) error(WARNING, "#line specifies number out of range");
            tknptr = tknptr + 1;
            if (tknptr + 1 < tknrw->lp) cursource->filename = (char*) newstring(tknptr->t + 1, tknptr->len - 2, 0);
            return;

        case KDEFINED : error(ERROR, "Bad syntax for control line"); break;

        case KINCLUDE :
            doinclude(tknrw);
            tknrw->lp = tknrw->bp;
            return;

        case KEVAL : eval(tknrw, np->val); break;

        default    : error(ERROR, "Preprocessor control `%t' not yet implemented", tknptr); break;
    }
    setempty(tknrw);
    return;
}
