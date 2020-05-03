#include <iostream>
#include <utility>
#include <vector>
#include <map>
#include <cstdlib>
#include <memory>
#include <cstring>


const uint32_t BATCHES = 100000;


/**
 * Memory management rules
 *   1. use new/delete
 *   2. who own who release
 */

/************************************************************************
 *
 * PRITIMITIVE
 *
 **************************************************************************/


uint32_t map_add_int32_col_int32_col(uint32_t n, int32_t *res, int32_t *col1, int32_t *col2, uint32_t *sel) {
    if (sel == nullptr) {
        for (uint32_t i = 0; i < n; i++)
            res[i] = col1[i] + col2[i];
    }
    else {
        for (uint32_t i = 0; i < n; i++)
            res[i] = col1[sel[i]] + col2[sel[i]];
    }

    return n;
}


uint32_t map_sub_int32_col_int32_col(uint32_t n, int32_t *res, int32_t *col1, int32_t *col2, uint32_t *sel) {
    if (sel == nullptr) {
        for (uint32_t i = 0; i < n; i++)
            res[i] = col1[i] - col2[i];
    }
    else {
        for (uint32_t i = 0; i < n; i++)
            res[i] = col1[sel[i]] - col2[sel[i]];
    }

    return n;
}


uint32_t map_mul_int32_col_int32_col(uint32_t n, int32_t *res, int32_t *col1, int32_t *col2, uint32_t *sel) {
    if (sel == nullptr) {
        for (uint32_t i = 0; i < n; i++)
            res[i] = col1[i] * col2[i];
    }
    else {
        for (uint32_t i = 0; i < n; i++)
            res[i] = col1[sel[i]] * col2[sel[i]];
    }

    return n;
}


uint32_t map_add_int32_val_int32_col(uint32_t n, int32_t *res, int32_t val, int32_t *col2, uint32_t *sel) {
    if (sel == nullptr) {
        for (uint32_t i = 0; i < n; i++)
            res[i] = val + col2[i];
    }
    else {
        for (uint32_t i = 0; i < n; i++)
            res[i] = val + col2[sel[i]];
    }

    return n;
}


uint32_t map_sub_int32_val_int32_col(uint32_t n, int32_t *res, int32_t val, int32_t *col2, uint32_t *sel) {
    if (sel == nullptr) {
        for (uint32_t i = 0; i < n; i++)
            res[i] = val - col2[i];
    }
    else {
        for (uint32_t i = 0; i < n; i++)
            res[i] = val - col2[sel[i]];
    }

    return n;
}


uint32_t map_mul_int32_val_int32_col(uint32_t n, int32_t *res, int32_t val, int32_t *col2, uint32_t *sel) {
    if (sel == nullptr) {
        for (uint32_t i = 0; i < n; i++)
            res[i] = val * col2[i];
    }
    else {
        for (uint32_t i = 0; i < n; i++)
            res[i] = val * col2[sel[i]];
    }

    return n;
}





/************************************************************************
 *
 * DAG NODES
 *
 **************************************************************************/


#define CHILD_TYPE_COL 1
#define CHILD_TYPE_VAL 2
#define CHILD_TYPE_DAG 3


struct DbVector {
    uint32_t n;
    int32_t *col;

    DbVector(uint32_t n, int32_t *col) :
            n(n), col(col) {}

    DbVector(uint32_t n) : n(n) {
        col = new int32_t[n];
    }

    DbVector(const DbVector& vec) : n(vec.n), col(nullptr){
        col = new int32_t[n];
        memcpy(col, vec.col, sizeof(uint32_t)*n);
    }

    ~DbVector() {
        delete col;
    }
};



class DAGNode {
public:
    DAGNode() = default;
    virtual ~DAGNode() = default;

    virtual uint32_t compute(DbVector** res) = 0;
    virtual int getLeftChildType() = 0;
    virtual int getRightChildType() = 0;

    virtual void setLeftVector(DbVector* v) {
        throw std::invalid_argument("not support");
    }

    virtual void setRightVector(DbVector* v) {
        throw std::invalid_argument("not support");
    }

    virtual std::string getLeftChildColName() {
        throw std::invalid_argument("not support");
    }

    virtual std::string getRightChildColName() {
        throw std::invalid_argument("not support");
    }

    virtual DAGNode* getLeftChildDagNode() {
        throw std::invalid_argument("not support");
    }

    virtual DAGNode* getRightChildDagNode() {
        throw std::invalid_argument("not support");
    }
};


#define OP_ADD      1
#define OP_SUB      2
#define OP_MUL      3


class ValColDAGNode : public DAGNode {
private:
    int op_;
    uint32_t (*primitive)(uint32_t n, int32_t *res, int32_t val, int32_t *col2, uint32_t *sel);
    int32_t left_val_;
    DAGNode *right_;
    std::string col_name_;

