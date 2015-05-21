//
// ql_manager_stub.cc
//

// Note that for the SM component (HW3) the QL is actually a
// simple stub that will allow everything to compile.  Without
// a QL stub, we would need two parsers.

#include <sys/times.h>
#include <sys/types.h>
#include <cassert>
#include <unistd.h>
#include <memory>
#include "redbase.h"
#include "ql.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "parser.h"
#include "ql_internal.h"

using namespace std;

//
// QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm)
//
// Constructor for the QL Manager
//
QL_Manager::QL_Manager(SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm) {
    this->smm = &smm;
    this->ixm = &ixm;
    this->rmm = &rmm;
}

//
// QL_Manager::~QL_Manager()
//
// Destructor for the QL Manager
//
QL_Manager::~QL_Manager() {
}

//
// Handle the select clause
//
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[]) {
    /* 
    int i;
    cout << "Select\n";
    cout << "   nSelAttrs = " << nSelAttrs << "\n";
    for (i = 0; i < nSelAttrs; i++)
        cout << "   selAttrs[" << i << "]:" << selAttrs[i] << "\n";
    cout << "   nRelations = " << nRelations << "\n";
    for (i = 0; i < nRelations; i++)
        cout << "   relations[" << i << "] " << relations[i] << "\n";
    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";
    */


    return OK_RC;
}

/*  Insert the values into relName
    Steps-
    1. Check if the relation exists
    2. Check if the passed values match in type
    3. Insert the values into the relation
    4. Insert the values into each index
*/
RC QL_Manager::Insert(const char *relName,
                      int nValues, const Value values[]) {
    /* 
    int i;
    cout << "Insert\n";
    cout << "   relName = " << relName << "\n";
    cout << "   nValues = " << nValues << "\n";
    for (i = 0; i < nValues; i++)
        cout << "   values[" << i << "]:" << values[i] << "\n";
    */
    RC WARN = QL_INSERT_WARN, ERR = QL_INSERT_ERR;
    if ((strcmp(relName, "relcat") == 0)
        || (strcmp(relName, "attrcat") == 0)) {
            return QL_CAT_WARN;
    }
    RelationInfo relation;
    vector<DataAttrInfo> attributes;
    QL_ErrorForward(smm->getRelation(relName, relation));
    QL_ErrorForward(smm->getAttributes(relName, attributes));
    if (nValues != (int) attributes.size()) return QL_INVALID_WARN;
    for (unsigned int i = 0; i < attributes.size(); i++) {
        if (attributes[i].attrType != values[i].type) return QL_INVALID_WARN;
    } 
    // safe to insert now
    char *buffer = new char[relation.tuple_size];
    for (unsigned int i = 0, offset = 0; i < attributes.size(); i++) {
        memcpy(buffer+offset, values[i].data, attributes[i].attrLength);
        offset += attributes[i].attrLength;
    }
    // open the file and index handles
    RM_FileHandle relh;
    vector<int> ind;
    vector<IX_IndexHandle> ihandles(attributes.size());
    QL_ErrorForward(rmm->OpenFile(relName, relh));
    for (size_t i = 0; i < attributes.size(); i++) {
        if (attributes[i].indexNo >= 0) {
            ind.push_back(i);
            QL_ErrorForward(ixm->OpenIndex(relName, 
                attributes[i].indexNo, ihandles[i]));
        }
    }    
    // insert the record into file and indices
    RID record_rid;
    QL_ErrorForward(relh.InsertRec(buffer, record_rid));
    for (size_t i = 0; i < ind.size(); i++) {
        QL_ErrorForward(ihandles[ind[i]].InsertEntry( (void*) 
            (buffer + attributes[ind[i]].offset), record_rid));
    }
    // close the relation and index files
    SM_ErrorForward(rmm->CloseFile(relh));
    for (size_t i = 0; i < ind.size(); i++) {
        SM_ErrorForward(ixm->CloseIndex(ihandles[ind[i]]));
    }
    // print the tuple that was inserted
    DataAttrInfo* attrs = &attributes[0];
    Printer p(attrs, relation.num_attr);
    p.PrintHeader(cout);
    p.Print(cout, buffer);
    p.PrintFooter(cout);
    delete[] buffer;
    return OK_RC;
}

