#include <cstdio>
#include <iostream>
#include "rm.h"

using namespace std;

RM_Record::RM_Record();
RM_Record::~RM_Record();

// Return the data corresponding to the record.  Sets *pData to the
// record contents.
RC RM_Record::GetData(char *&pData) const;

// Return the RID associated with the record
RM_Record::RC GetRid (RID &rid) const;
