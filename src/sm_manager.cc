#include <cstdio>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <sstream>
#include "redbase.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"


using namespace std;

SM_Manager::SM_Manager(IX_Manager &ixm, RM_Manager &rmm) {
    ixman = &ixm;
    rmman = &rmm;
    isOpen = false;
}

SM_Manager::~SM_Manager() {
    if (isOpen) {
        rmman->CloseFile(relcat);
        rmman->CloseFile(attrcat);
    }
}

/* Steps - 
    1. Change the directory to the directory defined by dbName. Will
        give an error if the directory doesn't exist
    2. Load the catalog files, by setting the rm filehandle
    3. 
*/
RC SM_Manager::OpenDb(const char *dbName) {
    RC WARN = SM_OPEN_WARN, ERR = SM_OPEN_ERR;
    if (isOpen) return WARN;
    // change the directory
    SM_ErrorForward(chdir(dbName));
    // open the catalog files by setting the handles
    char relcatfile[] = "relcat";
    char attrcatfile[] = "attrcat";
    SM_ErrorForward(rmman->OpenFile(relcatfile, relcat));
    SM_ErrorForward(rmman->OpenFile(attrcatfile, attrcat));
    isOpen = true;
    return OK_RC;
}

/* Steps-
    1. Check if a db is open
    2. Flush the data and index files to disk and close them
    3. Flush the catalog files to disk and close them
*/
RC SM_Manager::CloseDb() {
    RC WARN = SM_CLOSE_WARN, ERR = SM_CLOSE_ERR;
    if (!isOpen) return WARN;
    SM_ErrorForward(rmman->CloseFile(relcat));
    SM_ErrorForward(rmman->CloseFile(attrcat));
    SM_ErrorForward(chdir(".."));
    isOpen = false;
    return OK_RC;
}

/* Steps-
    1. Check attrCount > 0
    2. Check if the relation name is not one of the catalog names
    3. Check if the relation already exists
    4. If the relation name is longer than MAXNAME truncate it
    5. Check if the attributes are valid 
        (i) Clip the attribute name to the MAXNAME if null character
            not found
        (ii) Check if the attribute type is valid
        (iii) Check if the attribute length is consistent
    6. Create a file for the relation
    7. Create DataAttrInfo (defined in printer.h) structures and insert
        them into attrcat
    8. Create AttrInfo (defined in sm.h) and insert into relcat
*/

RC SM_Manager::CreateTable(const char *relName,
                           int        attrCount,
                           AttrInfo   *attributes) {
    RC WARN = SM_CREATE_WARN, ERR = SM_CREATE_ERR;
    cout << "CreateTable\n"
         << "   relName     =" << relName << "\n"
         << "   attrCount   =" << attrCount << "\n";
    for (int i = 0; i < attrCount; i++) {
        cout << "   attributes[" << i << "].attrName=" << attributes[i].attrName
             << "   attrType="
             << (attributes[i].attrType == INT ? "INT" :
                 attributes[i].attrType == FLOAT ? "FLOAT" : "STRING")
             << "   attrLength=" << attributes[i].attrLength << "\n";
    }
    if (attrCount < 1) return SM_BAD_INPUT;
    // check for duplicate attribute name
    for (int i = 0; i < attrCount; i++) {
        for (int j = i+1; j < attrCount; j++) {
            if (strcmp(attributes[i].attrName, attributes[j].attrName) == 0)
                return SM_BAD_INPUT;
        }
    }

    // check if the database is open
    if (!isOpen) return SM_DB_CLOSED;
    // check if the relname is not one of catalog names
    if ((strcmp(relName, "relcat") == 0)
        || (strcmp(relName, "attrcat") == 0)) {
            return SM_BAD_INPUT;
    }
    // check if the relname already exists and is of right size
    if (access(relName, F_OK) == 0) return SM_DUPLICATE_RELATION;
    if (strlen(relName) > MAXNAME) return SM_BAD_INPUT;
    // do sanity checks of parameters
    int recSize = 0;
    for (int i = 0; i < attrCount; i++) {
        if (strlen(attributes[i].attrName) > MAXNAME) return SM_BAD_INPUT;
        if ((attributes[i].attrType < INT)
            || (attributes[i].attrType > STRING)) return SM_BAD_INPUT;
        if ((attributes[i].attrType != STRING)
            && (attributes[i].attrLength != 4)) return SM_BAD_INPUT;
        if ((attributes[i].attrType == STRING)
            && ((attributes[i].attrLength < 1) ||
                (attributes[i].attrLength > MAXSTRINGLEN ))) return SM_BAD_INPUT;
        recSize += attributes[i].attrLength;    
    }
    // create file for the relation
    SM_ErrorForward(rmman->CreateFile(relName, recSize));
    // Update Attrcat
    DataAttrInfo attr_desc;
    RID temp_rid;
    for (int i = 0, offset = 0; i < attrCount; i++) {
        strcpy(attr_desc.relName, relName);
        strcpy(attr_desc.attrName, attributes[i].attrName);
        attr_desc.offset = offset;
        offset += attributes[i].attrLength;
        attr_desc.attrType = attributes[i].attrType;
        attr_desc.attrLength = attributes[i].attrLength;
        attr_desc.indexNo = -1;
        if (attrcat.InsertRec((char*) &attr_desc, temp_rid)) {
            return SM_CREATE_ERR; // non-recoverable error, sort of
        }
    }
    // update relcat
    RelationInfo relinfo;
    strncpy(relinfo.rel_name, relName, MAXNAME+1);
    relinfo.tuple_size = recSize;
    relinfo.num_attr = attrCount;
    relinfo.index_num = -1;
    if (relcat.InsertRec((char*) &relinfo, temp_rid)) {
        return SM_CREATE_ERR; // non-recoverable error, sort of
    }
    int pnum;
    SM_ErrorForward(temp_rid.GetPageNum(pnum));
    SM_ErrorForward(relcat.ForcePages(ALL_PAGES));
    SM_ErrorForward(attrcat.ForcePages(ALL_PAGES));
    return OK_RC;
}

