import pyjit

pyjit.disable()


def test_const_return():
    pyjit.enable()
    for _ in range(0, 1001):
        assert const_function() == True


def const_function():
    return True


if __name__ == "__main__":
    test_const_return()