//
// sm.h
//   Data Manager Component Interface
//

#ifndef SM_H
#define SM_H

// Please do not include any other files than the ones below in this file.

#include <stdlib.h>
#include <string.h>
#include <vector>
#include "redbase.h"  // Please don't change these lines
#include "parser.h"
#include "rm.h"
#include "ix.h"
#include "printer.h"  // for DataAttrInfo


struct RelationInfo {
  // Default constructor
  RelationInfo() {
    memset((void*) this, 0, sizeof(*this));
  };

  char      rel_name[MAXNAME + 1];
  int       tuple_size;
  int       num_attr;
  int       index_num;
  // RID       relcat_id; make more efficient by eliminating scans?
};


//
// SM_Manager: provides data management
//
class SM_Manager {
    friend class QL_Manager;
public:
    SM_Manager    (IX_Manager &ixm, RM_Manager &rmm);
    ~SM_Manager   ();                             // Destructor

    RC OpenDb     (const char *dbName);           // Open the database
    RC CloseDb    ();                             // close the database

    RC CreateTable(const char *relName,           // create relation relName
                   int        attrCount,          //   number of attributes
                   AttrInfo   *attributes);       //   attribute data
    RC CreateIndex(const char *relName,           // create an index for
                   const char *attrName);         //   relName.attrName
    RC DropTable  (const char *relName);          // destroy a relation

    RC DropIndex  (const char *relName,           // destroy index on
                   const char *attrName);         //   relName.attrName
    RC Load       (const char *relName,           // load relName from
                   const char *fileName);         //   fileName
    RC Help       ();                             // Print relations in db
    RC Help       (const char *relName);          // print schema of relName

    RC Print      (const char *relName);          // print relName contents

    RC Set        (const char *paramName,         // set parameter to
                   const char *value);            //   value

private:
    IX_Manager*         ixman;          // ix manager
    RM_Manager*         rmman;          // rm manager
    bool                isOpen;         // indicates whether a db is open
    RM_FileHandle       relcat;         // handle to the relation 
    RM_FileHandle       attrcat;        // handle to attribute catalog
    RC getRelInfo(const char* relName, RM_Record &rec);
    RC getAttrInfo(const char* relName, const char* attrName, 
                DataAttrInfo &dinfo, RM_Record &rec); 
    RC getAttributes(const char *relName, std::vector<DataAttrInfo> &attributes);
    RC getRelation(const char* relName, RelationInfo &relation);
    bool SHOW_ALL_PLANS;
    int SORT_RES;
};

//
// Print-error function
//
void SM_PrintError(RC rc);


// Macro for error forwarding
// WARN and ERR to be defined in the context where macro is used
#define SM_ErrorForward(expr) do { \
RC tmp_rc = (expr);\
if (tmp_rc != OK_RC) \
    return ((tmp_rc > 0) ? WARN : ERR); \
} while (0)


// Error codes

#define SM_CREATE_WARN                      (START_SM_WARN + 0)
#define SM_OPEN_WARN                        (START_SM_WARN + 1)
#define SM_CLOSE_WARN                       (START_SM_WARN + 2)
#define SM_BAD_INPUT                        (START_SM_WARN + 3)
#define SM_DROP_WARN                        (START_SM_WARN + 4)
#define SM_IXCREATE_WARN                    (START_SM_WARN + 5)
#define SM_IXDROP_WARN                      (START_SM_WARN + 6)
#define SM_DB_CLOSED                        (START_SM_WARN + 7)
#define SM_LOAD_WARN                        (START_SM_WARN + 8)
#define SM_DUPLICATE_RELATION               (START_SM_WARN + 9)
#define SM_RELATION_NOT_FOUND               (START_SM_WARN + 10)
#define SM_ATTRIBUTE_NOT_FOUND              (START_SM_WARN + 11)
#define SM_PRINT_WARN                       (START_SM_WARN + 12)
#define SM_NOT_IMPLEMENTED                  (START_SM_WARN + 13)
#define SM_LASTWARN                         SM_NOT_IMPLEMENTED

#define SM_CREATE_ERR                       (START_SM_ERR - 0)
#define SM_OPEN_ERR                         (START_SM_ERR - 1)
#define SM_CLOSE_ERR                        (START_SM_ERR - 2)
#define SM_DROP_ERR                         (START_SM_ERR - 3)
#define SM_IXCREATE_ERR                     (START_SM_ERR - 4)
#define SM_IXDROP_ERR                       (START_SM_ERR - 5)
#define SM_LOAD_ERR                         (START_SM_ERR - 6)
#define SM_PRINT_ERR                        (START_SM_ERR - 7)
#define SM_LASTERROR                        SM_PRINT_ERR


#endif