/* Steps-
    1. Check if the input name is not one of the catalog names
    2. Check if the relation exists, use the access command of unistd
    3. Delete the file for the relation
    4. Delete the tuples from attrcat
    5. Delete the tuple from relcat
    6. Delete the indexes
*/
RC SM_Manager::DropTable(const char *relName) {
    cout << "DropTable\n   relName=" << relName << "\n";
    RC WARN = SM_DROP_WARN, ERR = SM_DROP_ERR;
    // check if the database is open
    if (!isOpen) return SM_DB_CLOSED;
    // check if the relname is not one of catalog names
    if ((strcmp(relName, "relcat") == 0)
        || (strcmp(relName, "attrcat") == 0)) {
            return SM_BAD_INPUT;
    }
    if (unlink(relName) < 0) return WARN;
    // remove the tuple from relcat, failure is non-recoverable
    RM_Record rec;
    RID rid;
    SM_ErrorForward(getRelInfo(relName, rec));
    SM_ErrorForward(rec.GetRid(rid));
    SM_ErrorForward(relcat.DeleteRec(rid));
    // remove the tuples from attrcat
    RM_FileScan attrscan;
    SM_ErrorForward(attrscan.OpenScan(attrcat, STRING, 
        MAXNAME+1, 0, EQ_OP, (void*) relName, NO_HINT));
    DataAttrInfo* dinfo;
    char* dinfodata;
    while(attrscan.GetNextRec(rec) == OK_RC) {
        // delete the index if it exists
        SM_ErrorForward(rec.GetData(dinfodata));
        dinfo = (DataAttrInfo*) dinfodata;
        if (dinfo->indexNo >= 0) {
            SM_ErrorForward(ixman->DestroyIndex(relName, dinfo->indexNo));
        }
        SM_ErrorForward(rec.GetRid(rid));
        SM_ErrorForward(attrcat.DeleteRec(rid));
    }
    SM_ErrorForward(attrscan.CloseScan());
    SM_ErrorForward(relcat.ForcePages(ALL_PAGES));
    SM_ErrorForward(attrcat.ForcePages(ALL_PAGES));
    return OK_RC;
}

