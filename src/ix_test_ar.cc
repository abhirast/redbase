#include <cstdio>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>

#include "redbase.h"
#include "pf.h"
#include "rm.h"
#include "ix.h"

using namespace std;

PF_Manager pfm;
RM_Manager rmm(pfm);
IX_Manager ixm(pfm);

#define IX_Error(expr) do { \
RC tmp_rc = (expr);\
if (tmp_rc != OK_RC) \
        IX_PrintError(tmp_rc); \
} while (0)

void test1() {
	IX_IndexHandle ih;
	char* filename = "testrel2";
	int index = 0;
	int keys[10] = {3,1,2,4,6,5,8,9,0,7};
	// int keys[10] = {0,1,2,3,4,5,6,7,8,9};
	IX_Error(ixm.CreateIndex(filename, index, INT, sizeof(int)));
	IX_Error(ixm.OpenIndex(filename, index, ih));
    // insert the records
	for (int i = 0; i < 9; i++) {
		RID rid(keys[i]+1, keys[i]+1);
		int x = keys[i]+1;
		IX_Error(ih.InsertEntry((void*) &x, rid));
	}
	IX_Error(ih.ForcePages());
	// scan the records
	IX_IndexScan is;
	IX_Error(is.OpenScan(ih, NO_OP, 0, NO_HINT));
	RC rc;
	RID tmp;
	int p,s;
	while (true) {
		rc = is.GetNextEntry(tmp);
		if (rc != 0) {
			IX_PrintError(rc);
			break;
		}
		IX_Error(tmp.GetPageNum(p));
		IX_Error(tmp.GetSlotNum(s));
		cout<<p<<'\t';
	}
	IX_Error(ixm.CloseIndex(ih));
}

void test2() {
	IX_IndexHandle ih;
	char* filename = "testrel2";
	int index = 0;
	int keys[15] = {3,1,2,4,6,5,8,9,0,7, 10, 11, 12, 13, 14};
	IX_Error(ixm.CreateIndex(filename, index, INT, sizeof(int)));
	IX_Error(ixm.OpenIndex(filename, index, ih));
    // insert the records
	for (int i = 0; i < 15; i++) {
		RID rid(keys[i]+1, keys[i]+1);
		int x = i;
		IX_Error(ih.InsertEntry((void*) &x, rid));
	}
	IX_Error(ih.ForcePages());
	// delete the entries
	for (int i = 0; i < 15; i++) {
		RID rid(keys[i]+1, keys[i]+1);
		int x = i;
		cout<<"deleting record "<< i<<endl; 
		IX_Error(ih.DeleteEntry((void*) &x, rid));
	}
	IX_Error(ih.ForcePages());
	IX_Error(ixm.CloseIndex(ih));
	cout<<"Test 2 passed\n";
}

void test3() {
	IX_IndexHandle ih;
	char* filename = "testrel3";
	int index = 0;
	
	IX_Error(ixm.CreateIndex(filename, index, STRING, 10));
	IX_Error(ixm.OpenIndex(filename, index, ih));
    // insert the records
	for (int i = 100; i < 300; i++) {
		RID rid(i+1, i+1);
		char x[11];
		sprintf(x, "abhinav%d", i);
		IX_Error(ih.InsertEntry((void*) &x, rid));
	}
	IX_Error(ixm.CloseIndex(ih));
	IX_Error(ixm.OpenIndex(filename, index, ih));
	
	// scan the records
	IX_IndexScan is;
	char q[11];
	sprintf(q, "abhinav%d", 250);
	IX_Error(is.OpenScan(ih, GT_OP, (void*) q, NO_HINT));
	RC rc;
	RID tmp;
	int p,s;
	int k = 251;
	while (true) {
		rc = is.GetNextEntry(tmp);
		if (rc != 0) {
			IX_PrintError(rc);
			break;
		}
		char key[11];
		sprintf(key, "abhinav%d", k);
		k++;
		if (k == 270) IX_Error(ixm.CloseIndex(ih));
		// IX_Error(ih.DeleteEntry((void*) key, tmp));
		IX_Error(tmp.GetPageNum(p));
		IX_Error(tmp.GetSlotNum(s));
		cout<<p<<'\t';
	}
	IX_Error(ixm.CloseIndex(ih));

	IX_Error(ixm.DestroyIndex(filename, index));
	cout<<"Test 3 passed\n";
}

void test4() {
	IX_IndexHandle ih;
	char* filename = "testrel2";
	int index = 0;
	
	IX_Error(ixm.CreateIndex(filename, index, INT, sizeof(int)));
	IX_Error(ixm.OpenIndex(filename, index, ih));
    // insert the records
	for (int j = 0; j < 5000; j++) {
		RID rid(j+1, j+1);
		int x = j;
		IX_Error(ih.InsertEntry((void*) &x, rid));
	}
	IX_Error(ih.ForcePages());
	// scan the records
	IX_IndexScan is;
	IX_Error(is.OpenScan(ih, NO_OP, 0, NO_HINT));
	RC rc;
	RID tmp;
	int p,s;
	int re = 0;
	while (true) {
		rc = is.GetNextEntry(tmp);
		if (rc != 0) {
			IX_PrintError(rc);
			break;
		}
		//IX_Error(ih.DeleteEntry((void*) &re, tmp));
		re++;
		IX_Error(tmp.GetPageNum(p));
		IX_Error(tmp.GetSlotNum(s));
		cout<<p<<'\t';
	}
	IX_Error(ixm.CloseIndex(ih));
	IX_Error(ixm.DestroyIndex(filename, index));
}

int main() {
	test4();
	return 0;
}
