import functools
import time

import fasteners


def timed(f):
    @functools.wraps(f)
    def wrapper(*args, **kwargs):
        print(f"[{f.__module__}::{f.__name__} running...]", flush=True)
        t0 = time.time()
        result = f(*args, **kwargs)
        print(f"[{f.__module__}::{f.__name__} took {time.time() - t0:.1f} seconds]", flush=True)
        return result

    return wrapper


def locked(filename_or_callable):
    """Set up a process lock before executing the decorated function"""

    def decorator(f):
        @functools.wraps(f)
        def wrapper(*args, **kwargs):
            if callable(filename_or_callable):
                filename = filename_or_callable()
            else:
                filename = filename_or_callable
            with fasteners.InterProcessLock(filename):
                return f(*args, **kwargs)

        return wrapper

    return decorator