/*  Steps - 
    1. Check if the attribute is not already indexed by using attrcat
    2. If the attribute and the relation don't exist in the catalog, abort
    3. Use index manager to create an index on the attribute
    4. Update the index number of attribute in attrcat
*/
RC SM_Manager::CreateIndex(const char *relName,
                           const char *attrName) {
    cout << "CreateIndex\n"
         << "   relName =" << relName << "\n"
         << "   attrName=" << attrName << "\n";
    RC WARN = SM_IXCREATE_WARN, ERR = SM_IXCREATE_ERR;
    // check if the database is open
    if (!isOpen) return SM_DB_CLOSED;
    // check if the relation exists
    if (access(relName, F_OK) != 0) return WARN;
    // check if the attribute exists and is indexed
    RelationInfo relinfo;
    DataAttrInfo dinfo;
    RM_Record relrec, attrec, datarec;
    // checks the existence
    SM_ErrorForward(getAttrInfo(relName, attrName, dinfo, attrec));
    if (dinfo.indexNo >= 0) return WARN;
    // create an index on the attribute
    SM_ErrorForward(getRelInfo(relName, relrec));
    char *relinfodata;
    SM_ErrorForward(relrec.GetData(relinfodata));
    //relinfo = (RelationInfo) *relinfodata;
    memcpy(&relinfo, relinfodata, sizeof(RelationInfo));
    relinfo.index_num++;
    memcpy(relinfodata, &relinfo, sizeof(RelationInfo));
    SM_ErrorForward(ixman->CreateIndex(relName, relinfo.index_num,
        dinfo.attrType, dinfo.attrLength));
    dinfo.indexNo = relinfo.index_num;
    char *dinfodata;
    SM_ErrorForward(attrec.GetData(dinfodata));
    memcpy(dinfodata, &dinfo, sizeof(DataAttrInfo));
    // Add all the records in the relation to index
    IX_IndexHandle ihandle;
    RM_FileHandle relation;
    RM_FileScan fscan;
    SM_ErrorForward(ixman->OpenIndex(relName, dinfo.indexNo, ihandle));
    SM_ErrorForward(rmman->OpenFile(relName, relation));
    SM_ErrorForward(fscan.OpenScan(relation, dinfo.attrType, 
        dinfo.attrLength, dinfo.offset, NO_OP, 0, NO_HINT));
    char *data;
    RID rid;
    while (fscan.GetNextRec(datarec) == OK_RC) {
        // insert the record into index
        SM_ErrorForward(datarec.GetData(data));
        SM_ErrorForward(datarec.GetRid(rid));
        SM_ErrorForward(ihandle.InsertEntry((void*) (data + dinfo.offset), 
            rid));
    }
    SM_ErrorForward(fscan.CloseScan());
    SM_ErrorForward(rmman->CloseFile(relation));
    SM_ErrorForward(ihandle.ForcePages());
    SM_ErrorForward(ixman->CloseIndex(ihandle));
    SM_ErrorForward(attrcat.UpdateRec(attrec));
    SM_ErrorForward(relcat.UpdateRec(relrec));
    SM_ErrorForward(relcat.ForcePages(ALL_PAGES));
    SM_ErrorForward(attrcat.ForcePages(ALL_PAGES));
    return OK_RC;
}

/* Steps -
    1. Check if the attribute is indexed
    2. If the attribute and the relation dont exist in the catalog, abort
    3. Delete the corresponding index using index manager
    4. Update the index number of attribute in attrcat
*/
RC SM_Manager::DropIndex(const char *relName,
                         const char *attrName) {
    cout << "DropIndex\n"
         << "   relName =" << relName << "\n"
         << "   attrName=" << attrName << "\n";
    RC WARN = SM_IXDROP_WARN, ERR = SM_IXDROP_ERR;
    // check if the database is open
    if (!isOpen) return SM_DB_CLOSED;
    // check if the relation exists
    if (access(relName, F_OK) != 0) return WARN;
    // check if the attribute exists and is indexed
    DataAttrInfo dinfo;
    RM_Record rec;
    SM_ErrorForward(getAttrInfo(relName, attrName, dinfo, rec));
    // delete the index
    SM_ErrorForward(ixman->DestroyIndex(relName, dinfo.indexNo));
    // update catalog
    dinfo.indexNo = -1;
    char *dinfodata;
    SM_ErrorForward(rec.GetData(dinfodata));
    memcpy(dinfodata, &dinfo, sizeof(DataAttrInfo));
    SM_ErrorForward(attrcat.UpdateRec(rec));
    SM_ErrorForward(relcat.ForcePages(ALL_PAGES));
    SM_ErrorForward(attrcat.ForcePages(ALL_PAGES));
    return OK_RC;
}

