#include <prep.hpp>

#define NSTAK   1024
#define SGN     0
#define UNS     1
#define UND     2

#define UNSMARK 0x1000

struct value final {
        long val;
        long type;
};

// conversion types
enum class CNVRSNTYPE : char { NONE, RELAT, ARITH, LOGIC, SPCL, SHIFT, UNARY };

struct priority {
        char       pri;
        char       arity;
        CNVRSNTYPE ctype;
};

// operator priority, arity, and conversion type, indexed by tokentype
static constexpr priority operator_priority[] = {
    {  0, 0,  CNVRSNTYPE::NONE }, // END
    {  0, 0,  CNVRSNTYPE::NONE }, // UNCLASS
    {  0, 0,  CNVRSNTYPE::NONE }, // NAME
    {  0, 0,  CNVRSNTYPE::NONE }, // NUMBER
    {  0, 0,  CNVRSNTYPE::NONE }, // STRING
    {  0, 0,  CNVRSNTYPE::NONE }, // CCON
    {  0, 0,  CNVRSNTYPE::NONE }, // NL
    {  0, 0,  CNVRSNTYPE::NONE }, // WS
    {  0, 0,  CNVRSNTYPE::NONE }, // DSHARP
    { 11, 2, CNVRSNTYPE::RELAT }, // EQ
    { 11, 2, CNVRSNTYPE::RELAT }, // NEQ
    { 12, 2, CNVRSNTYPE::RELAT }, // LEQ
    { 12, 2, CNVRSNTYPE::RELAT }, // GEQ
    { 13, 2, CNVRSNTYPE::SHIFT }, // LSH
    { 13, 2, CNVRSNTYPE::SHIFT }, // RSH
    {  7, 2, CNVRSNTYPE::LOGIC }, // LAND
    {  6, 2, CNVRSNTYPE::LOGIC }, // LOR
    {  0, 0,  CNVRSNTYPE::NONE }, // PPLUS
    {  0, 0,  CNVRSNTYPE::NONE }, // MMINUS
    {  0, 0,  CNVRSNTYPE::NONE }, // ARROW
    {  0, 0,  CNVRSNTYPE::NONE }, // SBRA
    {  0, 0,  CNVRSNTYPE::NONE }, // SKET
    {  3, 0,  CNVRSNTYPE::NONE }, // LP
    {  3, 0,  CNVRSNTYPE::NONE }, // RP
    {  0, 0,  CNVRSNTYPE::NONE }, // DOT
    { 10, 2, CNVRSNTYPE::ARITH }, // AND
    { 15, 2, CNVRSNTYPE::ARITH }, // STAR
    { 14, 2, CNVRSNTYPE::ARITH }, // PLUS
    { 14, 2, CNVRSNTYPE::ARITH }, // MINUS
    { 16, 1, CNVRSNTYPE::UNARY }, // TILDE
    { 16, 1, CNVRSNTYPE::UNARY }, // NOT
    { 15, 2, CNVRSNTYPE::ARITH }, // SLASH
    { 15, 2, CNVRSNTYPE::ARITH }, // PCT
    { 12, 2, CNVRSNTYPE::RELAT }, // LT
    { 12, 2, CNVRSNTYPE::RELAT }, // GT
    {  9, 2, CNVRSNTYPE::ARITH }, // CIRC
    {  8, 2, CNVRSNTYPE::ARITH }, // OR
    {  5, 2,  CNVRSNTYPE::SPCL }, // QUEST
    {  5, 2,  CNVRSNTYPE::SPCL }, // COLON
    {  0, 0,  CNVRSNTYPE::NONE }, // ASGN
    {  4, 2,  CNVRSNTYPE::NONE }, // COMMA
    {  0, 0,  CNVRSNTYPE::NONE }, // SHARP
    {  0, 0,  CNVRSNTYPE::NONE }, // SEMIC
    {  0, 0,  CNVRSNTYPE::NONE }, // CBRA
    {  0, 0,  CNVRSNTYPE::NONE }, // CKET
    {  0, 0,  CNVRSNTYPE::NONE }, // ASPLUS
    {  0, 0,  CNVRSNTYPE::NONE }, // ASMINUS
    {  0, 0,  CNVRSNTYPE::NONE }, // ASSTAR
    {  0, 0,  CNVRSNTYPE::NONE }, // ASSLASH
    {  0, 0,  CNVRSNTYPE::NONE }, // ASPCT
    {  0, 0,  CNVRSNTYPE::NONE }, // ASCIRC
    {  0, 0,  CNVRSNTYPE::NONE }, // ASLSH
    {  0, 0,  CNVRSNTYPE::NONE }, // ASRSH
    {  0, 0,  CNVRSNTYPE::NONE }, // ASOR
    {  0, 0,  CNVRSNTYPE::NONE }, // ASAND
    {  0, 0,  CNVRSNTYPE::NONE }, // ELLIPS
    {  0, 0,  CNVRSNTYPE::NONE }, // DSHARP1
    {  0, 0,  CNVRSNTYPE::NONE }, // NAME1
    { 16, 1, CNVRSNTYPE::UNARY }, // DEFINED
    { 16, 0, CNVRSNTYPE::UNARY }, // UMINUS
};

