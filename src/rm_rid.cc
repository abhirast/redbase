#include <cstdio>
#include <iostream>
#include "rm_rid.h"

using namespace std;

// Default constructor
RID::RID() {
	pageNum = RID_FLAG;
	slotNum = RID_FLAG;
}

RID::RID(PageNum pageNum, SlotNum slotNum) {
	this->pageNum = pageNum;
	this->slotNum = slotNum;
}

// Destructor
RID::~RID(){
	// Nothing to do
}

// Copy constructor
RID::RID(const RID &rid) {
	this->pageNum = rid.pageNum;
	this->slotNum = rid.slotNum;
}						   

// Assignment operator
RID& RID::operator=(const RID &rid) {
	if (this != &rid ) {
		this->pageNum = rid.pageNum;
		this->slotNum = rid.slotNum;	
	}
	return (*this);
}

bool RID::operator==(const RID &rid) const {
	return (this->pageNum == rid.pageNum) && (this->slotNum == rid.slotNum);
}

// Return page number
RC RID::GetPageNum(PageNum &pageNum) const {
	// Error if RID not initialized using second constructor
	if (this->pageNum == RID_FLAG) return RM_INVALID_RID;
	pageNum = this->pageNum;
	return OK_RC;
}

// Return slot number
RC RID::GetSlotNum(SlotNum &slotNum) const {
	//Error if RID not initialized using second constructor
	if (this->slotNum == RID_FLAG) return RM_INVALID_RID;
	slotNum = this->slotNum;
	return OK_RC;	
}