/* Steps-
    1. Check if the relname and filename are valid
    2. Define an array of indexhandles to take care of the indices
    3. Loop through the file to get tuples and 
*/
RC SM_Manager::Load(const char *relName,
                    const char *fileName) {
    cout << "Load\n"
         << "   relName =" << relName << "\n"
         << "   fileName=" << fileName << "\n";
    RC WARN = SM_LOAD_WARN, ERR = SM_LOAD_ERR;
    // check if the database is open
    if (!isOpen) return SM_DB_CLOSED;
    // check if the relation and load file exist
    if (access(relName, F_OK) != 0) return WARN;
    if (access(fileName, F_OK) != 0) return WARN;
    RM_Record relrec, rec;
    RelationInfo* relinfo;
    char* relinfodata;
    SM_ErrorForward(getRelInfo(relName, relrec));
    SM_ErrorForward(relrec.GetData(relinfodata));
    relinfo = (RelationInfo*) relinfodata;
    // fetch the attributes in the relation
    vector<DataAttrInfo> attributes;
    RM_FileScan attrscan;
    SM_ErrorForward(attrscan.OpenScan(attrcat, STRING, 
        MAXNAME+1, 0, EQ_OP, (void*) relName, NO_HINT));
    DataAttrInfo dinfo;
    char *dinfodata;
    bool found = false;
    while(attrscan.GetNextRec(rec) == OK_RC) {
        found = true;
        SM_ErrorForward(rec.GetData(dinfodata));
        // dinfo = (DataAttrInfo) &dinfodata;
        memcpy(&dinfo, dinfodata, sizeof(DataAttrInfo));
        attributes.push_back(dinfo);
    }
    SM_ErrorForward(attrscan.CloseScan());
    if (!found) return WARN;
    // sort the attributes accourding to offset, defined in DataAttrInfo
    sort(attributes.begin(), attributes.end());
    // keep track of indexed attributes, initialize their indexhandles
    vector<int> ind;
    vector<IX_IndexHandle> ihandles(attributes.size());
    for (size_t i = 0; i < attributes.size(); i++) {
        if (attributes[i].indexNo >= 0) {
            ind.push_back(i);
            SM_ErrorForward(ixman->OpenIndex(relName, 
                attributes[i].indexNo, ihandles[i]));
        }
    }
    // initialize the relation filehandle
    RM_FileHandle relation;
    SM_ErrorForward(rmman->OpenFile(relName, relation));
    // scan through the input file and extract the records
    string line, word;
    char* buffer = new char[relinfo->tuple_size];
    RID record_rid;
    ifstream file(fileName);
    if (!file.is_open()) return WARN;
    stringstream ss;
    while (getline(file, line)) {
        ss << line;
        for (int i = 0; i < relinfo->num_attr; i++) {
            getline(ss, word, ',');
            if (attributes[i].attrType == INT) {
                int intatt = atoi(word.c_str());
                memcpy(buffer + attributes[i].offset, 
                    &intatt, attributes[i].attrLength);
            }
            else if (attributes[i].attrType == FLOAT) {
                float flatt = atof(word.c_str());
                memcpy(buffer+attributes[i].offset, 
                    &flatt, attributes[i].attrLength);
            }
            else if (attributes[i].attrType == STRING) {
                strncpy(buffer+attributes[i].offset, 
                    word.c_str(), attributes[i].attrLength);
            }
            else {
                return ERR; // bad attribute type in metadata
            }
        }
        ss.clear();
        SM_ErrorForward(relation.InsertRec(buffer, record_rid));
        for (size_t i = 0; i < ind.size(); i++) {
            SM_ErrorForward(ihandles[ind[i]].InsertEntry( (void*) 
                (buffer + attributes[ind[i]].offset), record_rid));
        }
    }
    // free the resources, close the relation and index files
    file.close();
    delete[] buffer;
    SM_ErrorForward(rmman->CloseFile(relation));
    for (size_t i = 0; i < ind.size(); i++) {
        SM_ErrorForward(ixman->CloseIndex(ihandles[ind[i]]));
    }
    return OK_RC;
}

