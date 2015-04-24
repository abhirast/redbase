#include <cstdio>
#include <iostream>
#include <cstring>
#include <cmath>
#include "ix.h"
#include "ix_internal.h"

using namespace std;

IX_Manager::IX_Manager(PF_Manager &pfm) {

}
IX_Manager::~IX_Manager() {

}

// Create a new Index
RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
    AttrType attrType, int attrLength) {

}

// Destroy and Index
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo) {

}

// Open an Index
RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
    IX_IndexHandle &indexHandle) {

}

// Close an Index
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle) {
	
}