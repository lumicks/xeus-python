import sys

from . import checks


def _report_and_exit(issues: list[str]):
    print("lmxcpp doctor found the following issues:")
    for i, issue in enumerate(issues, start=1):
        print(f" {i}. {issue}\n")
    sys.exit(1)


def run_checks(*check_functions):
    """Report found issue, if any"""
    if issues := sum([f() for f in check_functions], []):
        _report_and_exit(issues)


def doctor():
    """Check if everything is fine with the dev environment and tools"""
    run_checks(*checks.everything)
    print("No issues found. Your system is ready to build C++.")
