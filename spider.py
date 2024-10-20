# a python script to download all the original Plan9's cpp sources

from urllib.request import urlopen

FILES: tuple[str, ...] = (
    "cpp.h",
    "eval.c",
    "hideset.c",
    "include.c",
    "lex.c",
    "macro.c",
    # "mkfile", we don't need this
    "nlist.c",
    # "test.c", we don't need this either
    "tokens.c",
)

PLAN9_CPP_DIR_URL: str = r"https://9p.io/sources/plan9/sys/src/cmd/cpp/"


def main() -> None:
    """
    #
    """
    pass


if __name__ == "__main__":
    main()
