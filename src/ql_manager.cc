//
// ql_manager.cc
//


#include <sys/times.h>
#include <sys/types.h>
#include <cassert>
#include <unistd.h>
#include <memory>
#include <sstream>
#include "redbase.h"
#include "ql.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "parser.h"
#include "ql_internal.h"
#include "ex.h"

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

/*
    Handle the select clause
    Construct a naive operator tree and then optimize it using a set of
    heuristics
*/
RC QL_Manager::Select(int nSelAttrs, const RelAttr selAttrs[],
                      int nRelations, const char * const relations[],
                      int nConditions, const Condition conditions[]) {
    RC WARN = QL_SELECT_WARN, ERR = QL_SELECT_ERR;
    // Validate the inputs
    // check if relations are valid and distinct
    vector<vector<DataAttrInfo>> attributes(nRelations);
    vector<DataAttrInfo> allAttributes;
    for (int i = 0; i < nRelations; i++) {
        QL_ErrorForward(smm->getAttributes(relations[i], attributes[i]));
        for (int j = 0; j < i; j++) {
            if (strcmp(relations[i],relations[j]) == 0) return QL_INVALID_WARN;
        }
        allAttributes.insert(allAttributes.end(), attributes[i].begin(), 
            attributes[i].end());
    }
    // check if selAttrs are valid
    // relNames
    if (strcmp(selAttrs[0].attrName, "*") != 0) {
        for (int i = 0; i < nSelAttrs; i++) {
            if (findAttr(selAttrs[i].relName, selAttrs[i].attrName, 
                allAttributes) < 0) return QL_INVALID_WARN;
        }
    }
    // check if conditions are valid and involve 'from' clause relations
    for (int i = 0; i < nConditions; i++) {
        if (!isValidCondition(conditions[i], allAttributes))
            return QL_INVALID_WARN;
    }
    // collect ambiguous attributes, an attribute is ambiguous if
    // it appears in two relations in the 'from' clause 
    vector<char*> ambiguous;
    for (unsigned int i = 0; i < allAttributes.size(); i++) {
        for (unsigned int j = 0; j < i; j++) {
            if (strcmp(allAttributes[i].attrName, 
                allAttributes[j].attrName) == 0)
                    ambiguous.push_back(allAttributes[i].attrName);
        }
    }
    // check if select and where clause have any ambiguous attributes
    // without a relName
    for (int i = 0; i < nSelAttrs; i++) {
        if (selAttrs[i].relName == 0) {
            for (unsigned int j = 0; j < ambiguous.size(); j++) {
                if (strcmp(selAttrs[i].attrName, ambiguous[j]) == 0)
                    return QL_INVALID_WARN;
            }
        }
    }
    for (int i = 0; i < nConditions; i++) {
        if (conditions[i].lhsAttr.relName == 0) {
            for (unsigned int j = 0; j < ambiguous.size(); j++) {
                if (strcmp(conditions[i].lhsAttr.attrName, ambiguous[j]) == 0)
                    return QL_INVALID_WARN;
            }
        }
        if (conditions[i].bRhsIsAttr && conditions[i].rhsAttr.relName == 0) {
            for (unsigned int j = 0; j < ambiguous.size(); j++) {
                if (strcmp(conditions[i].rhsAttr.attrName, ambiguous[j]) == 0)
                    return QL_INVALID_WARN;
            }
        }
    }
    /************************************
    Define the operator tree
    *************************************/
    // define a filescan op for each relation
    vector<QL_Op*> opTree;
    for (int i = 0; i < nRelations; i++) {
        QL_Op* node = new QL_FileScan(rmm, ixm, relations[i], attributes[i]);
        opTree.push_back(node);
    }
    
    // do the cross product of relations
    QL_Op* xnode;
    if (nRelations > 1) {
        xnode = new QL_Cross(*opTree[0], *opTree[1]);
        opTree.push_back(xnode);
    }
    for (int i = 2; i < nRelations; i++) {
        xnode = new QL_Cross(*xnode, *opTree[i]);
        opTree.push_back(xnode);
    }

    // define one filter for each condition
    QL_Op* cnode = opTree.back();
    for (int i = 0; i < nConditions; i++) {
        cnode = new QL_Condition(*cnode, &conditions[i], 
            cnode->attributes);
        opTree.push_back(cnode);
    }

    // add the projection
    if (strcmp(selAttrs[0].attrName, "*") != 0) {
        QL_Op* proj = new QL_Projection(*(opTree.back()), nSelAttrs, 
            &selAttrs[0], opTree.back()->attributes);
        opTree.push_back(proj);
        
        // add a permutation / duplication operator if needed
        int numFound = 0;
        vector<DataAttrInfo> temp = opTree.back()->attributes;
        for (unsigned int i = 0; i < temp.size(); i++) {
            if (strcmp(temp[i].attrName, selAttrs[numFound].attrName) == 0 && 
                (selAttrs[numFound].relName == 0 ||
                strcmp(temp[i].relName, selAttrs[numFound].relName) == 0)) {
                    numFound ++;
            }
        }
        if (numFound < nSelAttrs) {
            QL_Op *permdup = new QL_PermDup(*(opTree.back()), nSelAttrs, 
                &selAttrs[0], opTree.back()->attributes);
            opTree.push_back(permdup);
        }
    }
    /************************************
        Optimize the query plan
    *************************************/
    QL_Op* root = opTree.back();
    

    //////////////////////////////////////////////
    // sorting test
    // vector<char> data;
    // EX_Sorter sorter(*(this->rmm->pf_manager), *root, 0);
    // char* fname = "abhinav";
    // cout << "error code " << sorter.sort(fname, 1.0) << endl;

    // EX_Scanner escn(*(*this->rmm).pf_manager);
    // char *fileName = "_abhinav.0";
    // escn.Open(fileName, 0, true, 154);
    // DataAttrInfo* attrs = &(root->attributes[0]);
    // Printer p(attrs, root->attributes.size());
    // p.PrintHeader(cout);
    // while (escn.Next(data) == OK_RC) {
    //     p.Print(cout, &data[0]);
    // }
    // p.PrintFooter(cout);
    

    ///////////////////////////////////////////////

    EX_Optimizer optimizer((*this->rmm).pf_manager);

    QL_Optimizer::pushCondition(root);
    QL_Optimizer::pushProjection(root);
    // print the result
    if (bQueryPlans) {
        printPlanHeader("SELECT", " ");
        printOperatorTree(root, 0);
        printPlanFooter();
    }
    QL_Optimizer::pushCondition(root);
    optimizer.mergeProjections(root);
    // optimizer.doSortMergeJoin(root);
    ///////////////////////////////////////
    // Sort operator test
    QL_Op* sort = new EX_Sort((*this->rmm).pf_manager, *root, 0);
    root = sort;
    if (bQueryPlans) {
        printPlanHeader("SELECT", " ");
        printOperatorTree(root, 0);
        printPlanFooter();
    }
    //////////////////////////////////////

    vector<char> data;
    QL_ErrorForward(root->Open());
    DataAttrInfo* attrs = &(root->attributes[0]);
    Printer p(attrs, root->attributes.size());
    p.PrintHeader(cout);
    while (root->Next(data) == OK_RC) {
        p.Print(cout, &data[0]);
    }
    p.PrintFooter(cout);
    SM_ErrorForward(root->Close());
    delete root;
    //////////////////////////////////////////////////////////////
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
    if (bQueryPlans) {
        printPlanHeader("DELETE FROM ", relName);
    }
    /* Can't do this because need to give feedback to user
    // if there are no conditions, delete the relation and create again
    if (nConditions == 0) {
        if (bQueryPlans) {
            cout<<"DELETING AND RECREATING RELATION AND INDEX(ES)"<<endl;
        }
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
    */
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
        int attrInd = 0;
        for (int i = 0; i < nConditions; i++) {
            if (!conditions[i].bRhsIsAttr) {
                found = true;
                attrInd = findAttr(0, conditions[i].lhsAttr.attrName, 
                    attributes);
                value = conditions[i].rhsValue.data;
                cmp = conditions[i].op;
                break;
            }
        }
        
        if (!found) cmp = NO_OP;
        
        dinfo = &attributes[attrInd];
        scanner.reset(new QL_FileScan(rmm, ixm, relName, attrInd,
            cmp, value, NO_HINT, attributes));
        if (bQueryPlans) {
            cout<<"FILE SCAN ON "<<dinfo->attrName<<endl;
        }
    }
    else {
        // use index scan
        CompOp cmp = conditions[idxno].op;
        int attrInd = findAttr(0, conditions[idxno].lhsAttr.attrName, 
                                    attributes);
        scanner.reset(new QL_IndexScan(rmm, ixm, relName, attrInd, cmp, 
            conditions[idxno].rhsValue.data, NO_HINT, attributes));
        if (bQueryPlans) {
            cout<<"INDEX SCAN ON "<<attributes[attrInd].attrName<<endl;
        }
    }
    if (bQueryPlans) printPlanFooter();
    RID rid;
    vector<char> data;
    bool isValid = false;
    QL_ErrorForward(scanner->Open());
    DataAttrInfo* attrs = &attributes[0];
    Printer p(attrs, relation.num_attr);
    p.PrintHeader(cout);
    while (scanner->Next(data, rid) == OK_RC) {
        isValid = true;
        for (int i = 0; i < nConditions; i++) {
            if (!evalCondition((void*) &data[0], conditions[i], attributes)) {
                isValid = false;
                break;
            }
        }
        if (isValid) {
            QL_ErrorForward(relh.DeleteRec(rid));
            for (unsigned int i = 0; i < ind.size(); i++) {
                QL_ErrorForward(ihandles[ind[i]].DeleteEntry(
                    (void*) &data[attributes[ind[i]].offset], rid));
            }
            p.Print(cout, &data[0]);
        }
    }
    p.PrintFooter(cout);
    SM_ErrorForward(scanner->Close());
    // close the relation and index files
    SM_ErrorForward(rmm->CloseFile(relh));
    for (size_t i = 0; i < ind.size(); i++) {
        SM_ErrorForward(ixm->CloseIndex(ihandles[ind[i]]));
    }
    return OK_RC;
}


