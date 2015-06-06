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
    friend class QL_Condition;
    friend class QL_Optimizer;
    friend class QL_Projection;
    friend class EX_Sorter;
    friend class EX_MergeJoin;
    friend class EX_Optimizer;
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
    static bool isValidAttr(char* attrName, const std::vector<DataAttrInfo> &attributes);
    static int indexToUse(int nConditions, const Condition conditions[], 
                const std::vector<DataAttrInfo> &attributes);
    static void buffer(void* ptr, char* buff, int len);
    static bool eq_op(void* attr1, void* attr2, int len1, int len2, AttrType type);
    static bool ne_op(void* attr1, void* attr2, int len1, int len2, AttrType type);
    static bool lt_op(void* attr1, void* attr2, int len1, int len2, AttrType type);
    static bool gt_op(void* attr1, void* attr2, int len1, int len2, AttrType type);
    static bool ge_op(void* attr1, void* attr2, int len1, int len2, AttrType type);
    static bool le_op(void* attr1, void* attr2, int len1, int len2, AttrType type);
    static bool evalCondition(void* data, const Condition &cond, 
                const std::vector<DataAttrInfo> &attributes);
    static bool isValidCondition(const Condition &cond, 
                const std::vector<DataAttrInfo> &attributes);
    static int findAttr(const char* relName, const char *attrName, 
                const std::vector<DataAttrInfo> &attributes);
    void printPlanHeader(const char *operation, const char* relname);
    void printPlanFooter();
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

#define QL_INSERT_WARN                (START_QL_WARN + 0)
#define QL_CAT_WARN                   (START_QL_WARN + 1)
#define QL_INVALID_WARN               (START_QL_WARN + 2)
#define QL_DELETE_WARN                (START_QL_WARN + 3)
#define QL_FILESCAN_WARN              (START_QL_WARN + 4)
#define QL_EOF                        (START_QL_WARN + 5)
#define QL_IXSCAN_WARN                (START_QL_WARN + 6)
#define QL_UPDATE_WARN                (START_QL_WARN + 7)
#define QL_COND_WARN                  (START_QL_WARN + 8)
#define QL_SELECT_WARN                (START_QL_WARN + 9)
#define QL_CROSS_WARN                 (START_QL_WARN + 10)
#define QL_PROJ_WARN                  (START_QL_WARN + 11)
#define QL_PERMDUP_WARN               (START_QL_WARN + 12)
#define QL_LASTWARN                    QL_PERMDUP_WARN

#define QL_INSERT_ERR                (START_QL_ERR - 0)
#define QL_DELETE_ERR                (START_QL_ERR - 1)
#define QL_FILESCAN_ERR              (START_QL_ERR - 2)
#define QL_IXSCAN_ERR                (START_QL_ERR - 3)
#define QL_UPDATE_ERR                (START_QL_ERR - 4)
#define QL_COND_ERR                  (START_QL_ERR - 5)
#define QL_SELECT_ERR                (START_QL_ERR - 6)
#define QL_CROSS_ERR                 (START_QL_ERR - 7)
#define QL_PROJ_ERR                  (START_QL_ERR - 8)
#define QL_PERMDUP_ERR               (START_QL_ERR - 9)
#define QL_LASTERROR                  QL_PERMDUP_ERR

#endif
