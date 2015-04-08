#include <cstdio>
#include <iostream>
#include "rm.h"

using namespace std;

RM_FileHandle::RM_FileHandle();
RM_FileHandle::~RM_FileHandle();

// Given a RID, return the record
RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const;

RC RM_FileHandle::InsertRec  (const char *pData, RID &rid);

RC RM_FileHandle::DeleteRec(const RID &rid);

RC RM_FileHandle::UpdateRec(const RM_Record &rec);

// Forces a page (along with any contents stored in this class)
// from the buffer pool to disk.  Default value forces all pages.
RC RM_FileHandle::ForcePages (PageNum pageNum = ALL_PAGES);
