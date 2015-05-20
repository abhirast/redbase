//
// ql.h
//   Query Language Component Interface
//

// This file only gives the stub for the QL component

#ifndef QL_H
#define QL_H

#include <stdlib.h>
#include <string.h>
#include <cstdio>
#include <vector>
#include <iostream>
#include "redbase.h"
#include "parser.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"

//
// QL_Manager: query language (DML)
//
class QL_Manager {
public:
    QL_Manager (SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm);
    ~QL_Manager();                       // Destructor

    RC Select  (int nSelAttrs,           // # attrs in select clause
        const RelAttr selAttrs[],        // attrs in select clause
        int   nRelations,                // # relations in from clause
        const char * const relations[],  // relations in from clause
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Insert  (const char *relName,     // relation to insert into
        int   nValues,                   // # values
        const Value values[]);           // values to insert

    RC Delete  (const char *relName,     // relation to delete from
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

    RC Update  (const char *relName,     // relation to update
        const RelAttr &updAttr,          // attribute to update
        const int bIsValue,              // 1 if RHS is a value, 0 if attribute
        const RelAttr &rhsRelAttr,       // attr on RHS to set LHS equal to
        const Value &rhsValue,           // or value to set attr equal to
        int   nConditions,               // # conditions in where clause
        const Condition conditions[]);   // conditions in where clause

private:
    RM_Manager* rmm;
    IX_Manager* ixm;
    SM_Manager* smm;
    bool isValidAttr(char* attrName, const std::vector<DataAttrInfo> &attributes);
    int indexToUse(int nConditions, const Condition conditions[], 
                const std::vector<DataAttrInfo> &attributes);
    void buffer(void* ptr, char* buff, int len) const;
    bool eq_op(void* attr1, void* attr2, int len1, int len2, AttrType type) const;
    bool ne_op(void* attr1, void* attr2, int len1, int len2, AttrType type) const;
    bool lt_op(void* attr1, void* attr2, int len1, int len2, AttrType type) const;
    bool gt_op(void* attr1, void* attr2, int len1, int len2, AttrType type) const;
    bool ge_op(void* attr1, void* attr2, int len1, int len2, AttrType type) const;
    bool le_op(void* attr1, void* attr2, int len1, int len2, AttrType type) const;
    bool evalCondition(void* data, const Condition &cond, 
                const std::vector<DataAttrInfo> &attributes);
    bool isValidCondition(const Condition &cond, 
                const std::vector<DataAttrInfo> &attributes);
    DataAttrInfo* findAttr(char *attrName, 
                std::vector<DataAttrInfo> &attributes);
};



class QL_Op {
public:
    std::vector<DataAttrInfo> attributes;
protected:
    QL_Op* parent;
    virtual RC Open() = 0;
    virtual RC Next(char *&rec) = 0;
    virtual RC Close() = 0; 
};



class QL_UnaryOp : public QL_Op {
protected:
    QL_Op* child;
};


class QL_BinaryOp : public QL_Op {
public:
    QL_BinaryOp(QL_Op &left, QL_Op &right) {
        lchild = &left;
        rchild = &right;
    }
protected:
    QL_Op* lchild;
    QL_Op* rchild;
};

// Macro for error forwarding
// WARN and ERR to be defined in the context where macro is used
#define QL_ErrorForward(expr) do { \
RC tmp_rc = (expr);\
if (tmp_rc != OK_RC) \
    return ((tmp_rc > 0) ? WARN : ERR); \
} while (0)


//
// Print-error function
//
void QL_PrintError(RC rc);

// Error codes

#define QL_INSERT_WARN                400
#define QL_CAT_WARN                   401
#define QL_INVALID_WARN               402
#define QL_DELETE_WARN                403






#define QL_INSERT_ERR                -400
#define QL_DELETE_ERR                -400


#endif
