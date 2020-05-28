#include <iostream>
#include <cstring>>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>


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


class BrainFuckVM {
private:
    uint8_t *tape_;
    int data_ptr_;

    int32_t *program_;
    int instructions_;
    int pc_;

public:
    BrainFuckVM(int32_t *program, size_t sz) :
            tape_(new uint8_t[TAPE_SIZE]),
            data_ptr_(0),
            program_(new int32_t[sz]),
            pc_(0),
            instructions_(sz / 2)
    {
        memset(tape_, 0, TAPE_SIZE);
        memcpy(program_, program, sizeof(int32_t) * sz);
    }

    ~BrainFuckVM() {
        delete tape_;
        delete program_;
    }

public:
    void Execute() {
        while (pc_ >= 0 && pc_ < instructions_) {
            int i = pc_ * 2;
            int32_t op = program_[i];
            int32_t opand = program_[i + 1];
            bool move_pc = true;

            switch (op)
            {
                case ADD:
                    tape_[data_ptr_] += (uint8_t)opand;
                    break;

                case SUB:
                    tape_[data_ptr_] -= (uint8_t)opand;
                    break;

                case LEFT_MOVE:
                    data_ptr_ -= opand;
                    if (data_ptr_ < 0)
                        data_ptr_ = 0;
                    break;

                case RIGHT_MOVE:
                    data_ptr_ += opand;
                    if (data_ptr_ >= TAPE_SIZE)
                        data_ptr_ = TAPE_SIZE - 1;
                    break;

                case IN:
                    tape_[data_ptr_] = std::cin.get();
                    break;

                case OUT:
                    std::cout.put((char)tape_[data_ptr_]);
                    break;

                case JZ:
                    if (tape_[data_ptr_] == 0) {
                        move_pc = false;
                        pc_ = opand;
                    }
                    break;

                case JMP:
                    pc_ = opand;
                    move_pc = false;
                    break;

                default:
                    throw std::invalid_argument("Unknown op");
            }

            pc_ += move_pc;
        }
    }
};


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

    // start VM to execute the program
    BrainFuckVM vm(program, sz);
    vm.Execute();
}