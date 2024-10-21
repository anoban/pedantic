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

#ifdef EOF // defined by <cstdio>
    #undef EOF
static constexpr long long EOF { -1 }; // end of file
#endif

// token kinds recognized as valid inside a macro definition
enum TKNTYPE : unsigned { // NOLINT(performance-enum-size)
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
enum KWTYPE : unsigned { // NOLINT(performance-enum-size)
    KIF,                 // #if
    KIFDEF,              // #ifdef
    KIFNDEF,             // #ifndef
    KELIF,               // #elif
    KELSE,               // #else
    KENDIF,              // #endif
    KINCLUDE,            // #include
    KDEFINE,             // #define
    KUNDEF,              // #undef
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
enum KWPROPS : unsigned {                                     // NOLINT(performance-enum-size)
    DEFINED_VALUE                                     = 0x01, // a macro with a #defined value
    KEYWORD                                           = 0x02, // a preprocessor keyword
    UNCHANGEABLE                                      = 0x04, // a macro that can't be #defined (redefined) by users, e.g builtin macros
    BUILTIN                                           = 0x08, // a builtin macro, e.g. __LINE__
    VARIADIC_MACRO                                    = 0x10, // variadic macro
    BUILTIN_UNCHANGEABLE /* BUILTIN + UNCHANGEABLE */ = 0x0C, // a builtin unchangeable macro
    DEFINED_UNCHANGEABLE /* DEFINED_VALUE + UNCHANGEABLE */ = 0x05 // a builtin unchangeable macro
};

static constexpr size_t EOB { 0xFE };  // sentinel for end of input buffer
static constexpr size_t EOFC { 0xFD }; // sentinel for end of input file
static constexpr size_t XPWS { 0x01 }; // token flag: white space to assure token separator

enum { NOT_IN_MACRO, IN_MACRO };

struct token final {
        TKNTYPE        type;
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
        long long max; // number of allocated tokens in the token row
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

struct nlist {
        nlist*         next;
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

template<typename _Ty> [[nodiscard]] static inline _Ty* _new_obj() noexcept { return _checked_malloc(sizeof(_Ty)); }

#define quicklook(a, b) (namebit[(a) & 077] & (1 << ((b) & 037)))
#define quickset(a, b)  namebit[(a) & 077] |= (1 << ((b) & 037))

extern unsigned long namebit[077 + 1]; //64

enum class ERRKIND : unsigned char { WARNING, ERROR, FATAL };

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
nlist*         lookup(token*, int);
void           control(token_row*);
void           dodefine(token_row*);
void           doadefine(token_row*, int);
void           doinclude(token_row*);
void           doif(token_row*, enum KWTYPE);
void           expand(token_row*, nlist*, int);
void           builtin(token_row*, int);
int            gatherargs(token_row*, token_row**, int, int*);
void           substargs(nlist*, token_row*, token_row**);
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
int            lookuparg(nlist*, token*);
long           eval(token_row*, int);
void           genline(void);
void           setempty(token_row*);
void           makespace(token_row*);
char*          outnum(char*, int);
int            digit(int);
unsigned char* newstring(unsigned char*, int, int);
int            check_hideset(int, nlist*);
void           print_hideset(int);
int            new_hideset(int, nlist*);
int            unionhideset(int, int);
void           init_hideset(void);
void           setobjname(char*);
void           clearwstab(void);

#pragma endregion

// #define rowlen(tokrow) ((tokrow)->lp - (tokrow)->bp)
[[nodiscard]] static inline ptrdiff_t tokenrow_len(_In_ const token_row* const tknrow) noexcept { return tknrow->lp - tknrow->bp; }

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
extern nlist*       kwdefined;
extern include_list includelist[MAX_INCLUDE_DIRS];
extern char         wd[];

[[nodiscard]] static inline void* __cdecl _checked_realloc(_In_ void* const ptr, _In_ const size_t size) noexcept {
    void* _ptr = ::realloc(ptr, size);
    if (!_ptr) {
        ::fputws(L"memory reallocation failed inside " __FUNCTIONW__ "\n", stderr);
        std::exit(_UCRT_ALLOC_ERROR);
    }
    return _ptr;
}

// returns a zeroed out buffer of size bytes
template<typename _Ty> [[nodiscard]] static inline _Ty* __cdecl _checked_malloc(_In_ const size_t count) noexcept {
    _Ty* ptr = reinterpret_cast<_Ty*>(::malloc(sizeof(_Ty) * count));
    if (!ptr) {
        ::fputws(L"memory allocation failed inside " __FUNCTIONW__ "\n", stderr);
        std::exit(_UCRT_ALLOC_ERROR);
    }
    ::memset(ptr, 0U, sizeof(_Ty) * count); // do we really need this??
    return ptr;
}
