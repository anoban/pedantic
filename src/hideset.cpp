#include <cstdio>

#include <prep.hpp>

// a hideset is a null-terminated array of nlist pointers.
// they are referred to by indexing into the hidesets array, hideset 0 is empty.

static constexpr size_t HIDESET_SIZE { 0x20 }; // 32

using Hideset = nlist**; // typedef nlist** Hideset;

Hideset* hidesets;

long long nhidesets   = 0;
long long maxhidesets = 3;

int insert_hideset(Hideset, Hideset, nlist*);

// test for membership in a hideset
bool check_hideset(int hs, nlist* np) noexcept {
    Hideset hsp;

    if (hs >= nhidesets) {
        // issue a diagnostic
        std::abort();
    }
    for (hsp = hidesets[hs]; *hsp; hsp++)
        if (*hsp == np) return true;
    return false;
}

// Return the (possibly new) hideset obtained by adding np to hs.
int new_hideset(int hs, nlist* np) noexcept {
    int     i, len;
    nlist*  nhs[HIDESET_SIZE + 3];
    Hideset hs1, hs2;

    len = insert_hideset(nhs, hidesets[hs], np);
    for (i = 0; i < nhidesets; i++) {
        for (hs1 = nhs, hs2 = hidesets[i]; *hs1 == *hs2; hs1++, hs2++)
            if (*hs1 == nullptr) return i;
    }
    if (len >= HIDESET_SIZE) return hs;
    if (nhidesets >= maxhidesets) {
        maxhidesets = 3 * maxhidesets / 2 + 1;
        hidesets    = (Hideset*) _checked_realloc(hidesets, (sizeof(Hideset*)) * maxhidesets);
    }
    hs1 = (Hideset) _checked_malloc<Hideset>(len); //(len * sizeof(Hideset));
    memmove(hs1, nhs, len * sizeof(Hideset));
    hidesets[nhidesets] = hs1;
    return nhidesets++;
}

int insert_hideset(Hideset dhs, Hideset shs, nlist* np) {
    Hideset odhs = dhs;

    while (*shs && *shs < np) *dhs++ = *shs++;
    if (*shs != np) *dhs++ = np;
    do { *dhs++ = *shs; } while (*shs++);
    return dhs - odhs;
}

// Hideset union
int unionhideset(int hs1, int hs2) noexcept {
    Hideset hp;

    for (hp = hidesets[hs2]; *hp; hp++) hs1 = new_hideset(hs1, *hp);
    return hs1;
}

void init_hideset() noexcept {
    hidesets     = (Hideset*) _checked_malloc<Hideset*>(maxhidesets); // (maxhidesets * sizeof(Hideset*));
    hidesets[0]  = (Hideset) _checked_malloc<Hideset>(1);             // (sizeof(Hideset));
    *hidesets[0] = nullptr;
    nhidesets++;
}

void print_hideset(int hs) noexcept {
    Hideset np;

    for (np = hidesets[hs]; *np; np++) {
        fprintf(stderr, (char*) (*np)->name, (*np)->len);
        fprintf(stderr, " ", 1);
    }
}
