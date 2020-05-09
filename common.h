#ifndef PROJECT_COMMON_H
#define PROJECT_COMMON_H


#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>
#include <map>
#include <string>
#include <iostream>


template<class T>
struct DbVector {
    uint32_t n;
    uint32_t capacity;
    T *col;

    DbVector(uint32_t n, T *col) :
            n(n), capacity(n), col(col) {}

    DbVector(uint32_t n) : n(n), capacity(n) {
        col = new T[n];
    }

    DbVector(const DbVector& vec) : n(vec.n), capacity(vec.capacity), col(nullptr) {
        col = new T[n];
        memcpy(col, vec.col, sizeof(uint32_t)*n);
    }

    ~DbVector() {
        delete col;
    }
};


struct BatchResult {
    std::map<std::string, DbVector<int32_t>*> data;
    DbVector<uint32_t> *res_sel;

    BatchResult(const std::vector<std::string>& col_names, uint32_t n) {
        for (const auto& name : col_names) {
            DbVector<int32_t>* v = new DbVector<int32_t>(n);
            data[name] = v;
        }
    }

    BatchResult() : data() {
    }

    ~BatchResult() {
        for (auto& elem : data) {
            delete elem.second;
        }
        delete res_sel;
    }


    void add(const std::string& col_name, DbVector<int32_t>* vec) {
        data[col_name] = vec;
    }


    DbVector<int32_t>* getCol(const std::string& col_name) {
        return data[col_name];
    }


    uint32_t getn() {
        const auto& itr = data.begin();
        return itr->second->n;
    }


    void print() {
        std::vector<std::string> col_names{};
        for (const auto& elem : data) {
            col_names.push_back(elem.first);
            std::cout << elem.first << "\t\t";
        }
        std::cout << "\n=========================================================\n";

        if (res_sel == nullptr) {
            uint32_t n = data[col_names[0]]->n;
            for (uint32_t i = 0; i < n; i++) {
                for (const auto &key : col_names) {
                    DbVector<int32_t> *v = data[key];
                    int32_t val;
                    val = v->col[i];
                    std::cout << val << "\t\t";
                }
                std::cout << "\n";
            }
        }
        else {
            uint32_t n = res_sel->n;
            for (uint32_t i = 0; i < n; i++) {
                for (const auto &key : col_names) {
                    DbVector<int32_t> *v = data[key];
                    std::cout << v->col[res_sel->col[i]] << "\t\t";
                }
                std::cout << "\n";
            }
        }
    }
};


class BaseOperator {
public:
    BaseOperator() = default;
    virtual ~BaseOperator() = default;

    virtual void open() {
        throw std::invalid_argument("Not supported");
    }

    virtual void close() {
        throw std::invalid_argument("Not supported");
    }

    virtual BatchResult* next() {
        throw std::invalid_argument("Not supported");
    }
};


class ScanOperator : public BaseOperator {
private:
    uint32_t num_of_batches_;
    std::vector<std::string> columns_;
    bool initialize_;
    int32_t value_range_;

public:
    ScanOperator(uint32_t num_of_batches,
                 std::vector<std::string> columns,
                 bool initialize,
                 int32_t value_range) :
            num_of_batches_(num_of_batches),
            columns_(std::move(columns)),
            initialize_(initialize),
            value_range_(value_range)
    {}

    ~ScanOperator() final = default;

    void open() final {
        // do nothing
    }

    void close() final {
        // do nothing
    }

    BatchResult* next() final {
        if (num_of_batches_ == 0)
            return nullptr;

        uint32_t n = 1000;
        BatchResult *br = new BatchResult(columns_, n);
        // fill each col using a random numbers
        for (const auto& name : columns_) {
            DbVector<int32_t>* v = br->data[name];
            if (initialize_) {
                for (uint32_t i = 0; i < n; i++) {
                    v->col[i] = (int32_t) (rand() % value_range_);
                }
            }
        }

        num_of_batches_--;

        // printf("%d batch\n", BATCHES-num_of_batches_);
        return br;
    }
};



class QueryPlan {
private:
    BaseOperator *head_;
    bool print_result_;

public:
    QueryPlan(BaseOperator *head, bool print_result) :
        head_(head), print_result_(print_result) {
    }

    ~QueryPlan() = default;

    void open() {
        head_->open();
    }

    void close() {
        head_->close();
    }

    void printResultSet() {
        BatchResult *rs = nullptr;
        while (true) {
            rs = head_->next();
            if (rs == nullptr)
                break;

            if (print_result_)
                rs->print();
            delete rs;
        }
    }
};


#endif //PROJECT_COMMON_H
