#include <unistd.h>
#include <cstdint>

extern "C" void jitfunc_1804289383(uint8_t* tape) {
    int dataptr = 0;
    char ch;

    tape[dataptr] += 8;
    while (tape[dataptr] != 0) {
        dataptr += 1;
        tape[dataptr] += 4;
        while (tape[dataptr] != 0) {
            dataptr += 1;
            tape[dataptr] += 2;
            dataptr += 1;
            tape[dataptr] += 3;
            dataptr += 1;
            tape[dataptr] += 3;
            dataptr += 1;
            tape[dataptr] += 1;
            dataptr -= 4;
            tape[dataptr] -= 1;
        }
        dataptr += 1;
        tape[dataptr] += 1;
        dataptr += 1;
        tape[dataptr] += 1;
        dataptr += 1;
        tape[dataptr] -= 1;
        dataptr += 2;
        tape[dataptr] += 1;
        while (tape[dataptr] != 0) {
            dataptr -= 1;
        }
        dataptr -= 1;
        tape[dataptr] -= 1;
    }
    dataptr += 2;
    write(1, tape+dataptr, 1);
    dataptr += 1;
    tape[dataptr] -= 3;
    write(1, tape+dataptr, 1);
    tape[dataptr] += 7;
    write(1, tape+dataptr, 1);
    write(1, tape+dataptr, 1);
    tape[dataptr] += 3;
    write(1, tape+dataptr, 1);
    dataptr += 2;
    write(1, tape+dataptr, 1);
    dataptr -= 1;
    tape[dataptr] -= 1;
    write(1, tape+dataptr, 1);
    dataptr -= 1;
    write(1, tape+dataptr, 1);
    tape[dataptr] += 3;
    write(1, tape+dataptr, 1);
    tape[dataptr] -= 6;
    write(1, tape+dataptr, 1);
    tape[dataptr] -= 8;
    write(1, tape+dataptr, 1);
    dataptr += 2;
    tape[dataptr] += 1;
    write(1, tape+dataptr, 1);
    dataptr += 1;
    tape[dataptr] += 2;
    write(1, tape+dataptr, 1);
}

