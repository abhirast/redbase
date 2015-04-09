#include <cstdio>
#include <iostream>
#include "rm_rid.h"

using namespace std;

// Default constructor
RID::RID() {
	pageNum = -1;
	slotNum = -1;
}

RID::RID(PageNum pageNum, SlotNum slotNum) {
	this->pageNum = pageNum;
	this->slotNum = slotNum;
}

// Destructor
RID::~RID(){}

// Return page number
RC RID::GetPageNum(PageNum &pageNum) const {
	// Error if RID not initialized using second constructor
	if (this->pageNum < 0) return RM_PAGE_UNINIT;
	pageNum = this->pageNum;
	return OK_RC;
}

// Return slot number
RC RID::GetSlotNum(SlotNum &slotNum) const {
	//Error if RID not initialized using second constructor
	if (this->slotNum < 0) return RM_SLOT_UNINIT;
	slotNum = this->slotNum;
	return OK_RC;	
}