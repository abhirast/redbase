#include <cstdio>
#include <iostream>
#include <cstring>
#include <cmath>
#include "ix.h"
#include "ix_internal.h"

using namespace std;

IX_IndexScan::IX_IndexScan() {

}
IX_IndexScan::~IX_IndexScan() {

}

// Open index scan
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
    CompOp compOp, void *value, ClientHint  pinHint = NO_HINT) {

}

// Get the next matching entry return IX_EOF if no more matching
// entries.
RC IX_IndexScan::GetNextEntry(RID &rid) {

}

// Close index scan
RC IX_IndexScan::CloseScan() {
    
}