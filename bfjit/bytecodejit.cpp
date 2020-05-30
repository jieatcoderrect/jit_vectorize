#include <iostream>
#include <cstring>>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>


constexpr size_t TAPE_SIZE = 30000;


// add <num> to current cell
constexpr int32_t ADD = 1;

// move data pointer <num> left
constexpr int32_t LEFT_MOVE = 2;

// move data pinter <num> right
constexpr int32_t RIGHT_MOVE = 3;

// sub <num> from current cell
constexpr int32_t SUB = 4;

// output the current cell as a char
constexpr int32_t OUT = 5;

// input a char into the current cell
constexpr int32_t IN = 6;

// jmp to the location if the current cel == 0
constexpr int32_t JZ = 7;

// jmp to the location
constexpr int32_t JMP = 8;


void ConvertToBytecode(char ch, int32_t count, int32_t *program, size_t *sz) {
    size_t loc = *sz;
    bool done = true;

    switch(ch)
    {
        case '+':
            program[loc] = ADD;
            break;

        case '-':
            program[loc] = SUB;
            break;

        case '>':
            program[loc] = RIGHT_MOVE;
            break;

        case '<':
            program[loc] = LEFT_MOVE;
            break;

        case '.':
            program[loc] = OUT;
            break;

        case ',':
            program[loc] = IN;
            break;

        case '[':
            program[loc] = JZ;
            break;

        case ']':
            program[loc] = JMP;
            break;

        default:
            done = false;
    }

    if (done) {
        program[loc + 1] = count;
        *sz += 2;
    }
}


int32_t* Compile(const std::string& src_code, size_t *rsz) {
    size_t max_sz = src_code.length() * 2;
    int32_t *program = new int32_t[max_sz];
    size_t sz = 0;

    // compile the source code into vm bytecode
    const char* data = src_code.data();
    const size_t len = src_code.length();
    char prevCh = 'A';
    int32_t count = 0;
    for (int i = 0; i < len; i++) {
        char ch = data[i];
        switch (ch)
        {
            case '+':;
            case '-':;
            case '>':;
            case '<':;
                if (prevCh == ch) {
                    count++;
                } else {
                    ConvertToBytecode(prevCh, count, program, &sz);
                    prevCh = ch;
                    count = 1;
                }
                break;

            case '.':;
            case ',':;
            case '[':;
            case ']':
                ConvertToBytecode(prevCh, count, program, &sz);
                ConvertToBytecode(ch, 0, program, &sz);
                prevCh = 'A';
                count = 0;
                break;
        }
    }

    // pass two - set jz/jmp destination address
    for (uint32_t addr = 0; addr < sz / 2; addr++) {
        int32_t op = program[addr * 2];
        if (op != JZ) {
            continue;
        }

        int nesting = 1;
        for (int32_t tmpaddr = addr + 1; tmpaddr < sz / 2; tmpaddr++) {
            int32_t tmpop = program[tmpaddr * 2];
            if (tmpop == JZ) {
                nesting++;
            }
            else if (tmpop == JMP) {
                nesting--;
                if (nesting == 0) {
                    program[addr * 2 + 1] = tmpaddr + 1;
                    program[tmpaddr * 2 + 1] = addr;
                    break;
                }
            }
        }
        if (nesting != 0) {
            throw std::invalid_argument("Un-matched [");
        }
    }

    *rsz = sz;
    return program;
}


static std::string CompileJit(int32_t *program, size_t sz) {
    std::ostringstream out;
    std::string indention = "    ";

    // generate the function name
    //
    // static void jitfunc_nnnn(uint8_t *tape) {
    //    int dataptr = 0;
    //    char ch;
    //
    //
    out << "static void jitfunc_" << rand() << "(uint8_t* tape) {\n";
    out << "    int dataptr = 0;\n";
    out << "    char ch;\n\n";

    int instructions = sz / 2;
    for (int pc = 0; pc < instructions; pc++) {
        int32_t op = program[pc * 2];
        int32_t opand = program[pc * 2 + 1];
        switch (op)
        {
            case ADD:
                //
                // tape[dataptr] += opand;
                //
                out << indention << "tape[dataptr] += " << opand << ";\n";
                break;

            case SUB:
                //
                // tape[dataptr] -= opand;
                //
                out << indention << "tape[dataptr] -= " << opand << ";\n";
                break;

            case LEFT_MOVE:
                //
                // dataptr -= opand;
                //
                out << indention << "dataptr -= " << opand << ";\n";
                break;

            case RIGHT_MOVE:
                //
                // dataptr += opand;
                //
                out << indention << "dataptr += " << opand << ";\n";
                break;

            case IN:
                //
                // read(0, tape+dataptr, 1)'
                //
                out << indention << "read(0, tape+dataptr, 1);\n";
                break;

            case OUT:
                //
                // write(1, (char)tape[data[tr], 1);
                //
                out << indention << "write(1, (char)tape[dataptr], 1);\n";
                break;

            case JZ:
                //
                // while (tape[dataptr] != 0) {
                //
                out << indention << "while (tape[dataptr] != 0) {\n";
                indention += "    ";
                break;

            case JMP:
                indention = indention.substr(0, indention.length() - 4);
                out << indention << "}\n";
                break;
        }
    }

    out << "}\n";
    return out.str();
}


int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: simplevm <sourcefile>\n";
        exit(1);
    }

    // load the source code into a string
    struct stat stat_buf;
    int rc = stat(argv[1], &stat_buf);
    if (rc < 0)
        return -1;

    char *buf = new char[stat_buf.st_size];

    int fd = open(argv[1], O_RDONLY);
    if (fd < 0)
        return -1;
    auto n = read(fd, buf, stat_buf.st_size);
    close(fd);

    // compile the source code into bytecode of our VM
    std::string src_code(buf, n);
    size_t sz = 0;
    int32_t *program = Compile(src_code, &sz);

    // compile bytecode to a function in c
    std::string cSrc = CompileJit(program, sz);
    std::cout << cSrc << "\n";

    // call LLVM JIT API to compile it into x64 code
    // todo

    uint8_t *tape = new uint8_t[TAPE_SIZE];
    // call the code to

    delete []tape;
}