// forward declarations
int   evalop(struct priority);
value tokval(token*);

value   vals[NSTAK + 1], *vp;
TKNTYPE ops[NSTAK + 1], *op;

// Evaluates an #if #elif #ifdef #ifndef line.  trp->tp points to the keyword.
long eval(_In_ token_row* trp, _In_ const KWTYPE& keyword) noexcept {
    token* tp {};
    nlist* np {};
    int    ntok {}, rand {};

    trp->tp++;
    if (keyword == KWTYPE::KIFDEF || keyword == KWTYPE::KIFNDEF) {
        if (trp->lp - trp->bp != 4 || trp->tp->type != TKNTYPE::NAME) {
            error(ERROR, "Syntax error in #ifdef/#ifndef");
            return 0;
        }
        np = lookup(trp->tp, 0);
        return (keyword == KWTYPE::KIFDEF) == (np && np->flag & (KWPROPS::DEFINED_VALUE | KWPROPS::BUILTIN));
    }
    ntok           = trp->tp - trp->bp;
    kwdefined->val = KWTYPE::KDEFINED; // activate special meaning of defined
    expandrow(trp, "<if>", NOT_IN_MACRO);
    kwdefined->val = NAME;
    vp             = vals;
    op             = ops;
    *op++          = END;
    for (rand = 0, tp = trp->bp + ntok; tp < trp->lp; tp++) {
        if (op >= ops + NSTAK) sysfatal("cpp: can't evaluate #if: increase NSTAK");
        switch (tp->type) {
            case TKNTYPE::WS :
            case TKNTYPE::NL : continue;

            // nilary
            case TKNTYPE::NAME :
            case TKNTYPE::NAME1 :
            case TKNTYPE::NUMBER :
            case TKNTYPE::CCON :
            case TKNTYPE::STRING :
                if (rand) goto syntax;
                *vp++ = tokval(tp);
                rand  = 1;
                continue;

            // unary
            case TKNTYPE::DEFINED :
            case TKNTYPE::TILDE :
            case TKNTYPE::NOT :
                if (rand) goto syntax;
                *op++ = tp->type;
                continue;

            // unary-binary
            case TKNTYPE::PLUS :
            case TKNTYPE::MINUS :
            case TKNTYPE::STAR :
            case TKNTYPE::AND :
                if (rand == 0) {
                    if (tp->type == MINUS) *op++ = UMINUS;
                    if (tp->type == STAR || tp->type == AND) {
                        error(ERROR, "Illegal operator * or & in #if/#elif");
                        return 0;
                    }
                    continue;
                }
                [[fallthrough]];

            // plain binary
            case EQ :
            case NEQ :
            case LEQ :
            case GEQ :
            case LSH :
            case RSH :
            case LAND :
            case LOR :
            case SLASH :
            case PCT :
            case LT :
            case GT :
            case CIRC :
            case OR :
            case QUEST :
            case COLON :
            case COMMA :
                if (rand == 0) goto syntax;
                if (evalop(operator_priority[tp->type]) != 0) return 0;
                *op++ = tp->type;
                rand  = 0;
                continue;

            case LP :
                if (rand) goto syntax;
                *op++ = LP;
                continue;

            case RP :
                if (!rand) goto syntax;
                if (evalop(operator_priority[RP]) != 0) return 0;
                if (op <= ops || op[-1] != LP) goto syntax;
                op--;
                continue;

            default : error(ERROR, "Bad operator (%t) in #if/#elif", tp); return 0;
        }
    }
    if (rand == 0) goto syntax;
    if (evalop(operator_priority[END]) != 0) return 0;
    if (op != &ops[1] || vp != &vals[1]) {
        error(ERROR, "Botch in #if/#elif");
        return 0;
    }
    if (vals[0].type == UND) error(ERROR, "Undefined expression value");
    return vals[0].val;
syntax:
    error(ERROR, "Syntax error in #if/#elif");
    return 0;
}

