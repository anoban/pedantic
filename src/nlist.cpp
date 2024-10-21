#include <prep.hpp>

extern int   getopt(int, char**, char*);
extern char* optarg;
extern int   optind;
int          verbose;
int          Mflag;
int          Cplusplus;
int          nolineinfo;
nlist*       kwdefined;
char         wd[128];

static constexpr size_t NLIST_SIZE { 128 };
nlist*                  nlist[NLIST_SIZE];

struct keyword final {
        const char* keyword;
        KWTYPE      val;
        KWPROPS     flag;
};

static constexpr keyword keyword_table[] = {
    {       "if",      KWTYPE::KIF,              KWPROPS::KEYWORD },
    {    "ifdef",   KWTYPE::KIFDEF,              KWPROPS::KEYWORD },
    {   "ifndef",  KWTYPE::KIFNDEF,              KWPROPS::KEYWORD },
    {     "elif",    KWTYPE::KELIF,              KWPROPS::KEYWORD },
    {     "else",    KWTYPE::KELSE,              KWPROPS::KEYWORD },
    {    "endif",   KWTYPE::KENDIF,              KWPROPS::KEYWORD },
    {  "include", KWTYPE::KINCLUDE,              KWPROPS::KEYWORD },
    {   "define",  KWTYPE::KDEFINE,              KWPROPS::KEYWORD },
    {    "undef",   KWTYPE::KUNDEF,              KWPROPS::KEYWORD },
    {     "line",    KWTYPE::KLINE,              KWPROPS::KEYWORD },
    {    "error",   KWTYPE::KERROR,              KWPROPS::KEYWORD },
    {  "warning", KWTYPE::KWARNING,              KWPROPS::KEYWORD }, // extension to ANSI
    {   "pragma",  KWTYPE::KPRAGMA,              KWPROPS::KEYWORD },
    {     "eval",    KWTYPE::KEVAL,              KWPROPS::KEYWORD },
    {  "defined", KWTYPE::KDEFINED, KWPROPS::DEFINED_UNCHANGEABLE },
    { "__LINE__",  KWTYPE::KLINENO, KWPROPS::BUILTIN_UNCHANGEABLE },
    { "__FILE__",    KWTYPE::KFILE, KWPROPS::BUILTIN_UNCHANGEABLE },
    { "__DATE__",    KWTYPE::KDATE, KWPROPS::BUILTIN_UNCHANGEABLE },
    { "__TIME__",    KWTYPE::KTIME, KWPROPS::BUILTIN_UNCHANGEABLE },
    { "__STDC__",    KWTYPE::KSTDC,         KWPROPS::UNCHANGEABLE },
    nullptr
};

unsigned long namebit[0x40];
nlist*        np;

void setup(int argc, char** argv) noexcept {
    const keyword* kp;
    nlist*         np;
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
        includelist[1].file = nullptr;
        error(WARNING, "Unknown $objtype");
    }
    if (getwd(wd, sizeof(wd)) == 0) wd[0] = '\0';
    includelist[0].file   = "/sys/include";
    includelist[0].always = 1;
    firstinclude          = MAX_INCLUDE_DIRS - 2;
    if ((includeenv = getenv("include")) != nullptr) {
        char* cp;
        includeenv = strdup(includeenv);
        for (; firstinclude > 0; firstinclude--) {
            cp = strtok(includeenv, " ");
            if (cp == nullptr) break;
            includelist[firstinclude].file   = cp;
            includelist[firstinclude].always = 1;
            includeenv                       = nullptr;
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
                if (includelist[i].file == nullptr) {
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
        if ((fp = strrchr(argv[0], '/')) != nullptr) {
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
    setsource(fp, fd, nullptr);
    if (debuginclude) {
        for (i = 0; i < MAX_INCLUDE_DIRS; i++)
            if (includelist[i].file && includelist[i].deleted == 0) error(WARNING, "Include: %s", includelist[i].file);
    }
}

nlist* lookup(token* tp, int install) noexcept {
    unsigned int   h;
    nlist*         np;
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
        np       = _new_obj<nlist>();
        np->val  = 0;
        np->vp   = nullptr;
        np->ap   = nullptr;
        np->flag = 0;
        np->len  = tp->len;
        np->name = newstring(tp->t, tp->len, 0);
        np->next = nlist[h];
        nlist[h] = np;
        quickset(tp->t[0], tp->len > 1 ? tp->t[1] : 0);
        return np;
    }
    return nullptr;
}