RC SM_Manager::Print(const char *relName) {
    cout << "Print\n"
         << "   relName=" << relName << "\n";
    RC WARN = SM_PRINT_WARN, ERR = SM_PRINT_ERR;
    if (!isOpen) return SM_DB_CLOSED;
    // check if the relation exists
    if (access(relName, F_OK) != 0) return WARN;
    RM_Record rec;
    RelationInfo relinfo;
    char* relinfodata;
    SM_ErrorForward(getRelInfo(relName, rec));
    SM_ErrorForward(rec.GetData(relinfodata));
    //relinfo = (RelationInfo*) relinfodata;
    memcpy(&relinfo, relinfodata, sizeof(RelationInfo));
    // fetch the attributes in the relation
    DataAttrInfo* attributes = new DataAttrInfo[relinfo.num_attr];
    RM_FileScan attrscan;
    SM_ErrorForward(attrscan.OpenScan(attrcat, STRING, 
        MAXNAME+1, 0, EQ_OP, (void*) relName, NO_HINT));
    char *dinfodata;
    for (int i = 0; i < relinfo.num_attr; i++) {
        SM_ErrorForward(attrscan.GetNextRec(rec));
        SM_ErrorForward(rec.GetData(dinfodata));
        memcpy(&attributes[i], dinfodata, sizeof(DataAttrInfo));
    }
    SM_ErrorForward(attrscan.CloseScan());

    // Instantiate a Printer object and print the header information
    Printer p(attributes, relinfo.num_attr);
    p.PrintHeader(cout);

    // Open the file and set up the file scan
    RM_FileHandle rfh;
    SM_ErrorForward(rmman->OpenFile(relName, rfh));

    RM_FileScan rfs;
    char *data;
    RC rc = OK_RC;

    SM_ErrorForward(rfs.OpenScan(rfh, INT, sizeof(int), 0, NO_OP, NULL));

    // Print each tuple
    while (rc!=RM_EOF) {
        rc = rfs.GetNextRec(rec);

       if (rc!=0 && rc!=RM_EOF)
          return (rc);

       if (rc!=RM_EOF) {
            SM_ErrorForward(rec.GetData(data));
            p.Print(cout, data);
        }
    }
    // Print the footer information
    p.PrintFooter(cout);
    SM_ErrorForward(rfs.CloseScan());
    SM_ErrorForward(rmman->CloseFile(rfh));
    delete[] attributes;
    return OK_RC;
}

RC SM_Manager::Set(const char *paramName, const char *value) {
    cout << "Set\n"
         << "   paramName=" << paramName << "\n"
         << "   value    =" << value << "\n";
    return SM_NOT_IMPLEMENTED;
}

RC SM_Manager::Help() {
    cout << "Help\n";
    RC WARN = SM_PRINT_WARN, ERR = SM_PRINT_ERR;
    if (!isOpen) return SM_DB_CLOSED;
    RM_Record rec;
    // define the pseudo header
    DataAttrInfo* attributes = new DataAttrInfo[3];
    strcpy(attributes[0].relName,"relcat");
    strcpy(attributes[0].attrName,"relName");
    attributes[0].offset = 0;
    attributes[0].attrType = STRING;
    attributes[0].attrLength = MAXNAME + 1;
    attributes[0].indexNo = -1;
    strcpy(attributes[1].relName,"relcat");
    strcpy(attributes[1].attrName,"tupleLength");
    attributes[1].offset = MAXNAME+1;
    attributes[1].attrType = INT;
    attributes[1].attrLength = 4;
    attributes[1].indexNo = -1;
    strcpy(attributes[2].relName,"relcat");
    strcpy(attributes[2].attrName,"attrCount");
    attributes[2].offset = MAXNAME+5;
    attributes[2].attrType = INT;
    attributes[2].attrLength = 4;
    attributes[2].indexNo = -1;

    // Instantiate a Printer object and print the header information
    Printer p(attributes, 3);
    p.PrintHeader(cout);

    RM_FileScan relscan;
    SM_ErrorForward(relscan.OpenScan(relcat, STRING, 
        MAXNAME+1, 0, NO_OP, 0, NO_HINT));
    char *data;
    char *buffer = new char[MAXNAME+9];
    RelationInfo relinfo;
    RC rc = OK_RC;

    // Print each tuple
    while (rc!=RM_EOF) {
        rc = relscan.GetNextRec(rec);

       if (rc!=0 && rc!=RM_EOF)
          return (rc);

       if (rc!=RM_EOF) {
            SM_ErrorForward(rec.GetData(data));
            memcpy(&relinfo, data, sizeof(RelationInfo));
            strncpy(buffer, relinfo.rel_name, MAXNAME+1);
            memcpy(buffer+MAXNAME+1, (void*) &relinfo.tuple_size, 4);
            memcpy(buffer+MAXNAME+5, (void*) &relinfo.num_attr, 4);
            p.Print(cout, buffer);
        }
    }
    // Print the footer information
    // p.PrintFooter(cout);
    SM_ErrorForward(relscan.CloseScan());
    delete[] attributes;
    delete[] buffer;
    return OK_RC;
}

