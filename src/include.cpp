#include <prep.hpp>

include_list includelist[MAX_INCLUDE_DIRS] {};

char* objname;

void doinclude(token_row* trp) {
    char          fname[256], iname[256], *p;
    include_list* ip;
    int           angled, len, fd, i;

    trp->tp += 1;
    if (trp->tp >= trp->lp) goto syntax;
    if (trp->tp->type != STRING && trp->tp->type != LT) {
        len = trp->tp - trp->bp;
        expandrow(trp, "<include>", NOT_IN_MACRO);
        trp->tp = trp->bp + len;
    }
    if (trp->tp->type == STRING) {
        len = trp->tp->len - 2;
        if (len > sizeof(fname) - 1) len = sizeof(fname) - 1;
        strncpy(fname, (char*) trp->tp->t + 1, len);
        angled = 0;
    } else if (trp->tp->type == LT) {
        len = 0;
        trp->tp++;
        while (trp->tp->type != GT) {
            if (trp->tp > trp->lp || len + trp->tp->len + 2 >= sizeof(fname)) goto syntax;
            strncpy(fname + len, (char*) trp->tp->t, trp->tp->len);
            len += trp->tp->len;
            trp->tp++;
        }
        angled = 1;
    } else
        goto syntax;
    trp->tp += 2;
    if (trp->tp < trp->lp || len == 0) goto syntax;
    fname[len] = '\0';
    if (fname[0] == '/') {
        fd = open(fname, 0);
        strcpy(iname, fname);
    } else
        for (fd = -1, i = MAX_INCLUDE_DIRS - 1; i >= 0; i--) {
            ip = &includelist[i];
            if (ip->file == nullptr || ip->deleted || (angled && ip->always == 0)) continue;
            if (strlen(fname) + strlen(ip->file) + 2 > sizeof(iname)) continue;
            strcpy(iname, ip->file);
            strcat(iname, "/");
            strcat(iname, fname);
            if ((fd = open(iname, 0)) >= 0) break;
        }
    if (fd < 0) {
        strcpy(iname, cursource->filename);
        p = strrchr(iname, '/');
        if (p != nullptr) {
            *p = '\0';
            strcat(iname, "/");
            strcat(iname, fname);
            fd = open(iname, 0);
        }
    }
    if (Mflag > 1 || !angled && Mflag == 1) {
        write(1, objname, strlen(objname));
        write(1, iname, strlen(iname));
        write(1, "\n", 1);
    }
    if (fd >= 0) {
        if (++incdepth > 20) error(FATAL, "#include too deeply nested");
        setsource((char*) newstring((unsigned char*) iname, strlen(iname), 0), fd, nullptr);
        genline();
    } else {
        trp->tp = trp->bp + 2;
        error(ERROR, "Could not find include file %r", trp);
    }
    return;
syntax:
    error(ERROR, "Syntax error in #include");
    return;
}

/*
 * Generate a line directive for cursource
 */
void genline(void) {
    static token     ta = { UNCLASS, nullptr, 0, 0 };
    static token_row tr = { &ta, &ta, &ta + 1, 1 };
    unsigned char*   p;

    if (nolineinfo) return;

    ta.t = p = (unsigned char*) outp;
    strcpy((char*) p, "#line ");
    p    += sizeof("#line ") - 1;
    p     = (unsigned char*) outnum((char*) p, cursource->line);
    *p++  = ' ';
    *p++  = '"';
    if (cursource->filename[0] != '/' && wd[0]) {
        strcpy((char*) p, wd);
        p    += strlen(wd);
        *p++  = '/';
    }
    strcpy((char*) p, cursource->filename);
    p      += strlen((char*) p);
    *p++    = '"';
    *p++    = '\n';
    ta.len  = (char*) p - outp;
    outp    = (char*) p;
    tr.tp   = tr.bp;
    puttokens(&tr);
}

void setobjname(char* f) {
    int n   = strlen(f);
    objname = (char*) _checked_malloc(n + 5);
    strcpy(objname, f);
    if (objname[n - 2] == '.')
        strcpy(objname + n - 1, "$O: ");
    else
        strcpy(objname + n, "$O: ");
}
