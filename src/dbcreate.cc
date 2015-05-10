//
// dbcreate.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.

#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
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
    return 0;
}
