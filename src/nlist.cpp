#include <prep.hpp>

extern int   getopt(int, char**, char*);
extern char* optarg;
extern int   optind;
int          verbose;
int          Mflag;
int          Cplusplus;
int          nolineinfo;
Nlist*       kwdefined;
char         wd[128];

static constexpr size_t NLIST_SIZE { 128 };
Nlist*                  nlist[NLIST_SIZE];

struct keyword final {
        const char*   keyword;
        keyword_type  val;
        keyword_props flag;
};

static constexpr keyword keyword_table[] = {
    {       "if",      keyword_type::KIF,              keyword_props::IS_KEYWORD },
    {    "ifdef",   keyword_type::KIFDEF,              keyword_props::IS_KEYWORD },
    {   "ifndef",  keyword_type::KIFNDEF,              keyword_props::IS_KEYWORD },
    {     "elif",    keyword_type::KELIF,              keyword_props::IS_KEYWORD },
    {     "else",    keyword_type::KELSE,              keyword_props::IS_KEYWORD },
    {    "endif",   keyword_type::KENDIF,              keyword_props::IS_KEYWORD },
    {  "include", keyword_type::KINCLUDE,              keyword_props::IS_KEYWORD },
    {   "define",  keyword_type::KDEFINE,              keyword_props::IS_KEYWORD },
    {    "undef",   keyword_type::KUNDEF,              keyword_props::IS_KEYWORD },
    {     "line",    keyword_type::KLINE,              keyword_props::IS_KEYWORD },
    {    "error",   keyword_type::KERROR,              keyword_props::IS_KEYWORD },
    {  "warning", keyword_type::KWARNING,              keyword_props::IS_KEYWORD }, // extension to ANSI
    {   "pragma",  keyword_type::KPRAGMA,              keyword_props::IS_KEYWORD },
    {     "eval",    keyword_type::KEVAL,              keyword_props::IS_KEYWORD },
    {  "defined", keyword_type::KDEFINED, keyword_props::IS_DEFINED_UNCHANGEABLE },
    { "__LINE__",  keyword_type::KLINENO, keyword_props::IS_BUILTIN_UNCHANGEABLE },
    { "__FILE__",    keyword_type::KFILE, keyword_props::IS_BUILTIN_UNCHANGEABLE },
    { "__DATE__",    keyword_type::KDATE, keyword_props::IS_BUILTIN_UNCHANGEABLE },
    { "__TIME__",    keyword_type::KTIME, keyword_props::IS_BUILTIN_UNCHANGEABLE },
    { "__STDC__",    keyword_type::KSTDC,         keyword_props::IS_UNCHANGEABLE },
    nullptr
};

unsigned long namebit[077 + 1];
Nlist*        np;

