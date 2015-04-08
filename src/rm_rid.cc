#include <cstdio>
#include <iostream>
#include "rm_rid.h"

using namespace std;

// Default constructor
RID::RID();

RID::RID(PageNum pageNum, SlotNum slotNum);

// Destructor
RID::~RID();

// Return page number
RC RID::GetPageNum(PageNum &pageNum) const;

// Return slot number
RC RID::GetSlotNum(SlotNum &slotNum) const;
