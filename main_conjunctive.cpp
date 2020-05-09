#include <iostream>
#include "common.h"

/**
 * This program evaluates the performance of a pure conjunctive selection query.
 *
 * Assume there is a table lineitem
 *
 * create table lineitem (
 *     discount int32,
 *     tax int32,
 *     extprice int32
 * )
 *
 * and a query
 *
 *   select * from lineimte
 *   where discount < 50 and tax < 50 and extprice < 50
 *
 * the values of discount, tax and extprice are evenly distributed between 1 and 100 so that
 * we can control the selectivity by adjusting the operand of the comparison operator (e.g. 50
 * means the selectivity of 0.5; and 60 means the selectivity of 0.6).
 *
 * the program evaluated the following strategies:
 *   vectorization-only, branching
 *   vectorization-only, non-branching (see
 *       Ross, Kenneth A. "Conjunctive selection conditions in main memory." Proceedings of
 *       the twenty-first ACM SIGMOD-SIGACT-SIGART symposium on Principles of database systems. 2002.)
 *   jit, branching
 *   jit, non-branching
 *   jit, if (1 && 2), 3 non-branching
 *
 */


const uint32_t BATCHES = 100000;


static uint32_t sel_lt_int32_col_int32_val_branching(uint32_t n,
                                                     uint32_t *res_sel,
                                                     int32_t *col,
                                                     int32_t val,
                                                     uint32_t *sel) {
    uint32_t res = 0;

    if (sel != nullptr) {
        for (uint32_t i = 0; i < n; i++) {
            if (col[sel[i]] < val)
                res_sel[res++] = sel[i];
        }
    }
    else {
        for (uint32_t i = 0; i < n; i++) {
            if (col[i] < val)
                res_sel[res++] = i;
        }
    }

    return res;
}


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
    ColValCondDAGNode(int cond, std::string left_col_name, int32_t right_val, bool branching) :
        CondDAGNode(cond),
        left_col_name_(std::move(left_col_name)),
        branching_(branching),
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
        if (branching_) {
            primitive_ = sel_lt_int32_col_int32_val_branching;
        }
        else {
            primitive_ = sel_lt_int32_col_int32_val_nonbranching;
        }
    }
};



class SelectVectorizationOnlyBranchingOperator : public BaseOperator {
private:
    BaseOperator* next_;
    std::vector<CondDAGNode*> expr_;

public:
    SelectVectorizationOnlyBranchingOperator(BaseOperator *next, std::vector<CondDAGNode*> expr) :
        next_(next), expr_(std::move(expr)) {
    }

    ~SelectVectorizationOnlyBranchingOperator() final {
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


class SelectJitOperator : public BaseOperator {
private:
    BaseOperator* next_;
    bool branching_;

public:
    SelectJitOperator(BaseOperator *next, bool branching) :
        next_(next), branching_(branching) {
    }

    ~SelectJitOperator() final {
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
        int32_t *tax = br->getCol("tax")->col;
        int32_t *discount = br->getCol("discount")->col;
        int32_t *extprice = br->getCol("extprice")->col;

        DbVector<uint32_t> *res_sel = new DbVector<uint32_t>(n);
        auto res_sel_col = res_sel->col;
        uint32_t res = 0;
        if (branching_) {
            for (uint32_t i = 0; i < n; i++) {
                if (tax[i] < 50 && discount[i] < 50 && extprice[i] < 50)
                    res_sel_col[res++] = i;
            }
        }
        else {
            for (uint32_t i = 0; i < n; i++) {
                res_sel_col[res] = i;
                res += (tax[i] < 50 && discount[i] < 50 && extprice[i] < 50);
            }
        }
        res_sel->n = res;
        br->res_sel = res_sel;

        // std::cout << "n=" << res_sel->n << ", capacity=" << res_sel->capacity << ", col=" << (uint64_t)res_sel->col << "\n";
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


QueryPlan *compileQuery_VectorizationOnly_Branching() {
    std::vector<std::string> col_names{"extprice", "discount", "tax"};
    ScanOperator *scan_op = new ScanOperator(BATCHES, col_names, true, 100);
    std::vector<CondDAGNode*> expr{};
    expr.push_back(new ColValCondDAGNode(COND_LT, "extprice", 50, true));
    expr.push_back(new ColValCondDAGNode(COND_LT, "discount", 50, true));
    expr.push_back(new ColValCondDAGNode(COND_LT, "tax", 50, true));

    SelectVectorizationOnlyBranchingOperator *sel_op = new SelectVectorizationOnlyBranchingOperator(scan_op, expr);
    return new QueryPlan(sel_op, false);
}


QueryPlan *compileQuery_VectorizationOnly_NonBranching() {
    std::vector<std::string> col_names{"extprice", "discount", "tax"};
    ScanOperator *scan_op = new ScanOperator(BATCHES, col_names, true, 100);
    std::vector<CondDAGNode*> expr{};
    expr.push_back(new ColValCondDAGNode(COND_LT, "extprice", 50, false));
    expr.push_back(new ColValCondDAGNode(COND_LT, "discount", 50, false));
    expr.push_back(new ColValCondDAGNode(COND_LT, "tax", 50, false));

    SelectVectorizationOnlyBranchingOperator *sel_op = new SelectVectorizationOnlyBranchingOperator(scan_op, expr);
    return new QueryPlan(sel_op, false);
}


QueryPlan *compileQuery_JIT_Branching() {
    std::vector<std::string> col_names{"extprice", "discount", "tax"};
    ScanOperator *scan_op = new ScanOperator(BATCHES, col_names, true, 100);
    SelectJitOperator *sel_op = new SelectJitOperator(scan_op, true);
    return new QueryPlan(sel_op, false);
}


QueryPlan *compileQuery_JIT_NonBranching() {
    std::vector<std::string> col_names{"extprice", "discount", "tax"};
    ScanOperator *scan_op = new ScanOperator(BATCHES, col_names, true, 100);
    SelectJitOperator *sel_op = new SelectJitOperator(scan_op, false);
    return new QueryPlan(sel_op, false);
}


int main(int argc, char*argv[]) {
    QueryPlan *query_plan = compileQuery_JIT_NonBranching();
    query_plan->open();
    query_plan->printResultSet();
    query_plan->close();
    delete query_plan;
}