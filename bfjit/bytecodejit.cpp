#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#include <fstream>
#include <dlfcn.h>
#include <array>
#include <memory>
#include <cstdio>

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


static void writeFile(const std::string& path, const std::string& data) {
    std::ofstream myfile;
    myfile.open (path);
    myfile << data;
    myfile.close();
}


static void ConvertToBytecode(char ch, int32_t count, int32_t *program, size_t *sz) {
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


static std::string CompileJit(int32_t *program, size_t sz, const std::string& funcname) {
    std::ostringstream out;
    std::string indention = "    ";

    // generate the function name
    //
    // static void jitfunc_nnnn(uint8_t *tape) {
    //    int dataptr = 0;
    //    char ch;
    //
    //
    out << "#include <unistd.h>\n"
           "#include <cstdint>\n";
    out << "extern \"C\" void "<< funcname << "(uint8_t* tape) {\n";
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
                out << indention << "write(1, tape+dataptr, 1);\n";
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


static std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}


/**
 * We assume the source file is funcname.cpp and the SO file
 * will be named funcname.so
 */
static void  compileToDynalib(char *funcname) {
    char cmdline[256];
    sprintf(cmdline, "clang++ -shared -fpic %s.cpp -o %s.so", funcname, funcname);
    exec(cmdline);
}


int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: bytecodejit <sourcefile>\n";
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
    std::cout << "Compilte to byte code\n";

    // compile bytecode to a function in c
    char funcname[64];
    sprintf(funcname, "jitfunc_%d", rand());
    std::string cSrc = CompileJit(program, sz, funcname);
    std::cout << "Convert byte code to C\n";

    // call LLVM JIT API to compile it into x64 code
    // todo

    // save the file to disk and compile it into a .so file
    std::string src_path(funcname);
    src_path += ".cpp";
    writeFile(src_path.c_str(), cSrc);
    compileToDynalib(funcname);
    std::cout << "Compile C to SO file\n";

    uint8_t *tape = new uint8_t[TAPE_SIZE];
    char* error;
    void (*fn)(uint8_t *);

    // load library
    std::string so_file_path = std::string(funcname) + ".so";
    void *lib_handle = dlopen(so_file_path.c_str(), RTLD_LAZY);
    if (!lib_handle)
    {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    // obtain the function handler
    *(void **)(&fn) = dlsym(lib_handle, funcname);
    if ((error = dlerror()) != NULL)
    {
        fprintf(stderr, "%s\n", error);
        exit(1);
    }

    (*fn)(tape);

    delete []tape;

    // release library
    dlclose(lib_handle);

    std::remove(so_file_path.c_str());
    std::remove(src_path.c_str());
}