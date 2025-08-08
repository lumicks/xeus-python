import os


def pre_build(conanfile):
    """Help Conan make better use of `ccache`

    Conan 2 builds in folders that look like `<base>/b/<short_package_name><hash>/b`.
    This creates cache misses in `ccache` because even a tiny change creates a different hash
    value in the base directory so it never matches anything. But if we pass this directory as
    `CCACHE_BASEDIR`, it will be able to match things correctly.
    """
    conanfile.output.info(f"CCACHE_BASEDIR={conanfile.build_folder}")
    os.environ["CCACHE_BASEDIR"] = str(conanfile.build_folder)
