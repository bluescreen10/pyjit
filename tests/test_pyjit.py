import pyjit
from datetime import datetime


def test_a(times):
    value = 0
    for i in range(0, times):
        value = simple_function()
    assert value == 1


def simple_function():
    return 1


if __name__ == "__main__":
    # warm up
    test_a(1002)
    print("Testing")
    start = datetime.now()
    test_a(100_000_000)
    ellapsed = datetime.now() - start
    print(f"ellapsed time:{ellapsed}")