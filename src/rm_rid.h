//
// rm_rid.h
//
//   The Record Id interface
//

#ifndef RM_RID_H
#define RM_RID_H


// We separate the interface of RID from the rest of RM because some
// components will require the use of RID but not the rest of RM.

#include "redbase.h"

#ifndef RM_INVALID_RID
#define RM_INVALID_RID              (START_RM_WARN + 6)
#endif


# define RID_FLAG -1771

//
// PageNum: uniquely identifies a page in a file
//
typedef int PageNum;

//
// SlotNum: uniquely identifies a record in a page
//
typedef int SlotNum;

//
// RID: Record id interface
//
class RID {
public:
    RID();                                         // Default constructor
    RID(PageNum pageNum, SlotNum slotNum);
    ~RID();                                        // Destructor
    RID(const RID &rid);						   // Copy constructor
    RID& operator=(const RID &rid);				   // Assignment operator
    bool operator==(const RID &rid) const;         // Equality operator
    RC GetPageNum(PageNum &pageNum) const;         // Return page number
    RC GetSlotNum(SlotNum &slotNum) const;         // Return slot number

private:
	// negative values indicate absence of attribute
	PageNum pageNum;
	SlotNum slotNum;
};

#endif
