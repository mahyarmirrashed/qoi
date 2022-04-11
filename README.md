# Quite OK Image Format

This C implementation of the QOI (Quite OK Image) foramt is implemented according to the [specification](https://qoiformat.org/qoi-specification.pdf) and is heavily influenced by the [original C implementation on GitHub](https://github.com/phoboslab/qoi/).

Some optimizations and better programming practices make the code somewhat more readable. Overall, it was a fun experiment in reading and implementing a specification and is definitely something that could be easily be adapted for a classroom or workshop. Specifically, everything inside the for-loops in the `qoi_decode()` and `qoi_encode()` functions inside `qoi.c` file is fair game for anyone with basic programming experience.

I really appreciate this codec because of it's simplicity and that it minimally uses any RAM. In fact, the implementation mostly uses a hash table, a run length, the current pixel, and the previous pixel.
