import pyjit
import datetime


def test_basic_function_call():
    for _ in range(0, 100002):
        assert function() == True


def function():
    return function2()


def function2():
    return True


if __name__ == "__main__":
    test_basic_function_call()
