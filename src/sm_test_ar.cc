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
    return tmp_rc; \
} while (0)

using namespace std;

// globals
PF_Manager _pfm;
RM_Manager _rmm(_pfm);
IX_Manager _ixm(_pfm);
SM_Manager _smm(_ixm, _rmm);
QL_Manager _qlm(_smm, _ixm, _rmm);

RC test1();
RC test2();
void tester(RC rc, int testnumber);

int main() {
	tester(test1(), 1);
	tester(test2(), 2);
}

void tester(RC rc, int testnumber) {
	cout<<endl<<endl;
	if (rc == OK_RC) {
		cout<<"Test "<<testnumber<<" successful";
	} else {
		SM_PrintError(rc);
	}
	cout<<endl<<endl;
}






RC test1() {
	    char dbname[] = "abhinav";
    SM_TestForward(_smm.OpenDb(dbname));
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
	attributes[3].attrName = a4;
	attributes[3].attrType = FLOAT;
	attributes[3].attrLength = 4;
	char relName[] = "soaprel";
    SM_TestForward(_smm.CreateTable(relName, 4, attributes));
    SM_TestForward(_smm.Load(relName, "/home/abhinav/Dropbox/redbase/src/soaps.data"));
    SM_TestForward(_smm.CreateIndex(relName, a1));
    SM_TestForward(_smm.DropIndex(relName, a1));
    SM_TestForward(_smm.CreateIndex(relName, a1));
    SM_TestForward(_smm.Print(relName));
    SM_TestForward(_smm.Help());
    SM_TestForward(_smm.DropTable(relName));
    SM_TestForward(_smm.CloseDb());
    delete[] attributes;
    return OK_RC;
}

RC test2() {
    char dbname[] = "abhinav";
    SM_TestForward(_smm.OpenDb(dbname));
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
	attributes[3].attrName = a4;
	attributes[3].attrType = FLOAT;
	attributes[3].attrLength = 4;
	char relName[] = "soaprel";
    SM_TestForward(_smm.CreateTable(relName, 4, attributes));
    SM_TestForward(_smm.CreateIndex(relName, a1));
    SM_TestForward(_smm.CreateIndex(relName, a2));
    SM_TestForward(_smm.Print(relName));
    SM_TestForward(_smm.Help(relName));
    SM_TestForward(_smm.DropTable(relName));
    delete[] attributes;
    return OK_RC;
}

