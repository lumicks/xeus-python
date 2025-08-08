import argparse

from . import ci, clang_tidy_cache
from .configure import configure
from .doctor import doctor


def main():
    parser = argparse.ArgumentParser()
    parser.set_defaults(func=parser.print_help)
    subparsers = parser.add_subparsers()

    def add_parser_func(name, func, flags=None):
        new_parser = subparsers.add_parser(name, help=func.__doc__)
        new_parser.set_defaults(func=func)
        for flag in flags or []:
            new_parser.add_argument(flag, action="store_true")
        return new_parser

    add_parser_func("configure", configure, ["--skip-checks"])
    add_parser_func("doctor", doctor)
    add_parser_func("ctcache-info", clang_tidy_cache.info)
    add_parser_func("ctcache-prune", clang_tidy_cache.prune)
    add_parser_func("ctcache-unlock", clang_tidy_cache.unlock)
    add_parser_func("ci-pre", ci.pre)
    add_parser_func("ci-post", ci.post)
    add_parser_func("ci-prune-conan-cache", ci.prune_conan_cache)

    args = vars(parser.parse_args())
    args.pop("func")(**args)


if __name__ == "__main__":
    main()