/*  Delete from the relName all tuples that satisfy conditions
    1. If no condition use a file scan
    2. If it is a range operator, use file scan (to be improved by
        using statistics to check if indexscan would be cheaper)
    3. If no index exists, use a file scan
    4. If index exists, operator is equality, use index scan
    #TODO - Sort conditions based on selectivity
*/
RC QL_Manager::Delete(const char *relName,
                      int nConditions, const Condition conditions[]) {
    /*
    int i;
    cout << "Delete\n";
    cout << "   relName = " << relName << "\n";
    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";
    */
    RC WARN = QL_DELETE_WARN, ERR = QL_DELETE_ERR;
    // validate the inputs
    if ((strcmp(relName, "relcat") == 0)
        || (strcmp(relName, "attrcat") == 0)) {
            return QL_CAT_WARN;
    }
    RelationInfo relation;
    vector<DataAttrInfo> attributes;
    QL_ErrorForward(smm->getRelation(relName, relation));
    QL_ErrorForward(smm->getAttributes(relName, attributes));
    for (int i = 0; i < nConditions; i++) {
        if (!isValidCondition(conditions[i], attributes)) 
            return QL_INVALID_WARN;
    }
    // if there are no conditions, delete the relation and create again
    if (nConditions == 0) {
        AttrInfo *attrs = new AttrInfo[attributes.size()];
        vector<int> indexedAttrs;
        for (unsigned int i = 0 ; i < attributes.size(); i++) {
            attrs[i].attrName = attributes[i].attrName;
            attrs[i].attrType = attributes[i].attrType;
            attrs[i].attrLength = attributes[i].attrLength;
            if (attributes[i].indexNo >= 0) indexedAttrs.push_back(i);
        }
        QL_ErrorForward(smm->DropTable(relName));
        QL_ErrorForward(smm->CreateTable(relName, attributes.size(), attrs));
        for (unsigned int i = 0; i < indexedAttrs.size(); i++) {
            QL_ErrorForward(smm->CreateIndex(relName, 
                attributes[indexedAttrs[i]].attrName));
        }
        delete[] attrs;
        return OK_RC;
    }
    // open the file and index handles
    RM_FileHandle relh;
    vector<int> ind;
    vector<IX_IndexHandle> ihandles(attributes.size());
    QL_ErrorForward(rmm->OpenFile(relName, relh));
    for (size_t i = 0; i < attributes.size(); i++) {
        if (attributes[i].indexNo >= 0) {
            ind.push_back(i);
            QL_ErrorForward(ixm->OpenIndex(relName, 
                attributes[i].indexNo, ihandles[i]));
        }
    }
    // find if an index scan is needed
    int idxno = indexToUse(nConditions, conditions, attributes);
    shared_ptr<QL_Op> scanner;
    if (idxno < 0) {
        // use file scan based on first condition having rhs value
        DataAttrInfo* dinfo;
        void *value = 0;
        CompOp cmp;
        bool found = false;
        for (int i = 0; i < nConditions; i++) {
            if (!conditions[i].bRhsIsAttr) {
                found = true;
                dinfo = findAttr(conditions[i].lhsAttr.attrName, attributes);
                value = conditions[i].rhsValue.data;
                cmp = conditions[i].op;
                break;
            }
        }
        if (!found) {
            dinfo = &attributes[0];
            cmp = NO_OP;
        }
        scanner.reset(new QL_FileScan(relh, dinfo->attrType, dinfo->attrLength,
            dinfo->offset, cmp, value, NO_HINT, attributes));
    }
    else {
        // use index scan
        scanner = 0;
    }

    RID rid;
    shared_ptr<char> data(new char[attributes.back().offset + attributes.back().attrLength]);
    bool isValid = false;
    QL_ErrorForward(scanner->Open());
    while (scanner->Next(data, rid) == OK_RC) {
        isValid = true;
        for (int i = 0; i < nConditions; i++) {
            if (!evalCondition((void*) data.get(), conditions[i], attributes)) {
                isValid = false;
                break;
            }
        }
        if (isValid) {
            QL_ErrorForward(relh.DeleteRec(rid));
            for (unsigned int i = 0; i < ind.size(); i++) {
                QL_ErrorForward(ihandles[ind[i]].DeleteEntry(
                    (void*) (data.get() + attributes[ind[i]].offset), rid));
            }
        }
    }
    SM_ErrorForward(scanner->Close());
    // close the relation and index files
    SM_ErrorForward(rmm->CloseFile(relh));
    for (size_t i = 0; i < ind.size(); i++) {
        SM_ErrorForward(ixm->CloseIndex(ihandles[ind[i]]));
    }
    return OK_RC;
}


//
// Update from the relName all tuples that satisfy conditions
//
RC QL_Manager::Update(const char *relName,
                      const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue,
                      int nConditions, const Condition conditions[])
{
    int i;

    cout << "Update\n";

    cout << "   relName = " << relName << "\n";
    cout << "   updAttr:" << updAttr << "\n";
    if (bIsValue)
        cout << "   rhs is value: " << rhsValue << "\n";
    else
        cout << "   rhs is attribute: " << rhsRelAttr << "\n";

    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    return 0;
}

//
// void QL_PrintError(RC rc)
//
// This function will accept an Error code and output the appropriate
// error.
//
void QL_PrintError(RC rc)
{
    cout << "QL_PrintError\n   rc=" << rc << "\n";
}



//
// Private methods
// 

#define QL_operator(expr) do { \
    char attr_value1[len1+1];\
    buffer(attr1, attr_value1, len1);\
    char attr_value2[len2+1];\
    buffer(attr2, attr_value2, len2);\
    switch (type) { \
        case INT:\
            return *((int*) attr_value1) expr *((int*) attr_value2);\
        case FLOAT: \
            return *((float*) attr_value1) expr *((float*) attr_value2);\
        case STRING: \
            return string(attr_value1) expr string(attr_value2);\
        default: return 1 == 0;\
    }\
}while(0)

