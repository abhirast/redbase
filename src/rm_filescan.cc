#include <cstdio>
#include <iostream>
#include "rm.h"

using namespace std;

RM_FileScan::RM_FileScan();
RM_FileScan::~RM_FileScan();

// Initialize a file scan
RC RM_FileScan::OpenScan(const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT); 

// Get next matching record
RC RM_FileScan::GetNextRec(RM_Record &rec);

RC RM_FileScan::CloseScan();
