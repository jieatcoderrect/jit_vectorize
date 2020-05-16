#include <iostream>
#include "common.h"


/**
 * select extprice * (100 - discount) * (100 + tax)
 * from table
 * where extprice < 50
 *
 * This program evaluates two strategies
 *   1. compute-all + jit projection + non-branching, vec-only selection
 *   2. non-compute-all + jie projection + non-branching, vec-only selection
 *
 * So, the key is compute-all vs. non-compute-all. The latter will prohibit SIMDization.
 */

const uint32_t BATCHES = 100000;


/**
 * Ross, Kenneth A. "Conjunctive selection conditions in main memory." Proceedings of
 * the twenty-first ACM SIGMOD-SIGACT-SIGART symposium on Principles of database systems. 2002.
 **/
static uint32_t sel_lt_int32_col_int32_val_nonbranching(uint32_t n,
                                                        uint32_t *res_sel,
                                                        int32_t *col,
                                                        int32_t val,
                                                        uint32_t *sel) {
    uint32_t res = 0;

    if (sel != nullptr) {
        for (uint32_t i = 0; i < n; i++) {
            res_sel[res] = sel[i];
            res += (col[sel[i]] < val);
        }
    }
    else {
        for (uint32_t i = 0; i < n; i++) {
            res_sel[res] = i;
            res += (col[i] < val);
        }
    }

    return res;
}


#define COND_LT     1


class CondDAGNode {
private:
    int cond_;

public:
    CondDAGNode(int cond) : cond_(cond) {}
    virtual ~CondDAGNode() = default;

    virtual uint32_t compute(DbVector<uint32_t>* res_sel) = 0;
    virtual uint32_t compute(DbVector<uint32_t>* res_sel, DbVector<uint32_t>* src_sel) = 0;

    virtual void setLeftVector(DbVector<int32_t> *vec) = 0;
    virtual std::string getLeftColName() = 0;
};


class ColValCondDAGNode : public CondDAGNode {
private:
    bool branching_;
    uint32_t (*primitive_)(uint32_t, uint32_t*, int32_t*, int32_t, uint32_t*);
    int32_t right_val_;

    DbVector<int32_t> *left_vec_;
    std::string left_col_name_;

public:
    ColValCondDAGNode(int cond, std::string left_col_name, int32_t right_val) :
            CondDAGNode(cond),
            left_col_name_(std::move(left_col_name)),
            left_vec_(nullptr),
            right_val_(right_val) {
        assignPrimitive_();
    }

    ~ColValCondDAGNode() final {
    }

    void setLeftVector(DbVector<int32_t> *vec) final {
        left_vec_ = vec;
    }

    std::string getLeftColName() final {
        return left_col_name_;
    }

    uint32_t compute(DbVector<uint32_t>* res_sel) final {
        auto n = primitive_(left_vec_->n, res_sel->col, left_vec_->col, right_val_, nullptr);
        res_sel->n = n;
        return n;
    }

    uint32_t compute(DbVector<uint32_t>* res_sel, DbVector<uint32_t>* src_sel) final {
        auto n = primitive_(src_sel->n, res_sel->col, left_vec_->col, right_val_, src_sel->col);
        res_sel->n = n;
        return n;
    }

private:
    void assignPrimitive_() {
        primitive_ = sel_lt_int32_col_int32_val_nonbranching;
    }
};


class SelectVectorizationOnlyNonBranchingOperator : public BaseOperator {
private:
    BaseOperator* next_;
    std::vector<CondDAGNode*> expr_;

public:
    SelectVectorizationOnlyNonBranchingOperator(BaseOperator *next, std::vector<CondDAGNode*> expr) :
            next_(next), expr_(std::move(expr)) {
    }

    ~SelectVectorizationOnlyNonBranchingOperator() final {
        delete next_;
        for (auto node : expr_) {
            delete node;
        }
        expr_.clear();
    }

    void open() {
        next_->open();
    }

    void close() {
        next_->close();
    }

    BatchResult* next() {
        BatchResult *br = next_->next();
        if (br == nullptr)
            return br;

        DbVector<uint32_t> *res_sel = new DbVector<uint32_t>(br->getn());
        bool first = true;
        for (auto node : expr_) {
            DbVector<int32_t> *dbVector = br->getCol(node->getLeftColName());
            node->setLeftVector(dbVector);
            if (first) {
                node->compute(res_sel);
                first = false;
            }
            else {
                node->compute(res_sel, res_sel);
            }
        }

        br->res_sel = res_sel;
        return br;
    }
};


