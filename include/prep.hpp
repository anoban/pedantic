#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr size_t INPUT_BUFFER_SIZE { 32768 };
static constexpr size_t OUTPUT_BUFFER_SIZE { 4096 };
static constexpr size_t MAX_MACRO_ARGS { 128 };     // max number arguments to a function like macro
static constexpr size_t MAX_INCLUDE_DIRS { 64 };    // max number of include directories (-I)
static constexpr size_t MAX_NESTED_IF_DEPTH { 32 }; // maximum allowed depth for nesting #if preprocessor directives

static constexpr int _UCRT_ALLOC_ERROR { 0xEE }; // an error code to indicate that a UCRT memory allocation routine failed

#ifndef EOF // <cstdio>
    #define EOF (-1)
#endif

#ifndef NULL // <vcruntime.h>
    #define NULL 0
#endif

// token kinds recognized as valid inside a macro definition
enum class token_type : unsigned { // NOLINT(performance-enum-size)
    END,
    UNCLASS,
    NAME,
    NUMBER,
    STRING,
    CCON,
    NL,     // newline
    WS,     // whitespace
    DSHARP, // ##
    EQ,     // =
    NEQ,    // !=
    LEQ,    // <=
    GEQ,    // >=
    LSH,    // <<
    RSH,    // >>
    LAND,   // &&
    LOR,    // ||
    PPLUS,
    MMINUS,
    ARROW,   // ->
    SBRA,    // [
    SKET,    // ]
    LP,      // (
    RP,      // )
    DOT,     // .
    AND,     // &
    STAR,    // *
    PLUS,    // +
    MINUS,   // -
    TILDE,   // ~
    NOT,     // !
    SLASH,   // /
    PCT,     // %
    LT,      // <
    GT,      // >
    CIRC,    // ^
    OR,      // |
    QUEST,   // ?
    COLON,   // :
    ASGN,    // =
    COMMA,   // ,
    SHARP,   // #
    SEMIC,   // ;
    CBRA,    // {
    CKET,    // }
    ASPLUS,  // +=
    ASMINUS, // -=
    ASSTAR,  // *=
    ASSLASH, // /=
    ASPCT,   // %=
    ASCIRC,
    ASLSH,  // <<=
    ASRSH,  // >>=
    ASOR,   // |=
    ASAND,  // &=
    ELLIPS, // ...
    DSHARP1,
    NAME1,
    DEFINED,
    UMINUS // unary -
};

// recognized preprocessor keywords/directives/macros
enum class keyword_type : unsigned { // NOLINT(performance-enum-size)
    KIF,                             // #if
    KIFDEF,                          // #ifdef
    KIFNDEF,                         // #ifndef
    KELIF,                           // #elif
    KELSE,                           // #else
    KENDIF,                          // #endif
    KINCLUDE,                        // #include
    KDEFINE,                         // #define
    KUNDEF,                          // #undef
    KLINE,
    KERROR,   // #error
    KWARNING, // #warning - a directive provided as an extension to ISO standard C
    KPRAGMA,  // #pragma
    KDEFINED, // #defined
    KLINENO,  // __LINE__
    KFILE,    // __FILE__
    KDATE,    // __DATE__
    KTIME,    // __TIME__
    KSTDC,    // __STDC__
    KEVAL
};

// macro keyword properties
enum class keyword_props : unsigned { // NOLINT(performance-enum-size)
    IS_DEFINED_VALUE        = 0x01,   // a macro with a #defined value
    IS_KEYWORD              = 0x02,   // a preprocessor keyword
    IS_UNCHANGEABLE         = 0x04,   // a macro that can't be #defined (redefined) by users, e.g builtin macros
    IS_BUILTIN              = 0x08,   // a builtin macro, e.g. __LINE__
    IS_VARIADIC_MACRO       = 0x10,   // variadic macro
    IS_BUILTIN_UNCHANGEABLE = 0x0C,   // a builtin unchangeable macro
    IS_DEFINED_UNCHANGEABLE = 0x05    // a builtin unchangeable macro
};

static constexpr size_t EOB { 0xFE };  // sentinel for end of input buffer
static constexpr size_t EOFC { 0xFD }; // sentinel for end of input file
static constexpr size_t XPWS { 0x01 }; // token flag: white space to assure token separator

enum { NOT_IN_MACRO, IN_MACRO };

struct token final {
        token_type     type;
        unsigned char  flag;
        unsigned short hideset;
        unsigned int   wslen;
        unsigned int   len;
        char*          t;
};

struct token_row final {
        token*    tp;  // current one to scan
        token*    bp;  // base (allocated value)
        token*    lp;  // last + 1 token used
        long long max; // number allocated
};

struct source final {
        char*          filename; // name of file of the source
        int            line;     // current line number
        int            lineinc;  // adjustment for \\n lines
        unsigned char* inb;      // input buffer
        unsigned char* inp;      // input pointer
        unsigned char* inl;      // end of input
        int            ins;      // input buffer size
        int            fd;       // input source
        int            ifdepth;  // conditional nesting in include
        source*        next;     // stack for #include
};

struct Nlist {
        struct nlist*  next;
        unsigned char* name;
        int            len;
        token_row*     vp;   // value as macro
        token_row*     ap;   // list of argument names, if any
        char           val;  // value as preprocessor name
        char           flag; // is defined, is pp name
};

struct include_list {
        char  deleted;
        char  always;
        char* file;
};

