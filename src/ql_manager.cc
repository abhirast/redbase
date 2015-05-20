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
#include "redbase.h"
#include "ql.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "parser.h"

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
    if (nValues != attributes.size()) return QL_INVALID_WARN;
    for (int i = 0; i < attributes.size(); i++) {
        if (attributes[i].attrType != values[i].type) return QL_INVALID_WARN;
    } 
    // safe to insert now
    char *buffer = new char[relation.tuple_size];
    for (int i = 0, offset = 0; i < attributes.size(); i++) {
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

//
// Delete from the relName all tuples that satisfy conditions
//
RC QL_Manager::Delete(const char *relName,
                      int nConditions, const Condition conditions[])
{
    int i;

    cout << "Delete\n";

    cout << "   relName = " << relName << "\n";
    cout << "   nCondtions = " << nConditions << "\n";
    for (i = 0; i < nConditions; i++)
        cout << "   conditions[" << i << "]:" << conditions[i] << "\n";

    return 0;
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