int evalop(struct priority pri) noexcept {
    struct value v1, v2;
    long         rv1, rv2;
    int          rtype, oper;

    rv2   = 0;
    rtype = 0;
    while (pri.pri < operator_priority[op[-1]].pri) {
        oper = *--op;
        if (operator_priority[oper].arity == 2) {
            v2  = *--vp;
            rv2 = v2.val;
        }
        v1  = *--vp;
        rv1 = v1.val;
        switch (operator_priority[oper].ctype) {
            case 0 :
            default : error(WARNING, "Syntax error in #if/#endif"); return 1;
            case ARITH :
            case RELAT :
                if (v1.type == UNS || v2.type == UNS)
                    rtype = UNS;
                else
                    rtype = SGN;
                if (v1.type == UND || v2.type == UND) rtype = UND;
                if (operator_priority[oper].ctype == RELAT && rtype == UNS) {
                    oper  |= UNSMARK;
                    rtype  = SGN;
                }
                break;
            case SHIFT :
                if (v1.type == UND || v2.type == UND)
                    rtype = UND;
                else
                    rtype = v1.type;
                if (rtype == UNS) oper |= UNSMARK;
                break;
            case UNARY : rtype = v1.type; break;
            case LOGIC :
            case SPCL  : break;
        }
        switch (oper) {
            case EQ :
            case EQ | UNSMARK  : rv1 = rv1 == rv2; break;
            case NEQ           :
            case NEQ | UNSMARK : rv1 = rv1 != rv2; break;
            case LEQ           : rv1 = rv1 <= rv2; break;
            case GEQ           : rv1 = rv1 >= rv2; break;
            case LT            : rv1 = rv1 < rv2; break;
            case GT            : rv1 = rv1 > rv2; break;
            case LEQ | UNSMARK : rv1 = (unsigned long) rv1 <= rv2; break;
            case GEQ | UNSMARK : rv1 = (unsigned long) rv1 >= rv2; break;
            case LT | UNSMARK  : rv1 = (unsigned long) rv1 < rv2; break;
            case GT | UNSMARK  : rv1 = (unsigned long) rv1 > rv2; break;
            case LSH           : rv1 <<= rv2; break;
            case LSH | UNSMARK : rv1 = (unsigned long) rv1 << rv2; break;
            case RSH           : rv1 >>= rv2; break;
            case RSH | UNSMARK : rv1 = (unsigned long) rv1 >> rv2; break;
            case LAND :
                rtype = UND;
                if (v1.type == UND) break;
                if (rv1 != 0) {
                    if (v2.type == UND) break;
                    rv1 = rv2 != 0;
                } else
                    rv1 = 0;
                rtype = SGN;
                break;
            case LOR :
                rtype = UND;
                if (v1.type == UND) break;
                if (rv1 == 0) {
                    if (v2.type == UND) break;
                    rv1 = rv2 != 0;
                } else
                    rv1 = 1;
                rtype = SGN;
                break;
            case AND   : rv1 &= rv2; break;
            case STAR  : rv1 *= rv2; break;
            case PLUS  : rv1 += rv2; break;
            case MINUS : rv1 -= rv2; break;
            case UMINUS :
                if (v1.type == UND) rtype = UND;
                rv1 = -rv1;
                break;
            case OR    : rv1 |= rv2; break;
            case CIRC  : rv1 ^= rv2; break;
            case TILDE : rv1 = ~rv1; break;
            case NOT :
                rv1 = !rv1;
                if (rtype != UND) rtype = SGN;
                break;
            case SLASH :
                if (rv2 == 0) {
                    rtype = UND;
                    break;
                }
                if (rtype == UNS)
                    rv1 /= (unsigned long) rv2;
                else
                    rv1 /= rv2;
                break;
            case PCT :
                if (rv2 == 0) {
                    rtype = UND;
                    break;
                }
                if (rtype == UNS)
                    rv1 %= (unsigned long) rv2;
                else
                    rv1 %= rv2;
                break;
            case COLON :
                if (op[-1] != QUEST)
                    error(ERROR, "Bad ?: in #if/endif");
                else {
                    op--;
                    if ((--vp)->val == 0) v1 = v2;
                    rtype = v1.type;
                    rv1   = v1.val;
                }
                break;
            case DEFINED : break;
            default      : error(ERROR, "Eval botch (unknown operator)"); return 1;
        }
        v1.val  = rv1;
        v1.type = rtype;
        *vp++   = v1;
    }
    return 0;
}

