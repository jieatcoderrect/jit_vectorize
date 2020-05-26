#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>


constexpr size_t TAPE_SIZE = 30000;


struct BrainFuckProgram {
    std::string program;
};


class BrainFuckInterpreter {
private:
    BrainFuckProgram p_;
    uint8_t *tape_;
    uint32_t dc_;
    uint32_t pc_;

public:
    explicit BrainFuckInterpreter(BrainFuckProgram p) : p_(std::move(p)), tape_(new uint8_t[TAPE_SIZE]), pc_(0), dc_(0) {
    }

    ~BrainFuckInterpreter() {
        delete []tape_;
    }

public:
    void interpret() {
        memset(tape_, 0, TAPE_SIZE);
        dc_ = 0;
        pc_ = 0;

        auto n = p_.program.length();
        const char* p = p_.program.data();
        int nesting {0};

        while (pc_ < n) {
            const char ch = p[pc_];
            char dch;
            switch (ch)
            {
                case '+':
                    tape_[dc_]++;
                    break;
                case '-':
                    tape_[dc_]--;
                    break;
                case '>':
                    dc_++;
                    if (dc_ == TAPE_SIZE)
                        dc_ = 0;
                    break;
                case '<':
                    dc_--;
                    if (dc_ == -1)
                        dc_ = TAPE_SIZE - 1;
                    break;
                case '.':
                    dch = tape_[dc_];
                    std::cout.put(dch);
                    break;
                case ',':
                    tape_[dc_] = std::cin.get();
                    break;
                case '[':
                    if (tape_[dc_] == 0) {
                        nesting = 1;
                        while (nesting > 0 && ++pc_ < n) {
                            if (p[pc_] == '[') {
                                nesting++;
                            } else if (p[pc_] == ']') {
                                nesting--;
                            }
                        }
                    }
                    break;
                case ']':
                    nesting = 1;
                    while (nesting > 0 && pc_ > 0) {
                        pc_--;
                        if (p[pc_] == '[') {
                            nesting--;
                        } else if (p[pc_] == ']') {
                            nesting++;
                        }
                    }
                    pc_--;
                    break;

                default:
                    throw std::invalid_argument("Unknown command");
            }
            pc_++;
        }
    }
};



static ssize_t GenerateProgram(BrainFuckProgram& p, const std::string& filepath) {
    struct stat stat_buf;
    int rc = stat(filepath.c_str(), &stat_buf);
    if (rc < 0)
        return -1;

    char *buf = new char[stat_buf.st_size];

    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd < 0)
        return -1;
    auto n = read(fd, buf, stat_buf.st_size);
    close(fd);

    for (int i = 0; i < n; i++) {
        char ch = buf[i];
        if (ch == '<' || ch == '>' || ch == '+' || ch == '-' || ch == '.' || ch == ',' || ch == '[' || ch == ']') {
            p.program.push_back(ch);
        }
    }
    return p.program.length();
}


int main(int argc, char**argv) {
    if (argc != 2) {
        std::cout << "Usage: bf <sourcefile>\n";
        exit(1);
    }

    BrainFuckProgram bfp;
    auto rc = GenerateProgram(bfp, argv[1]);
    if (rc < 0) {
        std::cerr << "Failed to read the source code\n";
        exit(1);
    }

    BrainFuckInterpreter interpreter(bfp);
    interpreter.interpret();
}