#include <cstdio>
#include <iostream>
#include <cstring>
#include <cmath>
#include <assert.h>
#include "ix.h"
#include "ix_internal.h"

using namespace std;

/* macro for writing comparison operators
	This occurs in the body of the function having signature
	bool IX_IndexHandle::xx_op(void* attr1, void* attr2)
	where xx = eq, lt, gt etc.
*/
#define IX_operator(expr) do { \
    char attr_value1[fHdr.attrLength+1];\
    buffer(attr1, attr_value1);\
    char attr_value2[fHdr.attrLength+1];\
    buffer(attr2, attr_value2);\
    switch (fHdr.attrType) { \
        case INT:\
        	return *((int*) attr_value1) expr *((int*) attr_value2);\
        case FLOAT: \
        	return *((float*) attr_value1) expr *((float*) attr_value2);\
        case STRING: \
        	return string(attr_value1) expr string(attr_value2);\
        default: return 1 == 0;\
    }\
}while(0)



IX_IndexHandle::IX_IndexHandle() {

}
IX_IndexHandle::~IX_IndexHandle() {

}

/*	Steps-
	1. check if pData is valid 
	2. if a root doesn't exist, create it
	3. insert data into the root (recursively inserts deeper)
	4. If pData is null, return
	5. If pData is not null, create a new root, with pData in it
	6. Update the file header
*/
RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid) {

}

// Delete a new index entry
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid) {

}

// Force index files to disk
RC IX_IndexHandle::ForcePages() {

}



//
// Private methods
//


// copies the contents into char array and null terminates it
void IX_IndexHandle::buffer(void *ptr, char* buff) {
    buff[attr_length] = '\0';
    memcpy(buff, ptr, attr_length);
    return;
}


// operators for comparison
bool IX_IndexHandle::eq_op(void* attr1, void* attr2) {
	IX_operator(==);
}
bool IX_IndexHandle::ne_op(void* attr1, void* attr2) {
	IX_operator(!=);
}
bool IX_IndexHandle::lt_op(void* attr1, void* attr2) {
	IX_operator(<);
}
bool IX_IndexHandle::gt_op(void* attr1, void* attr2) {
	IX_operator(>);
}
bool IX_IndexHandle::le_op(void* attr1, void* attr2) {
	IX_operator(<=);
}
bool IX_IndexHandle::ge_op(void* attr1, void* attr2) {
	IX_operator(>=);
}