/*
    Update from the relName all tuples that satisfy conditions
    Use index scan if there is an equlity condition having rhs as
    value on a non-update attribute. Else use filescan.
*/
RC QL_Manager::Update(const char *relName,
                      const RelAttr &updAttr,
                      const int bIsValue,
                      const RelAttr &rhsRelAttr,
                      const Value &rhsValue,
                      int nConditions, const Condition conditions[]) {
    RC WARN = QL_UPDATE_WARN, ERR = QL_UPDATE_ERR;
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
    int upInd = findAttr(0, updAttr.attrName, attributes);
    if (upInd < 0) return QL_INVALID_WARN;
    if (bIsValue) {
        if (attributes[upInd].attrType != rhsValue.type) 
            return QL_INVALID_WARN;
    }
    else {
        int j = findAttr(0, rhsRelAttr.attrName, attributes);
        if (j < 0) return QL_INVALID_WARN;
        if (attributes[upInd].attrType != attributes[j].attrType) 
            return QL_INVALID_WARN;
    }
    if (bQueryPlans) printPlanHeader("UPDATE ", relName);
    if (!bIsValue && strcmp(updAttr.attrName, rhsRelAttr.attrName) == 0) {
        if (bQueryPlans) {
            cout<<"TRIVIAL UPDATE, NO ACTION NEEDED"<<endl;
            printPlanFooter();
            return OK_RC;
        }
    }
    // Now all inputs are valid, set up the appropriate scan
    int indexCond = -1, attrIndex = -1;
    for (int i = 0; i < nConditions; i++) {
        if (!bIsValue) continue;
        if (strcmp(conditions[i].lhsAttr.attrName, updAttr.attrName) == 0)
            continue;
        if (conditions[i].op != EQ_OP) continue;
        attrIndex = findAttr(0, conditions[i].lhsAttr.attrName, attributes);
        if (attributes[attrIndex].indexNo >=0) {
            indexCond = i;
            break;
        }
    }

    shared_ptr<QL_Op> scanner;
    if (indexCond < 0) {
        // use file scan based on first condition having rhs value
        DataAttrInfo* dinfo;
        void *value = 0;
        CompOp cmp;
        bool found = false;
        int attrInd = 0;
        for (int i = 0; i < nConditions; i++) {
            if (!conditions[i].bRhsIsAttr) {
                found = true;
                attrInd = findAttr(0, conditions[i].lhsAttr.attrName, 
                    attributes);
                value = conditions[i].rhsValue.data;
                cmp = conditions[i].op;
                break;
            }
        }
        
        if (!found) cmp = NO_OP;
        dinfo = &attributes[attrInd];
        scanner.reset(new QL_FileScan(rmm, ixm, relName, attrInd, 
            cmp, value, NO_HINT, attributes));
        if (bQueryPlans) {
            cout<<"FILE SCAN ON "<<dinfo->attrName<<endl;
        }

    }
    else {
        // use index scan
        CompOp cmp = conditions[indexCond].op;
        scanner.reset(new QL_IndexScan(rmm, ixm, relName, attrIndex, cmp, 
            conditions[indexCond].rhsValue.data, NO_HINT, attributes));
        if (bQueryPlans) {
            cout<<"INDEX SCAN ON "<<
                conditions[indexCond].lhsAttr.attrName<<endl;
        }
    }
    if (bQueryPlans) printPlanFooter();
    // define and open the index handle for updated attribute if exists
    RM_FileHandle relh;
    QL_ErrorForward(rmm->OpenFile(relName, relh));
    IX_IndexHandle update_indh;
    if (attributes[upInd].indexNo >= 0) {
        QL_ErrorForward(ixm->OpenIndex(relName, 
                 attributes[upInd].indexNo, update_indh));
    }
    // fetch records and check if for records that satisfy all conditions
    RID rid;
    vector<char> data;
    bool isValid = false;
    QL_ErrorForward(scanner->Open());
    DataAttrInfo* attrs = &attributes[0];
    Printer p(attrs, relation.num_attr);
    p.PrintHeader(cout);
    while (scanner->Next(data, rid) == OK_RC) {
        isValid = true;
        for (int i = 0; i < nConditions; i++) {
            if (!evalCondition((void*) &data[0], conditions[i], attributes)) {
                isValid = false;
                break;
            }
        }
        if (isValid) {
            // the record satisfies all conditions, update it
            RM_Record updatedRec;
            updatedRec.rid = rid;
            updatedRec.record = new char[attributes.back().offset + 
                                            attributes.back().attrLength];
            memcpy(updatedRec.record, &data[0], attributes.back().offset + 
                                            attributes.back().attrLength);
            updatedRec.bIsAllocated = 1;

            void *updatePos = (void*) (updatedRec.record + 
                                    attributes[upInd].offset);
            // update the contents
            if (bIsValue) {
                memcpy(updatePos, rhsValue.data, attributes[upInd].attrLength);
            }
            else {
                int j = findAttr(0, rhsRelAttr.attrName, attributes);
                void *sourcePos = (void*) &data[attributes[j].offset];
                if (attributes[upInd].attrLength < attributes[j].attrLength) {
                    memcpy(updatePos, sourcePos, attributes[upInd].attrLength);
                } else {
                    memset(updatePos, 0, attributes[upInd].attrLength);
                    memcpy(updatePos, sourcePos, attributes[j].attrLength);
                }
            }
            // update the relation
            QL_ErrorForward(relh.UpdateRec(updatedRec));
            // update the index if exists
            if (attributes[upInd].indexNo >= 0) {
                QL_ErrorForward(update_indh.DeleteEntry((void*) 
                                    &data[attributes[upInd].offset], rid));
                QL_ErrorForward(update_indh.InsertEntry(updatePos, rid));
            }
            p.Print(cout, updatedRec.record);
        }
    }
    p.PrintFooter(cout);
    SM_ErrorForward(scanner->Close());
    // close the relation and index files
    SM_ErrorForward(rmm->CloseFile(relh));
    if (attributes[upInd].indexNo >= 0) {
        SM_ErrorForward(ixm->CloseIndex(update_indh));
    }
    return OK_RC;
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
void QL_Manager::buffer(void *ptr, char* buff, int len) {
    buff[len] = '\0';
    memcpy(buff, ptr, len);
    return;
}

// operators for comparison
bool QL_Manager::eq_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) {
    QL_operator(==);
}
bool QL_Manager::ne_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) {
    QL_operator(!=);
}
bool QL_Manager::lt_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) {
    QL_operator(<);
}
bool QL_Manager::gt_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) {
    QL_operator(>);
}
bool QL_Manager::le_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) {
    QL_operator(<=);    
}
bool QL_Manager::ge_op(void* attr1, void* attr2, int len1, 
                                int len2, AttrType type) {
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
        if (strcmp(cond.lhsAttr.attrName, attributes[i].attrName) == 0 &&
            (cond.lhsAttr.relName == 0 || 
                strcmp(cond.lhsAttr.relName, attributes[i].relName) == 0)
            ) {
            lhs = attributes[i].attrType;
            foundl = true;
        }
        if (cond.bRhsIsAttr) {
            if (strcmp(cond.rhsAttr.attrName, attributes[i].attrName) == 0 &&
                (cond.rhsAttr.relName == 0 || 
                strcmp(cond.rhsAttr.relName, attributes[i].relName) == 0)
                ) {
                rhs = attributes[i].attrType;
                foundr = true;
            }
        }
    }
    if (!cond.bRhsIsAttr) {
        rhs = cond.rhsValue.type;
    }
    // type coersion
    if (lhs != rhs && !cond.bRhsIsAttr) {
        Condition c = cond;
        if (lhs == INT) {
            c.rhsValue.type = lhs;
            int x;
            if (rhs == FLOAT) {
               x = (int) *((float*) c.rhsValue.data);
            } else if (rhs == STRING) {
               x = atoi((char*) c.rhsValue.data);
            }
            memcpy(c.rhsValue.data, &x, 4);
            rhs = lhs;
            // const cast
            memcpy(const_cast<Condition*>(&cond), &c, sizeof(Condition));
        }
        else if (lhs == FLOAT) {
            c.rhsValue.type = lhs;
            float x;
            if (rhs == INT) {
                x = (float) *((int*) c.rhsValue.data);
            } else if (rhs == STRING) {
                x = atof((char*) c.rhsValue.data);
            }
            memcpy(c.rhsValue.data, &x, 4);
            rhs = lhs;
            // const cast
            memcpy(const_cast<Condition*>(&cond), &c, sizeof(Condition));
        }
    }
    return (lhs == rhs && foundl && foundr);
}


