#include <cstdio>
#include <iostream>
#include "rm.h"
#include "rm_internal.h"

using namespace std;

RM_Record::RM_Record() {
	bIsAllocated = 0;
}

RM_Record::~RM_Record() {
	// free the dynamically allocated memory
	if (bIsAllocated) delete[] record;
}

// Return the data corresponding to the record.  Sets *pData to the
// record contents.
RC RM_Record::GetData(char *&pData) const {
	if (!bIsAllocated) return RM_INVALID_RECORD;
	pData = record;
	return OK_RC;
}

// Return the RID associated with the record
RC RM_Record::GetRid (RID &rid) const{
	if (!bIsAllocated) return RM_INVALID_RECORD;
	rid = this->rid;
	return OK_RC;
}