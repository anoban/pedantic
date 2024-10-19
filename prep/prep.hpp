#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>

static constexpr size_t INPUT_BUFFER_SIZE { 32768 }; // input buffer
static constexpr size_t OUTPUT_BUFFER_SIZE { 4096 }; // output buffer
static constexpr size_t MAX_MACRO_ARGS { 128 };      // max number arguments to a macro
static constexpr size_t MAX_INCLUDE_DIRS { 64 };     // max number of include directories (-I)
static constexpr size_t MAX_NESTED_IF_DEPTH { 32 };  // depth of nesting of #if

#ifndef EOF
    #define EOF (-1)
#endif

#ifndef NULL
    #define NULL 0
#endif

enum class TokenType : unsigned char {
    END,
    UNCLASS,
    NAME,
    NUMBER,
    STRING,
    CCON,
    NL,
    WS,
    DSHARP,
    EQ,
    NEQ,
    LEQ,
    GEQ,
    LSH,
    RSH,
    LAND,
    LOR,
    PPLUS,
    MMINUS,
    ARROW,
    SBRA,
    SKET,
    LP,
    RP,
    DOT,
    AND,
    STAR,
    PLUS,
    MINUS,
    TILDE,
    NOT,
    SLASH,
    PCT,
    LT,
    GT,
    CIRC,
    OR,
    QUEST,
    COLON,
    ASGN,
    COMMA,
    SHARP,
    SEMIC,
    CBRA,
    CKET,
    ASPLUS,
    ASMINUS,
    ASSTAR,
    ASSLASH,
    ASPCT,
    ASCIRC,
    ASLSH,
    ASRSH,
    ASOR,
    ASAND,
    ELLIPS,
    DSHARP1,
    NAME1,
    DEFINED,
    UMINUS
};

enum class KeywordType : unsigned char {
    KIF,      // #if
    KIFDEF,   // #ifdef
    KIFNDEF,  // #ifndef
    KELIF,    // #elif
    KELSE,    // #else
    KENDIF,   // #endif
    KINCLUDE, // #include
    KDEFINE,  // #define
    KUNDEF,   // #undef
    KLINE,    // __LINE__
    KERROR,
    KWARNING,
    KPRAGMA,  // #pragma
    KDEFINED, // #defined
    KLINENO,
    KFILE, // __FILE__
    KDATE, // __DATE__
    KTIME, // __TIME__
    KSTDC, // __STDC__
    KEVAL
};

static constexpr size_t ISDEFINED { 01 };  // has #defined value
static constexpr size_t ISKW { 02 };       // is a preprocessor keyword
static constexpr size_t ISUNCHANGE { 04 }; // can't be #defined in preprocessor
static constexpr size_t ISMAC { 010 };     // builtin macro, e.g. __LINE__
static constexpr size_t ISVARMAC { 020 };  // variadic macro

static constexpr size_t EOB { 0xFE };  // sentinel for end of input buffer
static constexpr size_t EOFC { 0xFD }; // sentinel for end of input file
static constexpr size_t XPWS { 0x01 }; // token flag: white space to assure token sep.

enum { NOT_IN_MACRO, IN_MACRO };

struct Token {
        TokenType      type;
        unsigned char  flag;
        unsigned short hideset;
        unsigned int   wslen;
        unsigned int   len;
        unsigned char* t;
};

struct Tokenrow {
        Token* tp;  // current one to scan
        Token* bp;  // base (allocated value)
        Token* lp;  // last + 1 token used
        int    max; // number allocated
};

struct Source {
        char*          filename; // name of file of the source */
        int            line;     // current line number */
        int            lineinc;  // adjustment for \\n lines */
        unsigned char* inb;      // input buffer */
        unsigned char* inp;      // input pointer */
        unsigned char* inl;      // end of input */
        int            ins;      // input buffer size */
        int            fd;       // input source */
        int            ifdepth;  // conditional nesting in include */
        struct source* next;     // stack for #include */
};

struct Nlist {
        struct nlist*  next;
        unsigned char* name;
        int            len;
        Tokenrow*      vp;   // value as macro */
        Tokenrow*      ap;   // list of argument names, if any */
        char           val;  // value as preprocessor name */
        char           flag; // is defined, is pp name */
};

struct Includelist {
        char  deleted;
        char  always;
        char* file;
};

#define new(t)          (t*) domalloc(sizeof(t))
#define quicklook(a, b) (namebit[(a) & 077] & (1 << ((b) & 037)))
#define quickset(a, b)  namebit[(a) & 077] |= (1 << ((b) & 037))

extern unsigned long namebit[077 + 1];

enum class ErrorKind : unsigned char { WARNING, ERROR, FATAL };

void expandlex(void);
void fixlex(void);
void setup(int, char**);

#define gettokens cpp_gettokens
int gettokens(Tokenrow*, int);

int            comparetokens(Tokenrow*, Tokenrow*);
Source*        setsource(char*, int, char*);
void           unsetsource(void);
void           puttokens(Tokenrow*);
void           process(Tokenrow*);
void*          dorealloc(void*, int);
void*          domalloc(int);
void           dofree(void*);
void           error(enum ErrorKind, char*, ...);
void           flushout(void);
int            fillbuf(Source*);
int            trigraph(Source*);
int            foldline(Source*);
Nlist*         lookup(Token*, int);
void           control(Tokenrow*);
void           dodefine(Tokenrow*);
void           doadefine(Tokenrow*, int);
void           doinclude(Tokenrow*);
void           doif(Tokenrow*, enum KeywordType);
void           expand(Tokenrow*, Nlist*, int);
void           builtin(Tokenrow*, int);
int            gatherargs(Tokenrow*, Tokenrow**, int, int*);
void           substargs(Nlist*, Tokenrow*, Tokenrow**);
void           expandrow(Tokenrow*, char*, int);
void           maketokenrow(int, Tokenrow*);
Tokenrow*      copytokenrow(Tokenrow*, Tokenrow*);
Token*         growtokenrow(Tokenrow*);
Tokenrow*      normtokenrow(Tokenrow*);
void           adjustrow(Tokenrow*, int);
void           movetokenrow(Tokenrow*, Tokenrow*);
void           insertrow(Tokenrow*, int, Tokenrow*);
void           peektokens(Tokenrow*, char*);
void           doconcat(Tokenrow*);
Tokenrow*      stringify(Tokenrow*);
int            lookuparg(Nlist*, Token*);
long           eval(Tokenrow*, int);
void           genline(void);
void           setempty(Tokenrow*);
void           makespace(Tokenrow*);
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

#define rowlen(tokrow) ((tokrow)->lp - (tokrow)->bp)

extern char*       outp;
extern Token       nltoken;
extern Source*     cursource;
extern char*       curtime;
extern int         incdepth;
extern int         ifdepth;
extern int         ifsatisfied[MAX_NESTED_IF_DEPTH];
extern int         Mflag;
extern int         nolineinfo;
extern int         skipping;
extern int         verbose;
extern int         Cplusplus;
extern Nlist*      kwdefined;
extern Includelist includelist[MAX_INCLUDE_DIRS];
extern char        wd[];