RC SM_Manager::Help(const char *relName) {
    RC WARN = SM_PRINT_WARN, ERR = SM_PRINT_ERR;
    if (!isOpen) return SM_DB_CLOSED;
    RM_Record rec;
    // define the pseudo header
    DataAttrInfo* attributes = new DataAttrInfo[4];
    strcpy(attributes[0].relName,"attrcat");
    strcpy(attributes[0].attrName,"Attribute");
    attributes[0].offset = 0;
    attributes[0].attrType = STRING;
    attributes[0].attrLength = MAXNAME + 1;
    attributes[0].indexNo = -1;
    strcpy(attributes[1].relName,"attrcat");
    strcpy(attributes[1].attrName,"Type");
    attributes[1].offset = MAXNAME + 1;
    attributes[1].attrType = STRING;
    attributes[1].attrLength = 7;
    attributes[1].indexNo = -1;
    strcpy(attributes[2].relName,"attrcat");
    strcpy(attributes[2].attrName,"Length");
    attributes[2].offset = MAXNAME + 8;
    attributes[2].attrType = INT;
    attributes[2].attrLength = 4;
    attributes[2].indexNo = -1;
    strcpy(attributes[3].relName,"attrcat");
    strcpy(attributes[3].attrName,"Indexed?");
    attributes[3].offset = MAXNAME + 12;
    attributes[3].attrType = STRING;
    attributes[3].attrLength = 4;
    attributes[3].indexNo = -1;
    

    // Instantiate a Printer object and print the header information
    Printer p(attributes, 4);
    p.PrintHeader(cout);

    RM_FileScan attrscan;
    SM_ErrorForward(attrscan.OpenScan(attrcat, STRING, 
        MAXNAME+1, 0, EQ_OP, (void*) relName, NO_HINT));
    char *data;
    RC rc = OK_RC;

    DataAttrInfo dinfo;
    char* buffer = new char[MAXNAME + 16];
    string isindexed, dtype;
    // Print each tuple
    while (rc!=RM_EOF) {
        rc = attrscan.GetNextRec(rec);

       if (rc!=0 && rc!=RM_EOF)
          return (rc);

       if (rc!=RM_EOF) {
            SM_ErrorForward(rec.GetData(data));
            memcpy(&dinfo, data, sizeof(DataAttrInfo));
            if (dinfo.attrType == INT) {
                dtype = "INT";
            }
            else if (dinfo.attrType == FLOAT) {
                dtype = "FLOAT";
            }
            else {
                dtype = "STRING";
            }
            isindexed = (dinfo.indexNo>=0)?"yes":"no";
            // put the values in buffer
            strncpy(buffer, dinfo.attrName, MAXNAME+1);
            strncpy(buffer+MAXNAME+1, dtype.c_str(), 7);
            memcpy(buffer+MAXNAME+8, (void*) &dinfo.attrLength, 4);
            strncpy(buffer+MAXNAME+12, isindexed.c_str(), 4);
            p.Print(cout, buffer);
        }
    }
    // Print the footer information
    // p.PrintFooter(cout);
    SM_ErrorForward(attrscan.CloseScan());
    delete[] attributes;
    delete[] buffer;
    return OK_RC;
}


//
// Private methods
//

RC SM_Manager::getRelInfo(const char* relName, RM_Record &rec) {
    RC WARN = SM_RELATION_NOT_FOUND, ERR = SM_RELATION_NOT_FOUND;
    RM_FileScan relscan;
    SM_ErrorForward(relscan.OpenScan(relcat, STRING, 
        MAXNAME+1, 0, EQ_OP, (void*) relName, NO_HINT));
    // only one record expected, return an error if none found
    SM_ErrorForward(relscan.GetNextRec(rec));
    SM_ErrorForward(relscan.CloseScan());
    return OK_RC;
}

RC SM_Manager::getAttrInfo(const char* relName, const char* attrName, 
                DataAttrInfo &dinfo, RM_Record &rec) {
    RC WARN = SM_ATTRIBUTE_NOT_FOUND, ERR = SM_ATTRIBUTE_NOT_FOUND;
    RM_FileScan attrscan;
    SM_ErrorForward(attrscan.OpenScan(attrcat, STRING, 
        MAXNAME+1, 0, EQ_OP, (void*) relName, NO_HINT));
    bool found = false;
    while(attrscan.GetNextRec(rec) == OK_RC) {
        char* dinfodata;
        SM_ErrorForward(rec.GetData(dinfodata));
        // dinfo = (DataAttrInfo) *dinfodata;
        memcpy(&dinfo, dinfodata, sizeof(DataAttrInfo));
        if (strcmp(dinfo.attrName, attrName) == 0) {
            found = true;
            break;
        }
    }
    SM_ErrorForward(attrscan.CloseScan());
    if (!found) return WARN;
    return OK_RC;
}