template<typename _Ty> static inline _Ty* _new_obj() noexcept { return _checked_malloc(sizeof(_Ty)); }

#define quicklook(a, b) (namebit[(a) & 077] & (1 << ((b) & 037)))
#define quickset(a, b)  namebit[(a) & 077] |= (1 << ((b) & 037))

extern unsigned long namebit[077 + 1]; //64

enum class error_kind : unsigned char { WARNING, ERROR, FATAL };

#pragma region __FORWARD_DECLARATIONS__

void expandlex(void);
void fixlex(void);
void setup(int, char**);

#define gettokens cpp_gettokens
int     gettokens(token_row*, int);
int     comparetokens(token_row*, token_row*);
source* setsource(char*, int, char*);
void    unsetsource(void);
void    puttokens(token_row*);
void    process(token_row*);

void           flushout(void);
int            fillbuf(source*);
int            trigraph(source*);
int            foldline(source*);
Nlist*         lookup(token*, int);
void           control(token_row*);
void           dodefine(token_row*);
void           doadefine(token_row*, int);
void           doinclude(token_row*);
void           doif(token_row*, enum keyword_type);
void           expand(token_row*, Nlist*, int);
void           builtin(token_row*, int);
int            gatherargs(token_row*, token_row**, int, int*);
void           substargs(Nlist*, token_row*, token_row**);
void           expandrow(token_row*, char*, int);
void           maketokenrow(int, token_row*);
token_row*     copytokenrow(token_row*, token_row*);
token*         growtokenrow(token_row*);
token_row*     normtokenrow(token_row*);
void           adjustrow(token_row*, int);
void           movetokenrow(token_row*, token_row*);
void           insertrow(token_row*, int, token_row*);
void           peektokens(token_row*, char*);
void           doconcat(token_row*);
token_row*     stringify(token_row*);
int            lookuparg(Nlist*, token*);
long           eval(token_row*, int);
void           genline(void);
void           setempty(token_row*);
void           makespace(token_row*);
char*          outnum(char*, int);
int            digit(int);
unsigned char* newstring(unsigned char*, int, int);
int            checkhideset(int, Nlist*);
void           prhideset(int);
int            newhideset(int, Nlist*);
int            unionhideset(int, int);
void           iniths(void);
void           setobjname(char*);
void           clearwstab(void);

#pragma endregion

#define rowlen(tokrow) ((tokrow)->lp - (tokrow)->bp)

extern char*        outp;
extern token        nltoken;
extern source*      cursource;
extern char         current_time[];
extern int          incdepth;
extern int          ifdepth;
extern int          ifsatisfied[MAX_NESTED_IF_DEPTH];
extern int          Mflag;
extern int          nolineinfo;
extern int          skipping;
extern int          verbose;
extern int          Cplusplus;
extern Nlist*       kwdefined;
extern include_list includelist[MAX_INCLUDE_DIRS];
extern char         wd[];

static inline void* __cdecl _checked_realloc(_In_ void* const ptr, _In_ const size_t size) noexcept {
    void* _ptr = ::realloc(ptr, size);
    if (!_ptr) {
        ::fputws(L"memory reallocation failed inside " __FUNCTIONW__ "\n", stderr);
        std::exit(_UCRT_ALLOC_ERROR);
    }
    return _ptr;
}

// returns a zeroed out buffer of size bytes
static inline void* __cdecl _checked_malloc(_In_ const size_t size) noexcept {
    void* ptr = ::malloc(size);
    if (!ptr) {
        ::fputws(L"memory allocation failed inside " __FUNCTIONW__ "\n", stderr);
        std::exit(_UCRT_ALLOC_ERROR);
    }
    ::memset(ptr, 0U, size);
    return ptr;
}

static inline void error(_In_ const error_kind& type, const char* string, ...) noexcept {
    va_list    ap;
    char *     cp, *ep;
    token*     tp;
    token_row* trp;
    source*    s;
    int        i;
    void*      p;

    fprintf(stderr, "prep: ");
    for (s = cursource; s; s = s->next)
        if (*s->filename) fprintf(stderr, "%s:%d ", s->filename, s->line);

    va_start(ap, string);
    for (ep = string; *ep; ep++) {
        if (*ep == '%') {
            switch (*++ep) {
                case 's' :
                    cp = va_arg(ap, char*);
                    fprintf(stderr, "%s", cp);
                    break;
                case 'd' :
                    i = va_arg(ap, int);
                    fprintf(stderr, "%d", i);
                    break;
                case 'p' :
                    p = va_arg(ap, void*);
                    fprintf(stderr, "%p", p);
                    break;
                case 't' :
                    tp = va_arg(ap, token*);
                    fprintf(stderr, "%.*s", tp->len, tp->t);
                    break;

                case 'r' :
                    trp = va_arg(ap, token_row*);
                    for (tp = trp->tp; tp < trp->lp && tp->type != NL; tp++) {
                        if (tp > trp->tp && tp->wslen) fputc(' ', stderr);
                        fprintf(stderr, "%.*s", tp->len, tp->t);
                    }
                    break;

                default : fputc(*ep, stderr); break;
            }
        } else
            fputc(*ep, stderr);
    }
    va_end(ap);
    fputc('\n', stderr);
    if (type == FATAL) exits("error");
    if (type != WARNING) nerrs = 1;
    fflush(stderr);
}