/*  Sets res to the index of the smallest key >= query. Sets it
	to cap if all keys are smaller than query
*/
bool IX_IndexHandle::findKey(char* keys, void* query, int cap, int& res) {
	bool found = false;
	void *ptr;
	for (res = 0; res < cap; res++) {
		ptr  = (void*) (keys + res * fHdr.attrLength);
		if (ge_op(ptr, query) {
			found = eq_op(ptr, query);
			return found;
		}
	}
	return found;
}

/*
Shifts num_units units starting from beg by 1 unit. This is used
in insert for making space for a key or pointer
*/
void IX_IndexHandle::shiftBytes(char* beg, int unit_size, int num_units) {
	int num_bytes = num_units * unit_size;
	char *dest_st = beg + unit_size;
	memmove(dest_st, beg, num_bytes);
}

/*
Finds if there is a duplicate key in the key array. This method is called
while a leaf page is to be split to check whether a split could be avoided
by sending duplicate keys to overflow pages.
*/
bool IX_IndexHandle::checkDuplicates(char* keys, int num_keys,
							int &max_count, int &most_repeated_index) {
	max_count = 1;
	most_repeated_index = 0;
	int curr_count = 1;
	void* prev, curr;
	prev = (void*) keys;
	for (int i = 1; i < num_keys; i++) {
		curr = (void*) (keys + i * fHdr.attrLength);
		if (eq_op(prev, curr)) {
			curr_count++;
			if (max_count > curr_count) {
				max_count = curr_count;
				most_repeated_index = i;
			}
		} else {
			curr_count = 0;
		}
		prev = curr;
	}
	return (max_count > 1);
}






/* 	Recursively inserts key contained in pData into Page pnum
	Cases - 
	1. If the page type is LEAF
		(i) If the key to be inserted is not already present
			(a) If there is space, insert it, make pData null
			(b) If there is no space, 
				(I) Check if sending duplicate keys into overflow
					page will free up space. If it does, allocate
					an overflow page for duplicate entry with 
					highest frequency
				(II) If it doesn't allocate a new LEAF page, move 
					half of the entries to the new LEAF page and copy the 
					key corresponding to the minimum  value in the 
					new LEAF page to pData. This part can be safely done 
					because the previous part ensures duplicate keys wont
					be redistributed
		(ii) If the key to be inserted is already present
			(a) If it has a overflow page, insert the entry into
				the overflow page and make pData null
			(b) If there is no overflow page and there is sufficient
				place in the page, insert and make pData null
			(c) If there is no overflow page and there is no place 
				in the LEAF page, create an overflow page and move all
				RIDs to the new page along with the new RID. No need to 
				store key in the overflow page, make pData null
				TODO : Be more clever
	2. If the page type is OVERFLOW
		(i) If there is space in the overflow page, insert it and 
			make pData null
		(ii) If there is no space in the overflow page
			(a) If it has a next overflow page, insert it in that and
				make pData null.
			(b) If there is no next overflow page, allocate a new page
				and insert into that, make pData null
	3. If the page type is INTERNAL
		(i) Find the appropriate child page number. Call insert on that
			page.
		(ii) If pData is null, return.
		(iii) If pData is not null
			(a) If there is space on the current page, insert pData there
				and make pData null
			(b) If there is no space, allocate a new INTERNAL page, move
				half the entries to new INTERNAL page and copy into pData 
				the key corresponding to the mid value. The mid-key should 
				not	exist in any of the pages. 
	4. Unpin the appropriate pages in all the steps
*/
RC IX_IndexHandle::InsertEntry(PageNum pnum, void *&pData, const RID &rid) {
	RC WARN = IX_INSERT_WARN, ERR = IX_INSERT_ERR;
	PF_PageHandle ph;
	// fetch the page
	char* page;
	int pnum;
	IX_ErrorForward(pf_fh.GetThisPage(pnum));
	IX_ErrorForward(ph.GetData(page));
	IPageType* ptype = (IPageType*) page;

	// leaf page insert
	if (&ptype == LEAF) {
		// header is located in the beginning of the page
		IX_LeafHdr *pHdr = (IX_LeafHdr*) page;
		int index, num_keys = pHdr->num_keys;
		char *keys = page + sizeof(IX_LeafHdr);
		char *pointers = keys + fHdr.attrLength * fHdr.leaf_capacity;
		// find the appropriate key in the page
		bool found = findKey(keys, pData, num_keys, index);
		// key to be inserted not already present
		if (!found) {
			// if there is space, insert at indexth entry
			// move entries on and after index right
			if (num_keys < fHdr.leaf_capacity) {
				shiftBytes(keys + index * fHdr.attrLength, 
					fHdr.attrLength, num_keys - index);
				memcpy(keys + index * fHdr.attrLength, pData, fHdr.attrLength);
				shiftBytes(pointers + index * sizeof(RID), 
					sizeof(RID), num_keys - index);
				memcpy(pointers + index * sizeof(RID), (void*) &rid, sizeof(RID));
				pData = 0;
				pHdr->num_keys++;
				IX_ErrorForward(pf_fh.UnpinPage(pnum));
				return OK_RC;
			}
			// if there is no space, allocate a new page, split the page
			else if (num_keys == fHdr.leaf_capacity) {
				// check if duplicate keys are present
				int dup_count, dup_index;
				if (checkDuplicates(keys, fHdr.leaf_capacity, dup_count, dup_index)) {
					// create an overflow page and put the duplicate key in it

				}
				else {
					// allocate a new page and get its data
					PF_PageHandle newph;
					pf_fh.AllocatePage(newph);
					char* newpage;
					int newpnum;
					IX_ErrorForward(newph.GetPageNum(newpnum));
					IX_ErrorForward(newph.GetData(newpage));
					char *newkeys, *newpointers;
					newkeys = newpage + sizeof(IX_LeafHdr);
					newpointers = newkeys + fHdr.attrLength * fHdr.leaf_capacity;
					// update the page pointers
					IX_LeafHdr *newpHdr = (IX_LeafHdr*) newpage;
					newpHdr->left_pnum = pnum;
					newpHdr->right_pnum = pHdr->right_pnum;
					pHdr->right_pnum = newpnum;
					// update the page type
					newpHdr->type = LEAF;
					// move half the entries to new page
					int tokeep = fHdr.leaf_capacity/2; // num keys to be kept
					int togive = fHdr.leaf_capacity - tokeep;
					memcpy(newkeys, keys + fHdr.attrLength * tokeep, 
								fHdr.attrLength * togive);
					memcpy(newpointers, pointers + sizeof(RID) * tokeep, 
								sizeof(RID) * togive);
					// update the record count
					pHdr->num_keys = tokeep;
					newpHdr->num_keys = togive;
					// insert the new record in the appropriate page
					if (index <= tokeep) {
						// insert the new record in the current page
						num_keys = tokeep;
						shiftBytes(keys + index * fHdr.attrLength, 
								fHdr.attrLength, num_keys - index);
						memcpy(keys + index * fHdr.attrLength, pData, fHdr.attrLength);
						shiftBytes(pointers + index * sizeof(RID), 
								sizeof(RID), num_keys - index);
						memcpy(pointers + index * sizeof(RID), (void*) &rid, sizeof(RID));
							pHdr->num_keys++;
					} else {
						// insert in the new page
						index -= (tokeep + 1);
						num_keys = togive;
						shiftBytes(newkeys + index * fHdr.attrLength, 
							fHdr.attrLength, num_keys - index);
						memcpy(newkeys + index * fHdr.attrLength, pData, fHdr.attrLength);
						shiftBytes(newpointers + index * sizeof(RID), 
							sizeof(RID), num_keys - index);
						memcpy(newpointers + index * sizeof(RID), (void*) &rid, sizeof(RID));
						newpHdr->num_keys++;
					}
					pData = 0;
					IX_ErrorForward(pf_fh.UnpinPage(pnum));
					IX_ErrorForward(pf_fh.UnpinPage(newpnum));
					return OK_RC;
				}
			}
		}
		// key to be inserted already present
		else if (found) {
			// get the corresponding rid, located at index-1 position
			RID* recid = pointers + (index-1) * sizeof(RID);
			int opage, oslot;
			IX_ErrorForward(recid->GetSlotNum(oslot));
			IX_ErrorForward(recid->GetPageNum(opage));
			// negative slot indicates presence of overflow page
			if (oslot < 0) {
				// recursively insert in the overflow page
				RC ret = InsertEntry(opage, pData, rid);
				pData = 0;
				return ret;
			}
			// if there is no overflow page and there is space, normally
			// insert the data
			else if (num_keys < fHdr.leaf_capacity) {
				shiftBytes(keys + index * fHdr.attrLength, 
					fHdr.attrLength, num_keys - index);
				memcpy(keys + index * fHdr.attrLength, pData, fHdr.attrLength);
				shiftBytes(pointers + index * sizeof(RID), 
					sizeof(RID), num_keys - index);
				memcpy(pointers + index * sizeof(RID), (void*) &rid, sizeof(RID));
				pData = 0;
				pHdr->num_keys++;
				IX_ErrorForward(pf_fh.UnpinPage(pnum));
				return OK_RC;
			} 
			// if the page is full and there is no overflow page, create one
			// and put pointers of all the duplicate keys in this page. Only
			// keep a single copy of the key in leaf page and set its slotnum
			// to -1
			else if (num_keys == fHdr.leaf_capacity) {
				// create an overflow page
				// allocate a new page and get its data
				PF_PageHandle overflow_handle;
				pf_fh.AllocatePage(overflow_handle);
				char *op_data, *op_rids;
				int op_num;
				IX_ErrorForward(overflow_handle.GetPageNum(op_num));
				IX_ErrorForward(overflow_handle.GetData(op_data));
				IX_OverflowHdr *oHdr = (IX_OverflowHdr*) op_data;
				// iterate through all the keys and copy the rids
				op_rids = op_data + sizeof(IX_OverflowHdr);
				int first_seen = -1;
				for (int i = 0; i < index; i++) {
					if (eq_op(pData, keys + i * fHdr.attrLength, fHdr.attrLength)) {
						memcpy(op_rids, pointers + i * sizeof(RID), sizeof(RID));
						op_rids += sizeof(RID);
						if (first_seen < 0) first_seen = i;
					}
				}
				// put the passed in rid
				memcpy(op_rids, &rid, sizeof(RID));
				// remove duplicate keys and pointers
				memmove(keys + (first_seen + 1) * fHdr.attrLength, 
					keys + index * fHdr.attrLength, (index - first_seen - 1) * fHdr.attrLength);
				memmove(pointers + (first_seen + 1) * sizeof(RID), 
					pointers + index * sizeof(RID), (index - first_seen - 1) * sizeof(RID));
				// update the page headers
			 	pHdr->num_keys -= (index - first_seen - 1);
			 	oHdr->num_rids = index - first_seen + 1;
			 	oHdr->type = OVERFLOW;
			 	oHdr->next_page = IX_SENTINEL;
			 	// update the rid in leaf page
			 	RID *temp_rid = (RID*) (pointers + first_seen * sizeof(RID));
			 	*temp_rid = RID(op_num, -1);
			 	// unpin the pages
			 	pData = 0;
				IX_ErrorForward(pf_fh.UnpinPage(pnum));
				IX_ErrorForward(pf_fh.UnpinPage(op_num));
				return OK_RC;
			}
		}
	}	
	// overflow page insert
	else if (&ptype == OVERFLOW) {
		// header is located in the beginning of the page
		IX_OverflowHdr *pHdr = (IX_OverflowHdr*) page;
		int index;
		char *rids = page + sizeof(IX_OverflowHdr);

		// if the overflow page has space
		if (pHdr->num_rids < fHdr.overflow_capacity) {
			// insert it and make pData null
			memcpy(rids + num_rids * sizeof(RID), &rid, sizeof(RID));
			pHdr->num_rids++;
			pData = 0;
			IX_ErrorForward(pf_fh.UnpinPage(pnum));
			return OK_RC;
		}
		// if there is no space in overflow page
		else if (pHdr->num_rids == fHdr.overflow_capacity) {
			// if there is a next page, call the function on that
			if (pHdr->next_page != IX_SENTINEL) {
				RC ret = InsertEntry(pHdr->next_page, pData, rid);
				pData = 0;
				return ret;
			}
			// if no new overflow page, allocate a new one and insert in that
			else if (pHdr->next_page == IX_SENTINEL) {
				PF_PageHandle overflow_handle;
				pf_fh.AllocatePage(overflow_handle);
				char *op_data, *op_rids;
				int op_num;
				IX_ErrorForward(overflow_handle.GetPageNum(op_num));
				IX_ErrorForward(overflow_handle.GetData(op_data));
				IX_OverflowHdr *oHdr = (IX_OverflowHdr*) op_data;
				op_rids = op_data + sizeof(IX_OverflowHdr);
				oHdr->type = OVERFLOW;
				oHdr->next_page = IX_SENTINEL;
				oHdr->num_rids = 1;
				memcpy(op_rids, &rid, sizeof(RID));
				// unpin the pages
			 	pData = 0;
				IX_ErrorForward(pf_fh.UnpinPage(pnum));
				IX_ErrorForward(pf_fh.UnpinPage(op_num));
				return OK_RC;
			}
		}
	} 

	// internal page insert
	else if (&ptype == INTERNAL) {
		// header is located in the beginning of the page
		IX_InternalHdr *pHdr = (IX_InternalHdr*) page;
		int index, num_keys = pHdr->num_keys;
		char *keys = page + sizeof(IX_InternalHdr);
		char *pointers = keys + fHdr.attrLength * fHdr.internal_capacity;
		// find the appropriate key in the page
		bool found = findKey(keys, pData, num_keys, index);
			
	} 

	// invalid page type seen
	else {
		return IX_PAGE_TYPE_ERROR;
	}
} 
