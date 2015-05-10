#include <iostream>
#include <cstdio>
#include <vector>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <unistd.h>
#include "redbase.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "ql.h"
#include "parser.h"

#define SM_TestForward(expr) do { \
RC tmp_rc = (expr);\
if (tmp_rc != OK_RC) \
    SM_PrintError(tmp_rc); \
} while (0)

using namespace std;

RC test1() {
	PF_Manager pfm;
    RM_Manager rmm(pfm);
    IX_Manager ixm(pfm);
    SM_Manager smm(ixm, rmm);
    QL_Manager qlm(smm, ixm, rmm);

    char dbname[] = "abhinav";
    SM_TestForward(smm.OpenDb(dbname));
    // create table soaps(soapid  i, sname  c28, network  c4, rating  f);
	AttrInfo *attributes = new AttrInfo[4];
	char a1[] = "soapid", a2[] = "sname", a3[] = "network", a4[] = "rating";
	attributes[0].attrName = a1;
	attributes[0].attrType = INT;
	attributes[0].attrLength = 4;
	attributes[1].attrName = a2;
	attributes[1].attrType = STRING;
	attributes[1].attrLength = 28;
	attributes[2].attrName = a3;
	attributes[2].attrType = STRING;
	attributes[2].attrLength = 4;
	attributes[3].attrName = a1;
	attributes[3].attrType = FLOAT;
	attributes[3].attrLength = 4;
	char relName[] = "soaprel";
    SM_TestForward(smm.CreateTable(relName, 4, attributes));
    SM_TestForward(smm.Load(relName, "/home/abhinav/temp/data/soaps.data"));
    SM_TestForward(smm.DropTable(relName));
}


int main() {
	test1();
	return 0;
}