/*  Check if a record satisfies a certain condition
*/
bool QL_Manager::evalCondition(void* data, const Condition &cond, 
                    const vector<DataAttrInfo> &attributes) {
    // define pointer to appropriate member function
    bool (*comp)(void* attr1, void* attr2, int len1, 
                                    int len2, AttrType type);
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
        if (strcmp(cond.lhsAttr.attrName, attributes[i].attrName) == 0 &&
            (cond.lhsAttr.relName == 0 || 
                strcmp(cond.lhsAttr.relName, attributes[i].relName) == 0)
            ) {
            attr1 = (void*) ((char*) data + attributes[i].offset);
            len1 = attributes[i].attrLength;
            type = attributes[i].attrType;
        }
        if (cond.bRhsIsAttr) {
            if (strcmp(cond.rhsAttr.attrName, attributes[i].attrName) == 0 &&
                (cond.rhsAttr.relName == 0 || 
                strcmp(cond.rhsAttr.relName, attributes[i].relName) == 0)
                ) {
                attr2 = (void*) ((char*) data + attributes[i].offset);
                len2 = attributes[i].attrLength;
            }
        }
    }
    if (!cond.bRhsIsAttr) {
        attr2 = cond.rhsValue.data;
        len2 = len1;
    }

    return (*comp)(attr1, attr2, len1, len2, type);
}

/*  Find attribute info by attribute name 
*/
int QL_Manager::findAttr(const char* relName, const char* attrName, 
                    const vector<DataAttrInfo> &attributes) {
    for (unsigned int i = 0; i < attributes.size(); i++) {
        if (strcmp(attrName, attributes[i].attrName) == 0 &&
            (relName == 0 || 
                strcmp(relName, attributes[i].relName) == 0)
            ) {
            return i;
        }
    }
    return -1;
}

void QL_Manager::printPlanHeader(const char* operation, const char* relname) {
    cout << "********************************************\n";
    cout << "Query Execution Plan" << endl;
    cout << "-----------------------\n";
    cout << "Operation: " << operation << " " << relname << endl;
    cout << "-----------------------\n";
}


void QL_Manager::printPlanFooter() {
    cout << "\n********************************************\n\n";
}