    DbVector *right_vec_;

private:
    void assignPrimitive_() {
        switch (op_)
        {
            case OP_ADD:
                primitive = map_add_int32_val_int32_col;
                break;

            case OP_MUL:
                primitive = map_mul_int32_val_int32_col;
                break;

            case OP_SUB:
                primitive = map_sub_int32_val_int32_col;
                break;

            default:
                throw std::invalid_argument("Unkonwn op");
        }
    }

public:
    ValColDAGNode(int op, int32_t left_val, DAGNode *right) :
        op_(op), right_(right), col_name_(""), left_val_(left_val), right_vec_(nullptr) {
        assignPrimitive_();
    }

    ValColDAGNode(int op, int32_t left_val, std::string col_name) :
        op_(op), left_val_(left_val), right_(nullptr), col_name_(std::move(col_name)), right_vec_(nullptr) {
        assignPrimitive_();
    }

    virtual ~ValColDAGNode() {
        delete right_;
        delete right_vec_;
    }

    int getLeftChildType() final {
        return CHILD_TYPE_VAL;
    }

    int getRightChildType() final {
        if (right_ != nullptr)
            return CHILD_TYPE_DAG;
        return CHILD_TYPE_COL;
    }

    void setRightVector(DbVector* v) final {
        delete right_vec_;
        right_vec_ = v;
    }

    std::string getRightChildColName() final {
        return col_name_;
    }

    DAGNode* getRightChildDagNode() final {
        return right_;
    }

    uint32_t compute(DbVector** res) final {
        *res = new DbVector(right_vec_->n);
        return primitive(right_vec_->n, (*res)->col, left_val_, right_vec_->col, nullptr);
    }
};


class ColColDAGNode : public DAGNode {
private:
    int op_;
    uint32_t (*primitive)(uint32_t n, int32_t *res, int32_t *col1, int32_t *col2, uint32_t *sel);
    DAGNode *right_;
    std::string right_col_name_;
    DAGNode *left_;
    std::string left_col_name_;

    DbVector* left_vec_;
    DbVector* right_vec_;


private:
    void assignPrimitive_() {
        switch (op_)
        {
            case OP_ADD:
                primitive = map_add_int32_col_int32_col;
                break;

            case OP_MUL:
                primitive = map_mul_int32_col_int32_col;
                break;

            case OP_SUB:
                primitive = map_sub_int32_col_int32_col;
                break;

            default:
                throw std::invalid_argument("Unkonwn op");
        }
    }


public:
    ColColDAGNode(int op, DAGNode* left, DAGNode* right) :
        op_(op), right_(right), left_(left), left_vec_(nullptr), right_vec_(nullptr) {
        assignPrimitive_();
    }

    ColColDAGNode(int op, std::string left_col_name, DAGNode* right) :
        op_(op), left_col_name_(std::move(left_col_name)), right_(right), left_(nullptr), left_vec_(nullptr), right_vec_(nullptr) {
        assignPrimitive_();
    }

    ColColDAGNode(int op, DAGNode* left, std::string right_col_name) :
        op_(op), left_(left), right_(nullptr), right_col_name_(std::move(right_col_name)), left_vec_(nullptr), right_vec_(nullptr) {
        assignPrimitive_();
    }

    ColColDAGNode(int op, std::string left_col_name, std::string right_col_name) :
        op_(op), left_col_name_(std::move(left_col_name)), right_col_name_(std::move(right_col_name)), left_(nullptr), right_(nullptr), left_vec_(nullptr), right_vec_(nullptr) {
        assignPrimitive_();
    }

    virtual ~ColColDAGNode() {
        delete left_;
        delete right_;
        delete left_vec_;
        delete right_vec_;
    }

    int getLeftChildType() final {
        if (left_ != nullptr)
            return CHILD_TYPE_DAG;
        return CHILD_TYPE_COL;
    }

    int getRightChildType() final {
        if (right_ != nullptr)
            return CHILD_TYPE_DAG;
        return CHILD_TYPE_COL;
    }


    void setLeftVector(DbVector* v) final {
        delete left_vec_;
        left_vec_ = v;
    }

    void setRightVector(DbVector* v) final {
        delete right_vec_;
        right_vec_ = v;
    }


    std::string getLeftChildColName() final {
        return left_col_name_;
    }

    std::string getRightChildColName() final {
        return right_col_name_;
    }

    DAGNode* getLeftChildDagNode() final {
        return left_;
    }

    virtual DAGNode* getRightChildDagNode() {
        return right_;
    }

    uint32_t compute(DbVector** res) {
        *res = new DbVector(left_vec_->n);
        return primitive(left_vec_->n, (*res)->col, left_vec_->col, right_vec_->col, nullptr);
    }
};



/************************************************************************
 *
 * OPERATORS
 *
 **************************************************************************/


struct BatchResult {
    std::map<std::string, DbVector*> data;

