//
// dbcreate.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.

#include <cstdio>
#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include <sstream>
#include "rm.h"
#include "sm.h"
#include "redbase.h"
#include "printer.h"

using namespace std;

//
// main
//
int main(int argc, char *argv[]) {

    char *dbname;
    char command[255] = "mkdir ";
    
    // Look for 2 arguments. The first is always the name of the program
    // that was executed, and the second should be the name of the
    // database.
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }

    // The database name is the second argument
    dbname = argv[1];

    // Create a subdirectory for the database
    if (system (strcat(command,dbname)) != 0) {
        cerr << argv[0] << " cannot create directory: " << dbname << "\n";
        exit(1);
    }

    if (chdir(dbname) < 0) {
        cerr << argv[0] << " chdir error to " << dbname << "\n";
        exit(1);
    }

    // Create the system catalogs...

    /* create relcat, a relation which will contain information about
        1. Name of the relation in database
        2. Length of the tuple of the relation
        3. Number of attributes in the relation
        4. Number of indexed attributes in the relation
        5. Page number in attrcat containing attribute information
    */
    PF_Manager pfm;
    RM_Manager rmm(pfm);
    char relcat[] = "relcat";
    // create the file for storing relation catalogs
    if (rmm.CreateFile(relcat, sizeof(RelationInfo)) != 0) {
        cerr<<"Error while creating relcat\n";
    }


    /* create attrcat, a relation which will contain information about
        the attributes in a relation. It has the following fields
        1. Name of the relation
        2. Name of the attribute
        3. Offset of the attribute from beginning of tuple
        4. Type of the attribute
        5. Length of the attribute
        6. Index number of the attribute (-1 if doesn't exist)
    */
    char attrcat[] = "attrcat";
    // create the file for storing attributes 
    if (rmm.CreateFile(attrcat, sizeof(DataAttrInfo)) != 0) {
        cerr<<"Error while creating attrcat\n";
    }

    // open the files 
    RM_FileHandle relc, attc;
    if ((rmm.OpenFile(relcat, relc)) ||
    (rmm.OpenFile(attrcat, attc))) {
        cout<<"Error while creating database\n";
        return 1;
    }

    // update relcat
    RelationInfo relinfo1, relinfo2;
    RID temp_rid;
    strncpy(relinfo1.rel_name, relcat, MAXNAME+1);
    relinfo1.tuple_size = sizeof(RelationInfo);
    relinfo1.num_attr = 4;
    relinfo1.index_num = -1;
    strncpy(relinfo2.rel_name, attrcat, MAXNAME+1);
    relinfo2.tuple_size = sizeof(DataAttrInfo);
    relinfo2.num_attr = 6;
    relinfo2.index_num = -1;
    if (relc.InsertRec((char*) &relinfo1, temp_rid) ||
        relc.InsertRec((char*) &relinfo2, temp_rid)) {
        cout<<"Error while creating database\n";
        return 1;
    }

    // Update Attrcat
    DataAttrInfo attr[10];
    const char *anames[] = {"relName", "tupleLength", "attrCount", 
        "indexNo", "relName", "attrName", "offset", "attrType", 
        "attrLength", "indexNo"};
    int stl = (char*) &relinfo1.tuple_size - (char*) &relinfo1;
    int attl = sizeof(AttrType);
    int offsets[] = {0, stl, stl+4, stl+8, 0, stl, 2*stl, 
        2*stl+4, 2*stl+4+attl, 2*stl+8+attl};
    int attrlengths[] = {stl, 4, 4, 4, stl, stl, 4, attl, 4, 4};
    AttrType attypes[] = {STRING, INT, INT, INT, STRING, STRING,
        INT, INT, INT, INT};
    for (int i = 0; i < 10; i++) {
        if (i < 4) {
            strncpy(attr[i].relName, relcat, MAXNAME+1);
        } else {
            strncpy(attr[i].relName, attrcat, MAXNAME+1);
        }
        strncpy(attr[i].attrName, (char*) anames[i], MAXNAME+1);
        attr[i].offset = offsets[i];
        attr[i].attrType = attypes[i];
        attr[i].attrLength = attrlengths[i];
        attr[i].indexNo = -1;
        if (attc.InsertRec((char*) &attr[i], temp_rid)) {
            cout<<"Error while creating database\n";
        }
    }

    if (relc.ForcePages(ALL_PAGES) || attc.ForcePages(ALL_PAGES)) {
        cout<<"Error while creating database\n";
        return 1;
    }
    if (rmm.CloseFile(relc) || rmm.CloseFile(attc)) {
        cout<<"Error while creating database\n";
        return 1;
    }

    return 0;
}