class ProjectJitComputeAllOperator : public BaseOperator {
private:
    BaseOperator* next_;

public:
    ProjectJitComputeAllOperator(BaseOperator *next) :
        next_(next) {}

    ~ProjectJitComputeAllOperator() final {
        delete next_;
    }

    void open() {
        next_->open();
    }

    void close() {
        next_->close();
    }

    BatchResult* next() {
        BatchResult *br = next_->next();
        if (br == nullptr)
            return br;

        uint32_t n = br->getn();
        DbVector<int32_t> *res = new DbVector<int32_t>(n);
        int32_t *tax = br->getCol("tax")->col;
        int32_t *discount = br->getCol("discount")->col;
        int32_t *extprice = br->getCol("extprice")->col;
        int32_t *resvec = res->col;
        for (uint32_t i = 0; i < n; i++) {
            resvec[i] = extprice[i] * (100 - discount[i]) * (100 + tax[i]);
        }

        br->add("price", res);
        br->remove("tax");
        br->remove("extprice");
        br->remove("discount");

        return br;
    }
};


class ProjectJitNonComputeAllOperator : public BaseOperator {
private:
    BaseOperator* next_;

public:
    ProjectJitNonComputeAllOperator(BaseOperator *next) :
            next_(next) {}

    ~ProjectJitNonComputeAllOperator() final {
        delete next_;
    }

    void open() {
        next_->open();
    }

    void close() {
        next_->close();
    }

    BatchResult* next() {
        BatchResult *br = next_->next();
        if (br == nullptr)
            return br;

        uint32_t *ressel = br->res_sel->col;
        uint32_t n1 = br->res_sel->n;
        DbVector<int32_t> *res = new DbVector<int32_t>(n1);
        int32_t *resvec = res->col;

        int32_t *tax = br->getCol("tax")->col;
        int32_t *discount = br->getCol("discount")->col;
        int32_t *extprice = br->getCol("extprice")->col;

        for (uint32_t i = 0; i < n1; i++) {
            resvec[i] = extprice[ressel[i]] * (100 - discount[ressel[i]]) * (100 + tax[ressel[i]]);
        }

        br->add("price", res);

        br->res_sel = nullptr;
        delete ressel;
        br->remove("tax");
        br->remove("extprice");
        br->remove("discount");

        return br;
    }
};


/************************************************************************
 *
 * Query compiler
 *
 **************************************************************************/


QueryPlan *compileQuery_Baseline() {
    std::vector<std::string> col_names{"extprice", "discount", "tax"};
    ScanOperator *scan_op = new ScanOperator(BATCHES, col_names, true, 100);
    return new QueryPlan(scan_op, false);
}


QueryPlan *compileQuery_ComputeAll() {
    std::vector<std::string> col_names{"extprice", "discount", "tax"};
    ScanOperator *scan_op = new ScanOperator(BATCHES, col_names, true, 100);
    std::vector<CondDAGNode*> expr{};
    expr.push_back(new ColValCondDAGNode(COND_LT, "tax", 90));

    auto sel_op = new SelectVectorizationOnlyNonBranchingOperator(scan_op, expr);
    auto proj_op = new ProjectJitComputeAllOperator(sel_op);
    return new QueryPlan(proj_op, false);
}


QueryPlan *compileQuery_NonComputeAll() {
    std::vector<std::string> col_names{"extprice", "discount", "tax"};
    ScanOperator *scan_op = new ScanOperator(BATCHES, col_names, true, 100);
    std::vector<CondDAGNode*> expr{};
    expr.push_back(new ColValCondDAGNode(COND_LT, "tax", 90));

    auto sel_op = new SelectVectorizationOnlyNonBranchingOperator(scan_op, expr);
    auto proj_op = new ProjectJitNonComputeAllOperator(sel_op);
    return new QueryPlan(proj_op, false);
}


int main(int argc, char **argv) {
    QueryPlan *query_plan = compileQuery_NonComputeAll();
    query_plan->open();
    query_plan->printResultSet();
    query_plan->close();
    delete query_plan;
}