    BatchResult(const std::vector<std::string>& col_names, uint32_t n) {
        for (const auto& name : col_names) {
            DbVector* v = new DbVector(n);
            data[name] = v;
        }
    }

    BatchResult() : data() {
    }

    ~BatchResult() {
        for (auto& elem : data) {
            delete elem.second;
        }
    }


    void add(const std::string& col_name, DbVector* vec) {
        data[col_name] = vec;
    }


    DbVector* getCol(const std::string& col_name) {
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

        uint32_t n = data[col_names[0]]->n;
        for (uint32_t i = 0; i < n ; i++) {
            for (const auto& key : col_names) {
                DbVector* v = data[key];
                int32_t val;
                val = v->col[i];
                std::cout << val << "\t\t";
            }
            std::cout << "\n";
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



/**
 * 这个简单的project operator把scan operator的所有列经过计算后得到一个
 * 唯一的列。
 */
class ProjectOperator : public BaseOperator {
private:
    BaseOperator* next_;
    DAGNode* expr_;
    std::string col_name_;

public:
    ProjectOperator(BaseOperator *next, std::string col_name, DAGNode* expr) :
        next_(next), expr_(expr), col_name_(std::move(col_name)) {
    }

    ~ProjectOperator() final {
        delete next_;
        delete expr_;
    }

    void open() {
        next_->open();
    }

    void close() {
        next_->close();
    }

    BatchResult* next() {
        std::unique_ptr<BatchResult> br(next_->next());
        if (br == nullptr)
            return nullptr;

        DbVector* vec = nullptr;
        evaluateExpr_(&vec, expr_, br.get());

        BatchResult* rs = new BatchResult();
        rs->add(col_name_, vec);
        return rs;
    }

private:
    // evaluate the expression -
    uint32_t evaluateExpr_(DbVector** res, DAGNode *expr, BatchResult* input) {
        uint32_t n = input->getn();
        DbVector *lres = nullptr, *rres = nullptr;
        DAGNode *tmp_node = nullptr;
        DbVector *tmp_vec = nullptr;

        switch (expr->getLeftChildType())
        {
            case CHILD_TYPE_VAL:
                break;
            case CHILD_TYPE_DAG:
                tmp_node = expr->getLeftChildDagNode();
                evaluateExpr_(&lres, tmp_node, input);
                break;
            case CHILD_TYPE_COL:
                tmp_vec = input->getCol(expr->getLeftChildColName());
                lres = new DbVector(*tmp_vec);
                break;
        }
        if (lres != nullptr)
            expr->setLeftVector(lres);

        switch (expr->getRightChildType())
        {
            case CHILD_TYPE_VAL:
                break;
            case CHILD_TYPE_COL:
                tmp_vec = input->getCol(expr->getRightChildColName());
                rres = new DbVector(*tmp_vec);
                break;
            case CHILD_TYPE_DAG:
                DAGNode* tmp_node = expr->getRightChildDagNode();
                evaluateExpr_(&rres, tmp_node, input);
                break;
        }
        if (rres != nullptr)
            expr->setRightVector(rres);

        return expr->compute(res);
    }
};


class ScanOperator : public BaseOperator {
private:
    uint32_t num_of_batches_;
    std::vector<std::string> columns_;

public:
    ScanOperator(uint32_t num_of_batches, const std::vector<std::string>& columns) :
        num_of_batches_(num_of_batches),
        columns_(columns)
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
            DbVector* v = br->data[name];
            /*
            for (uint32_t i = 0; i < n; i++) {
                v->col[i] = (int32_t)rand();
            }
            */
        }

        num_of_batches_--;

        // printf("%d batch\n", BATCHES-num_of_batches_);
        return br;
    }
};



/************************************************************************
 *
 * Query compiler
 *
 **************************************************************************/



class QueryPlan {
private:
    BaseOperator *head_;

public:
    QueryPlan(BaseOperator *head) : head_(head) {
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

            // rs->print();
            delete rs;
        }
    }
};


QueryPlan *compileQuery() {
    std::vector<std::string> col_names{"extprice", "discount", "tax"};
    ScanOperator *scan_op = new ScanOperator(BATCHES, col_names);

    ValColDAGNode *oneMinusDiscount = new ValColDAGNode(OP_SUB, 1, "discount");
    ColColDAGNode *extpriceMul = new ColColDAGNode(OP_MUL, "extprice", oneMinusDiscount);
    ValColDAGNode *oneAddTax = new ValColDAGNode(OP_ADD, 1, "tax");
    ColColDAGNode *mul = new ColColDAGNode(OP_MUL, extpriceMul, oneAddTax);
    ProjectOperator *proj_op = new ProjectOperator(scan_op, "bonus", mul);

    return new QueryPlan(proj_op);
}


int main(int argc, char*argv[]) {
    QueryPlan *query_plan = compileQuery();
    query_plan->open();
    query_plan->printResultSet();
    query_plan->close();
    delete query_plan;
}