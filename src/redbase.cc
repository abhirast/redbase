//
// redbase.cc
//
// Author: Jason McHugh (mchughj@cs.stanford.edu)
//
// This shell is provided for the student.

#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "redbase.h"
#include "rm.h"
#include "sm.h"
#include "ql.h"

using namespace std;

//
// main
//
int main(int argc, char *argv[])
{
    char *dbname;
    
    // Look for 2 arguments.  The first is always the name of the program
    // that was executed, and the second should be the name of the
    // database.
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " dbname \n";
        exit(1);
    }

    dbname = argv[1];

    PF_Manager pfm;
    RM_Manager rmm(pfm);
    IX_Manager ixm(pfm);
    SM_Manager smm(ixm, rmm);
    QL_Manager qlm(smm, ixm, rmm);

    // open the database
    if (smm.OpenDb(dbname) != OK_RC) {
        cout<<"Unable to open db\n";
    }

    RBparse(pfm, smm, qlm);

    // close the database
    if (smm.CloseDb() != OK_RC) {
        cout<<"Unable to close db\n";
    }
    cout << "Bye.\n";
}
