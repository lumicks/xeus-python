import functools
import pathlib


@functools.lru_cache(maxsize=1)
def conan_api():
    from conan.api.conan_api import ConanAPI

    return ConanAPI()


@functools.lru_cache(maxsize=1)
def conan_home() -> pathlib.Path:
    """The Conan home folder is usually `~/.conan2` but can be customized"""
    return pathlib.Path(conan_api().home_folder)
