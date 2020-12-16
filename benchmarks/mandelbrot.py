# import pyjit
from contextlib import closing
from itertools import islice
from os import cpu_count
from sys import argv, stdout


def pixels(y, n, absfun):
    range7 = range(7)
    pixel_bits = bytearray(128 >> pos for pos in range(8))
    c1 = 2.0 / float(n)
    c0 = -1.5 + 1j * y * c1 - 1j
    x = 0

    while True:
        pixel = 0
        c = x * c1 + c0
        for pixel_bit in pixel_bits:
            z = c
            for _ in range7:
                for _ in range7:
                    z = z ** 2 + c
                if absfun(z) >= 2.0:
                    break
            else:
                pixel += pixel_bit
            c += c1
        yield pixel
        x += 8


def compute_row(p):
    y, n = p

    result = bytearray(islice(pixels(y, n, abs), (n + 7) // 8))
    result[-1] &= 0xFF << (8 - n % 8)
    return y, result


def ordered_rows(rows, n):
    order = [None] * n
    i = 0
    j = n
    lenfun = len
    nextfun = next
    zero = 0
    one = 1
    none = None
    while i < lenfun(order):
        if j > zero:
            row = nextfun(rows)
            order[row[zero]] = row
            j -= one

        if order[i]:
            yield order[i]
            order[i] = none
            i += one


def compute_rows(n, f):
    row_jobs = ((y, n) for y in range(n))

    if True and cpu_count() < 2:
        yield from map(f, row_jobs)
    else:
        from multiprocessing import Pool

        with Pool() as pool:
            unordered_rows = pool.imap_unordered(f, row_jobs)
            yield from ordered_rows(unordered_rows, n)


def mandelbrot(n):
    write = stdout.buffer.write

    with closing(compute_rows(n, compute_row)) as rows:
        write("P4\n{0} {0}\n".format(n).encode())
        for row in rows:
            write(row[1])


if __name__ == "__main__":
    mandelbrot(16000)