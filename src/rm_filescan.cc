#include <cstdio>
#include <iostream>
#include <cstring>
#include "rm.h"
#include "rm_internal.h"

using namespace std;


/* macro for writing comparison operators
	This occurs in the body of the function having signature
	bool RM_FileScan::xx_op(void* attr)
	where xx = eq, lt, gt etc.
*/
#define RM_filescan_operator(expr) do { \
    char attr_value[attr_length+1];\
    buffer(attr, attr_value);\
    switch (attr_type) { \
        case INT:\
        	return *((int*) attr_value) expr *((int*) query_value);\
        case FLOAT: \
        	return *((float*) attr_value) expr *((float*) query_value);\
        case STRING: \
        	return string(attr_value) expr string(query_value);\
        default: return 1 == 1;\
    }\
}while(0)


RM_FileScan::RM_FileScan() {
	bIsOpen = 0;
}

RM_FileScan::~RM_FileScan() {
	if (bIsOpen) {
		delete[] query_value;
		delete[] bitmap_copy;
	}
}


/*  Initialize a file scan
	Steps-
	1. Check if the file handle object is open
	2. The the value pointer is null, set comparison operator to 
	   NO_OP
	3. Store the passed parameters
	4. Make the comp function pointer point to the correct member 
	   function
	TODO : Error if CLientHint invalid
*/
RC RM_FileScan::OpenScan(const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint) {
	// Set all the private members to initialize the scan
	if (fileHandle.bIsOpen == 0) return RM_FILE_NOT_OPEN;
	// check if the scan is already open
	if (bIsOpen) return RM_SCAN_OPEN_FAIL;
	// check if the compOp is valid
	if (compOp < NO_OP || compOp > GE_OP) return RM_SCAN_OPEN_FAIL;
	// check if the attribute type is valid
	if (attrType < INT || attrType > STRING) return RM_SCAN_OPEN_FAIL;
	// check if attr length and attr type are consistent
	if (((attrType == INT) && (attrLength != 4))
		|| ((attrType == FLOAT) && (attrLength != 4))
		|| ((attrType == STRING) && (attrLength > MAXSTRINGLEN)))
		return RM_SCAN_OPEN_FAIL;

	if (!value) compOp = NO_OP;
	rm_fh = &fileHandle;
	// check if attr length and offset are valid
	if (rm_fh->fHdr.record_length < attrLength + attrOffset 
		|| attrOffset < 0  || attrLength < 0 ) return RM_SCAN_OPEN_FAIL;
	attr_offset = attrOffset;
	attr_length = attrLength;
	attr_type = attrType;
	bIsOpen = 1;
	query_value = new char[attr_length + 1];
	if (value) buffer(value, query_value);
	pin_hint = pinHint;
	switch (compOp) {
		case NO_OP:
			comp = &RM_FileScan::no_op;
			break;
		case EQ_OP:
			comp = &RM_FileScan::eq_op;
			break;
		case NE_OP:
			comp = &RM_FileScan::ne_op;
			break;
		case LT_OP:
			comp = &RM_FileScan::lt_op;
			break;
		case GT_OP:
			comp = &RM_FileScan::gt_op;
			break;
		case LE_OP:
			comp = &RM_FileScan::le_op;
			break;
		case GE_OP:
			comp = &RM_FileScan::ge_op;
			break;
		default:
			comp = &RM_FileScan::no_op;
			break;
	}
	recs_seen = 0;
	num_recs = 0;
	current = fileHandle.fHdr.header_pnum;
	bitmap_copy = new char[fileHandle.fHdr.bitmap_size];
	return OK_RC;
}