// copies the contents into char array and null terminates it
void QL_Manager::buffer(void *ptr, char* buff, int len) const{
    buff[len] = '\0';
    memcpy(buff, ptr, len);
    return;
}

// operators for comparison
bool QL_Manager::eq_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) const{
    QL_operator(==);
}
bool QL_Manager::ne_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) const{
    QL_operator(!=);
}
bool QL_Manager::lt_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) const{
    QL_operator(<);
}
bool QL_Manager::gt_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) const{
    QL_operator(>);
}
bool QL_Manager::le_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) const{
    QL_operator(<=);    
}
bool QL_Manager::ge_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) const{
    QL_operator(>=);
}


bool QL_Manager::isValidAttr(char* attrName, 
                    const vector<DataAttrInfo> &attributes) {
    for (unsigned int i = 0; i < attributes.size(); i++) {
        if (strcmp(attrName, attributes[i].attrName) == 0) return true;
    }
    return false;
}


/*  Given a set of conditions, checks which attributes have index and then
    finds the condition for which index should be used.
    Current implementation - If any equality condition is present
    whose rhs is a value and the attribute is indexed ,then the index is
    used, otherwise file scan is used.
*/ 
int QL_Manager::indexToUse(int nConditions, const Condition conditions[], 
                const vector<DataAttrInfo> &attributes) {
    for (int i = 0; i < nConditions; i++) {
        if (conditions[i].op != EQ_OP) continue;
        if (conditions[i].bRhsIsAttr) continue;
        for (unsigned int j = 0; j < attributes.size(); j++) {
            if (strcmp(conditions[i].lhsAttr.attrName, 
                    attributes[j].attrName) == 0) {
                if (attributes[j].indexNo >= 0) return i;
            }
        }
    }
    return -1;
}

/*  Checks if a condition is valid by matching the data-types of rhs
    and lhs
*/
bool QL_Manager::isValidCondition(const Condition &cond, 
                    const vector<DataAttrInfo> &attributes) {
    AttrType lhs = INT, rhs = FLOAT;
    bool foundl = false, foundr = (cond.bRhsIsAttr == 0);
    for (unsigned int i = 0; i < attributes.size(); i++) {
        if (strcmp(cond.lhsAttr.attrName, attributes[i].attrName) == 0) {
            lhs = attributes[i].attrType;
            foundl = true;
        }
        if (cond.bRhsIsAttr) {
            if (strcmp(cond.rhsAttr.attrName, attributes[i].attrName) == 0) {
                rhs = attributes[i].attrType;
                foundr = true;
            }
        }
    }
    if (!cond.bRhsIsAttr) {
        rhs = cond.rhsValue.type;
    }
    return (lhs == rhs && foundl && foundr);
}


/*  Check if a record satisfies a certain condition
*/
bool QL_Manager::evalCondition(void* data, const Condition &cond, 
                    const vector<DataAttrInfo> &attributes) {
    // define pointer to appropriate member function
    bool (QL_Manager::*comp)(void* attr1, void* attr2, int len1, 
                                    int len2, AttrType type) const;
    switch (cond.op) {
        case NE_OP:
            comp = &QL_Manager::ne_op;
            break;
        case EQ_OP:
            comp = &QL_Manager::eq_op;
            break;
        case LT_OP:
            comp = &QL_Manager::lt_op;
            break;
        case GT_OP:
            comp = &QL_Manager::gt_op;
            break;
        case LE_OP:
            comp = &QL_Manager::le_op;
            break;
        case GE_OP:
            comp = &QL_Manager::ge_op;
            break;
        default:
            return false;
            break;
    }
    void *attr1 = 0, *attr2 = 0;
    int len1 = 4, len2 = 4;
    AttrType type = INT;
    for (unsigned int i = 0; i < attributes.size(); i++) {
        if (strcmp(cond.lhsAttr.attrName, attributes[i].attrName) == 0) {
            attr1 = (void*) ((char*) data + attributes[i].offset);
            len1 = attributes[i].attrLength;
            type = attributes[i].attrType;
        }
        if (cond.bRhsIsAttr) {
            if (strcmp(cond.rhsAttr.attrName, attributes[i].attrName) == 0) {
                attr2 = (void*) ((char*) data + attributes[i].offset);
                len2 = attributes[i].attrLength;
            }
        }
    }
    if (!cond.bRhsIsAttr) {
        attr2 = cond.rhsValue.data;
        len2 = len1;
    }

    return (this->*comp)(attr1, attr2, len1, len2, type);
}

/*  Find attribute info by attribute name 
*/
DataAttrInfo* QL_Manager::findAttr(char* attrName, 
                    vector<DataAttrInfo> &attributes) {
    for (unsigned int i = 0; i < attributes.size(); i++) {
        if (strcmp(attrName, attributes[i].attrName) == 0) {
            return &attributes[i];
        }
    }
    return &attributes[0];
}