struct value tokval(token* tp) {
    struct value   v;
    nlist*         np;
    int            i, base, c, longcc;
    unsigned long  n;
    Rune           r;
    unsigned char* p;

    v.type = SGN;
    v.val  = 0;
    switch (tp->type) {
        case NAME : v.val = 0; break;

        case NAME1 :
            if ((np = lookup(tp, 0)) && np->flag & (DEFINED_VALUE | BUILTIN)) v.val = 1;
            break;

        case NUMBER :
            n          = 0;
            base       = 10;
            p          = tp->t;
            c          = p[tp->len];
            p[tp->len] = '\0';
            if (*p == '0') {
                base = 8;
                if (p[1] == 'x' || p[1] == 'X') {
                    base = 16;
                    p++;
                }
                p++;
            }
            for (;; p++) {
                if ((i = digit(*p)) < 0) break;
                if (i >= base) error(WARNING, "Bad digit in number %t", tp);
                n *= base;
                n += i;
            }
            if (n >= 0x80000000 && base != 10) v.type = UNS;
            for (; *p; p++) {
                if (*p == 'u' || *p == 'U')
                    v.type = UNS;
                else if (*p == 'l' || *p == 'L') {
                } else {
                    error(ERROR, "Bad number %t in #if/#elif", tp);
                    break;
                }
            }
            v.val          = n;
            tp->t[tp->len] = c;
            break;

        case CCON :
            n      = 0;
            p      = tp->t;
            longcc = 0;
            if (*p == 'L') {
                p      += 1;
                longcc  = 1;
            }
            p += 1;
            if (*p == '\\') {
                p += 1;
                if ((i = digit(*p)) >= 0 && i <= 7) {
                    n  = i;
                    p += 1;
                    if ((i = digit(*p)) >= 0 && i <= 7) {
                        p  += 1;
                        n <<= 3;
                        n  += i;
                        if ((i = digit(*p)) >= 0 && i <= 7) {
                            p  += 1;
                            n <<= 3;
                            n  += i;
                        }
                    }
                } else if (*p == 'x') {
                    p += 1;
                    while ((i = digit(*p)) >= 0 && i <= 15) {
                        p  += 1;
                        n <<= 4;
                        n  += i;
                    }
                } else {
                    static char cvcon[] = "a\ab\bf\fn\nr\rt\tv\v''\"\"??\\\\";
                    for (i = 0; i < sizeof(cvcon); i += 2) {
                        if (*p == cvcon[i]) {
                            n = cvcon[i + 1];
                            break;
                        }
                    }
                    p += 1;
                    if (i >= sizeof(cvcon)) error(WARNING, "Undefined escape in character constant");
                }
            } else if (*p == '\'')
                error(ERROR, "Empty character constant");
            else {
                i  = chartorune(&r, (char*) p);
                n  = r;
                p += i;
                if (i > 1 && longcc == 0) error(WARNING, "Undefined character constant");
            }
            if (*p != '\'')
                error(WARNING, "Multibyte character constant undefined");
            else if (n > 127 && longcc == 0)
                error(WARNING, "Character constant taken as not signed");
            v.val = n;
            break;

        case STRING : error(ERROR, "String in #if/#elif"); break;
    }
    return v;
}

int digit(int i) {
    if ('0' <= i && i <= '9')
        i -= '0';
    else if ('a' <= i && i <= 'f')
        i -= 'a' - 10;
    else if ('A' <= i && i <= 'F')
        i -= 'A' - 10;
    else
        i = -1;
    return i;
}