/* 	Get next matching record
	Steps - 
	1. Check if the scan is open
	2. While EOF reached or record found do -
		(i) If number of records seen equals the total number
			of records, fetch the next non-empty page and save 
			its header. Set number of records seen to 0. Make
			the page number of this page the current page no.
		(ii) If the number of records seen is less than the
			total number of records, fetch the current page
		(iii) While no. of records seen is less than the total
			no. of records, read a valid record (use the bitmap),
			unset the bit in the bitmap to indicate that it has 
			been seen. Increment the no. of seen records. If the 
			record satisfies the condition, copy it to rec and
			return.

*/
RC RM_FileScan::GetNextRec(RM_Record &rec) {
	RC WARN = RM_EOF, ERR = RM_FILESCAN_FATAL; // used by macro
	if (!bIsOpen) return RM_SCAN_NOT_OPEN;
	if (!rm_fh->bIsOpen) return RM_FILE_NOT_OPEN;
	// if record was allocated earlier, delete it
	char *data;
	SlotNum dest;
	char* attr_position;
	int bFound = 0;
	//RM_FileHandle temp; // using for some non-const function access
	while (1) {
		if (recs_seen == num_recs) {
			RM_ErrorForward(GiveNewPage(data));
			recs_seen = 0;
		} else {
			RM_ErrorForward(rm_fh->pf_fh.GetThisPage(current, pf_ph));
			RM_ErrorForward(pf_ph.GetData(data));
		}
		while (recs_seen < num_recs) {
			dest = rm_fh->FindSlot(bitmap_copy);
			attr_position = data + rm_fh->fHdr.first_record_offset
							+ dest * rm_fh->fHdr.record_length 
							+ attr_offset;
			recs_seen ++;
			RM_ErrorForward(rm_fh->SetBit(bitmap_copy, dest));
			if ((this->*comp)(attr_position)) {
				if (rec.bIsAllocated) delete[] rec.record;
				rec.record = new char[rm_fh->fHdr.record_length];
				RM_ErrorForward(rm_fh->FetchRecord(data, rec.record, dest));
				rec.rid = RID(current, dest);
				rec.bIsAllocated = 1;
				bFound = 1;
				break;
			}
		}
		// Unpin page, new page will be allocated or we will exit
		RM_ErrorForward(rm_fh->pf_fh.UnpinPage(current));
		if (bFound) return OK_RC;
	}
}

RC RM_FileScan::CloseScan() {
	if (!bIsOpen) return RM_SCAN_NOT_OPEN;
	delete[] query_value;
	delete[] bitmap_copy;
	bIsOpen = 0;
	return OK_RC;
}

// Read a new non-blank page and update status variables
RC RM_FileScan::GiveNewPage(char *&data) {
	RC WARN = RM_EOF, ERR = RM_FILESCAN_FATAL; // used by macro
	do {
		RM_ErrorForward(rm_fh->pf_fh.GetNextPage(current, pf_ph));
		RM_ErrorForward(pf_ph.GetData(data));
		num_recs = ((RM_PageHdr *) data)->num_recs;
		RM_ErrorForward(pf_ph.GetPageNum(current));
		if (num_recs == 0) {
			RM_ErrorForward(rm_fh->pf_fh.UnpinPage(current));
		}
	} while(num_recs == 0);
	memcpy(bitmap_copy, data+rm_fh->fHdr.bitmap_offset, rm_fh->fHdr.bitmap_size);
	// Negate the bits because we are interested in taken slots
	for (int i = 0; i < rm_fh->fHdr.bitmap_size; i++) {
		*(bitmap_copy + i) = ~*(bitmap_copy+i);
	}
	return OK_RC;
}


void RM_FileScan::buffer(void *ptr, char* buff) {
    buff[attr_length] = '\0';
    memcpy(buff, ptr, attr_length);
    return;
}

bool RM_FileScan::no_op(void* attr) {
	return 1 == 1;
}
bool RM_FileScan::eq_op(void* attr) {
	RM_filescan_operator(==);
}
bool RM_FileScan::ne_op(void* attr) {
	RM_filescan_operator(!=);
}
bool RM_FileScan::lt_op(void* attr) {
	RM_filescan_operator(<);
}
bool RM_FileScan::gt_op(void* attr) {
	RM_filescan_operator(>);
}
bool RM_FileScan::le_op(void* attr) {
	RM_filescan_operator(<=);
}
bool RM_FileScan::ge_op(void* attr) {
	RM_filescan_operator(>=);
}