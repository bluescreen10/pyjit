# import pyjit
import dis
from datetime import datetime


def test_a(times):
    value = False
    for i in range(0, times):
        # value = simple_function3()
        value = simple_function2()
    print(value)
    assert value == False


def simple_function():
    return 1


def simple_function2():
    value = True
    return not value


def simple_function3():
    return simple_function2()


if __name__ == "__main__":
    # warm up
    dis.dis(simple_function2)
    test_a(1002)
    print("Testing")
    start = datetime.now()
    test_a(500_000_000)
    ellapsed = datetime.now() - start
    print(f"ellapsed time:{ellapsed}")