void setup(int argc, char** argv) noexcept {
    const keyword* kp;
    Nlist*         np;
    token          t;
    int            fd, i;
    char *         fp, *dp;
    token_row      tr;
    char*          objtype;
    char*          includeenv;
    int            firstinclude;
    static char    nbuf[40];
    static token   deftoken[1] = {
        { NAME, 0, 0, 0, 7, (unsigned char*) "defined" }
    };
    static token_row deftr        = { deftoken, deftoken, deftoken + 1, 1 };
    int              debuginclude = 0;
    int              nodot        = 0;
    char             xx[2]        = { 0, 0 };

    for (kp = keyword_table; kp->keyword; kp++) {
        t.t      = (unsigned char*) kp->keyword;
        t.len    = strlen(kp->keyword);
        np       = lookup(&t, 1);
        np->flag = kp->flag;
        np->val  = kp->val;
        if (np->val == KDEFINED) {
            kwdefined = np;
            np->val   = NAME;
            np->vp    = &deftr;
            np->ap    = 0;
        }
    }
    /*
	 * For Plan 9, search /objtype/include, then /sys/include
	 * (Note that includelist is searched from high end to low)
	 */
    if ((objtype = getenv("objtype"))) {
        snprintf(nbuf, sizeof nbuf, "/%s/include", objtype);
        includelist[1].file   = nbuf;
        includelist[1].always = 1;
    } else {
        includelist[1].file = NULL;
        error(WARNING, "Unknown $objtype");
    }
    if (getwd(wd, sizeof(wd)) == 0) wd[0] = '\0';
    includelist[0].file   = "/sys/include";
    includelist[0].always = 1;
    firstinclude          = MAX_INCLUDE_DIRS - 2;
    if ((includeenv = getenv("include")) != NULL) {
        char* cp;
        includeenv = strdup(includeenv);
        for (; firstinclude > 0; firstinclude--) {
            cp = strtok(includeenv, " ");
            if (cp == NULL) break;
            includelist[firstinclude].file   = cp;
            includelist[firstinclude].always = 1;
            includeenv                       = NULL;
        }
    }
    setsource("", -1, 0);
    ARGBEGIN {
        case 'N' :
            for (i = 0; i < MAX_INCLUDE_DIRS; i++)
                if (includelist[i].always == 1) includelist[i].deleted = 1;
            break;
        case 'I' :
            for (i = firstinclude; i >= 0; i--) {
                if (includelist[i].file == NULL) {
                    includelist[i].always = 1;
                    includelist[i].file   = ARGF();
                    break;
                }
            }
            if (i < 0) error(WARNING, "Too many -I directives");
            break;
        case 'D' :
        case 'U' :
            setsource("<cmdarg>", -1, ARGF());
            maketokenrow(3, &tr);
            gettokens(&tr, 1);
            doadefine(&tr, ARGC());
            unsetsource();
            break;
        case 'M' : Mflag++; break;
        case 'V' : verbose++; break;
        case '+' : Cplusplus++; break;
        case 'i' : debuginclude++; break;
        case 'P' : nolineinfo++; break;
        case '.' : nodot++; break;
        default :
            xx[0] = ARGC();
            error(FATAL, "Unknown argument '%s'", xx);
            break;
    }
    ARGEND
    dp = ".";
    fp = "<stdin>";
    fd = 0;
    if (argc > 2) error(FATAL, "Too many file arguments; see cpp(1)");
    if (argc > 0) {
        if ((fp = strrchr(argv[0], '/')) != NULL) {
            int len = fp - argv[0];
            dp      = (char*) newstring((unsigned char*) argv[0], len + 1, 0);
            dp[len] = '\0';
        }
        fp = (char*) newstring((unsigned char*) argv[0], strlen(argv[0]), 0);
        if ((fd = open(fp, 0)) < 0) error(FATAL, "Can't open input file %s", fp);
    }
    if (argc > 1) {
        int fdo = create(argv[1], 1, 0666);
        if (fdo < 0) error(FATAL, "Can't open output file %s", argv[1]);
        dup(fdo, 1);
    }
    if (Mflag) setobjname(fp);
    includelist[MAX_INCLUDE_DIRS - 1].always = 0;
    includelist[MAX_INCLUDE_DIRS - 1].file   = dp;
    if (nodot) includelist[MAX_INCLUDE_DIRS - 1].deleted = 1;
    setsource(fp, fd, NULL);
    if (debuginclude) {
        for (i = 0; i < MAX_INCLUDE_DIRS; i++)
            if (includelist[i].file && includelist[i].deleted == 0) error(WARNING, "Include: %s", includelist[i].file);
    }
}

Nlist* lookup(token* tp, int install) noexcept {
    unsigned int   h;
    Nlist*         np;
    unsigned char *cp, *cpe;

    h = 0;
    for (cp = tp->t, cpe = cp + tp->len; cp < cpe;) h += *cp++;
    h  %= NLIST_SIZE;
    np  = nlist[h];
    while (np) {
        if (*tp->t == *np->name && tp->len == np->len && strncmp((char*) tp->t, (char*) np->name, tp->len) == 0) return np;
        np = np->next;
    }
    if (install) {
        np       = _new_obj<Nlist>();
        np->val  = 0;
        np->vp   = NULL;
        np->ap   = NULL;
        np->flag = 0;
        np->len  = tp->len;
        np->name = newstring(tp->t, tp->len, 0);
        np->next = nlist[h];
        nlist[h] = np;
        quickset(tp->t[0], tp->len > 1 ? tp->t[1] : 0);
        return np;
    }
    return NULL;
}
