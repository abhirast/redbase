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
	bIsOpen = 0;
	bHeaderChanged = 0;
}


IX_IndexHandle::~IX_IndexHandle() {
	// nothing to do
}

/*	Steps-
	1. check if pData is valid 
	2. if a root doesn't exist, create it
	3. insert data into the root (recursively inserts deeper)
	4. if newpage is negative, return
	5. else, create a new root, with pData in it
	6. Update the file header
*/
    
RC IX_IndexHandle::InsertEntry(void *pData, const RID &rid) {
	RC WARN = IX_INSERT_WARN, ERR = IX_INSERT_ERR;
	if (!pData) return IX_INVALID_INSERT_PARAM;
	if (!bIsOpen) return IX_INDEX_CLOSED;
	PF_PageHandle root_handle;
	if (fHdr.root_pnum < 0) {
		// no root exists, create a root
		// declared as a leaf
		pf_fh.AllocatePage(root_handle);
		char* newdata;
		PageNum newpnum;
		IX_ErrorForward(root_handle.GetPageNum(newpnum));
		IX_ErrorForward(root_handle.GetData(newdata));
		IX_ErrorForward(pf_fh.MarkDirty(newpnum));
		IX_LeafHdr* pHdr = (IX_LeafHdr*) newdata;
		fHdr.root_pnum = newpnum;
		bHeaderChanged = 1;
		pHdr->num_keys = 0;
		pHdr->left_pnum = IX_SENTINEL;
		pHdr->right_pnum = IX_SENTINEL;
		pHdr->type = LEAF;
	} else {
		pf_fh.GetThisPage(fHdr.root_pnum, root_handle);
	}
	// insert into the root
	int newpage;
	char *data;
	IX_ErrorForward(root_handle.GetData(data));
	IPageType type = *((IPageType*) data);
	IX_ErrorForward(treeInsert(root_handle, pData, rid, newpage));
	IX_ErrorForward(pf_fh.UnpinPage(fHdr.root_pnum));
	if (newpage < 0) {
		// successfully inserted, no new root to be created
		return OK_RC;
	}
	else {
		// create a new root
		PF_PageHandle new_root;
		pf_fh.AllocatePage(new_root);
		char *newdata, *keys, *pointers;
		PageNum newpnum;
		IX_ErrorForward(new_root.GetPageNum(newpnum));
		IX_ErrorForward(new_root.GetData(newdata));
		IX_ErrorForward(pf_fh.MarkDirty(newpnum));
		IX_InternalHdr* pHdr = (IX_InternalHdr*) newdata;
		keys = newdata + sizeof(IX_InternalHdr);
		pointers = keys + fHdr.attrLength * fHdr.internal_capacity;
		// insert the key and pointers into new root
		memcpy(keys, pData, fHdr.attrLength);
		pHdr->left_pnum = fHdr.root_pnum;
		memcpy(pointers, &newpage, sizeof(PageNum));
		// update the pages and headers
		pHdr->type = INTERNAL;
		pHdr->num_keys = 1;
		// make the new page the root page
		fHdr.root_pnum = newpnum;
		bHeaderChanged = 1;
		// unpin the new root
		IX_ErrorForward(pf_fh.UnpinPage(fHdr.root_pnum));
		return OK_RC;
	}
	return WARN; //should not reach here
}

/* 
Delete a new index entry.
Steps - 
*/
RC IX_IndexHandle::DeleteEntry(void *pData, const RID &rid) {
	RC WARN = IX_DELETE_WARN, ERR = IX_DELETE_ERR;
	if (!pData) return IX_NULL_KEY;
	if (!bIsOpen) return IX_INDEX_CLOSED;
	if (fHdr.root_pnum < 0) return IX_REC_NOT_FOUND;
	// get the root page
	PF_PageHandle root_handle;
	int numKeys;
	IX_ErrorForward(pf_fh.GetThisPage(fHdr.root_pnum, root_handle));
	WARN = IX_REC_NOT_FOUND;
	IX_ErrorForward(treeDelete(root_handle, pData, rid, numKeys));
	WARN = IX_DELETE_WARN;
	// the root page is never deleted even if it becomes empty
	IX_ErrorForward(pf_fh.UnpinPage(fHdr.root_pnum));
	last_deleted = rid;
	return OK_RC;
}

// Force index files to disk
RC IX_IndexHandle::ForcePages() {
	RC WARN = IX_FORCEPAGE_WARN, ERR = IX_FORCEPAGE_ERR;
	if (!bIsOpen) return IX_INDEX_CLOSED;
	IX_ErrorForward(pf_fh.ForcePages(ALL_PAGES));
	return OK_RC;
}



//
// Private methods
//


RC IX_IndexHandle::treeDelete(PF_PageHandle &ph, void *pData, const RID& rid, int& numKeys) {
	RC WARN = IX_TREE_DELETE_WARN, ERR = IX_TREE_DELETE_ERR;
	char *data;
	IX_ErrorForward(ph.GetData(data));
	IX_InternalHdr *pHdr = (IX_InternalHdr*) data;

	if (pHdr->num_keys == 0) {
		return IX_REC_NOT_FOUND;
	}
	// base case, leaf reached
	if (pHdr->type == LEAF) {
		WARN = IX_REC_NOT_FOUND;
		IX_ErrorForward(leafDelete(ph, pData, rid, numKeys));
		WARN = IX_TREE_DELETE_WARN;
		return OK_RC;
	}
	char *keys = data + sizeof(IX_InternalHdr);
	char *pointers = keys + fHdr.attrLength * fHdr.internal_capacity;
	int index;
	bool found = findKey(keys, pData, pHdr->num_keys, index);
	// call the function on the appropriate page
	PageNum child_pnum;
	int page_called;
	if (found) {
		// the pointer index is correct
		memcpy(&child_pnum, pointers + index * sizeof(PageNum), sizeof(PageNum));
		page_called = index;
	} else if (index == 0) {
		child_pnum = pHdr->left_pnum;
		page_called = -1;
	} else {
		index--;
		memcpy(&child_pnum, pointers + index * sizeof(PageNum), sizeof(PageNum));
		page_called = index;
	}

	// get the appropriate child and call the function
	PF_PageHandle cph;
	IX_ErrorForward(pf_fh.GetThisPage(child_pnum, cph));
	WARN = IX_REC_NOT_FOUND;
	IX_ErrorForward(treeDelete(cph, pData, rid, numKeys));
	WARN = IX_TREE_DELETE_WARN;
	IX_ErrorForward(pf_fh.UnpinPage(child_pnum));
	/*
	if (numKeys == 0) {
		// dispose the child page
		IX_ErrorForward(pf_fh.DisposePage(child_pnum));
		int temp_pnum;
		IX_ErrorForward(ph.GetPageNum(temp_pnum));
		IX_ErrorForward(pf_fh.MarkDirty(temp_pnum));
		// remove from the current page too
		if (page_called == -1) {
			memcpy(&(pHdr->left_pnum), pointers, sizeof(PageNum));
			page_called = 0;
		}
		// delete the key and pointer at index page_called
		if (page_called < pHdr->num_keys - 1) {
			index = page_called + 1;
			int to_shift = pHdr->num_keys - index;
			shiftBytes(keys + index * fHdr.attrLength, 
						fHdr.attrLength, to_shift, -1);
			shiftBytes(pointers + index * sizeof(PageNum), 
							sizeof(PageNum), to_shift, -1);	
		}
		pHdr->num_keys--;
	}
	*/
	numKeys = pHdr->num_keys;
	return OK_RC;
}

/*
Iterate through all the keys, if key mathches, check rid. If rid denotes 
overflow, call on overflow.
*/
RC IX_IndexHandle::leafDelete(PF_PageHandle &ph, void *pData, const RID& rid, int& numKeys) {
	RC WARN = IX_LEAF_DELETE_WARN, ERR = IX_LEAF_DELETE_ERR;
	char *data;
	IX_ErrorForward(ph.GetData(data));
	IX_LeafHdr *pHdr = (IX_LeafHdr*) data;
	char *keys = data + sizeof(IX_LeafHdr);
	char *rids = keys + fHdr.attrLength * fHdr.leaf_capacity;
	int index;
	bool found = findKey(keys, pData, pHdr->num_keys, index);
	if (!found) return IX_REC_NOT_FOUND;
	// get the rid of the found key
	RID* cand = (RID*) (rids + index * sizeof(RID));
	int page, slot;
	IX_ErrorForward(cand->GetPageNum(page));
	IX_ErrorForward(cand->GetSlotNum(slot));
	if (slot < 0) {
		// found key has an overflow page
		PF_PageHandle nextph;
		IX_ErrorForward(pf_fh.GetThisPage(page, nextph));
		WARN = IX_REC_NOT_FOUND;
		IX_ErrorForward(overflowDelete(nextph, rid, numKeys));
		WARN = IX_LEAF_DELETE_WARN;
		IX_ErrorForward(pf_fh.UnpinPage(page));
		if (numKeys == 0) {
			int temp_pnum;
			IX_ErrorForward(ph.GetPageNum(temp_pnum));
			IX_ErrorForward(pf_fh.MarkDirty(temp_pnum));
			// delete entry from the leaf
			if (index < pHdr->num_keys - 1) {
				index += 1;
				int to_shift = pHdr->num_keys - index;
				shiftBytes(keys + index * fHdr.attrLength, 
							fHdr.attrLength, to_shift, -1);
				shiftBytes(rids + index * sizeof(RID), 
								sizeof(RID), to_shift, -1);	
			}
			pHdr->num_keys--;
			// delete the overflow page
			pf_fh.DisposePage(page);
			return OK_RC;
		}
		else {
			return OK_RC;
		}
	} 
	else {
		// found key not an overflow key
		found = false;
		for (int i = index; i < pHdr->num_keys; i++) {
			if (ne_op(pData, (void*)(keys + i * fHdr.attrLength))) break;
			// check if the rid is equal
			RID* temp_rid = (RID*) (rids + i * sizeof(RID));
			if (*temp_rid == rid) {
				found = true;
				if (i < pHdr->num_keys - 1) {
					i += 1;
					int to_shift = pHdr->num_keys - i;
					shiftBytes(keys + i * fHdr.attrLength, 
							fHdr.attrLength, to_shift, -1);
					shiftBytes(rids + i * sizeof(RID), sizeof(RID), 
								to_shift, -1);	
				}
				break;
			}
		}
		if (!found) return IX_REC_NOT_FOUND;
	}
	// Mark the current page dirty as some entry got deleted
	int temp_pnum;
	IX_ErrorForward(ph.GetPageNum(temp_pnum));
	IX_ErrorForward(pf_fh.MarkDirty(temp_pnum));
	/*
	if (pHdr->num_keys == 0) {
		// update the pointers of the nearby pages
		PF_PageHandle newph;
		char *newdata;
		IX_ErrorForward(pf_fh.GetThisPage(pHdr->left_pnum, newph));
		IX_ErrorForward(newph.GetData(newdata));
		IX_LeafHdr* newpHdr = (IX_LeafHdr*) newdata;
		newpHdr->right_pnum = pHdr->right_pnum;
		IX_ErrorForward(pf_fh.UnpinPage(pHdr->left_pnum));
		IX_ErrorForward(pf_fh.GetThisPage(pHdr->right_pnum, newph));
		IX_ErrorForward(newph.GetData(newdata));
		newpHdr = (IX_LeafHdr*) newdata;
		newpHdr->left_pnum = pHdr->left_pnum;
		IX_ErrorForward(pf_fh.UnpinPage(pHdr->right_pnum));
	}
	// will be deleted by the parent if no. of keys is 0
	*/
	pHdr->num_keys--;
	numKeys = pHdr->num_keys;
	return OK_RC;
}

RC IX_IndexHandle::overflowDelete(PF_PageHandle &ph, const RID& rid, int& numKeys) {
	RC WARN = IX_OVERFLOW_DELETE_WARN, ERR = IX_OVERFLOW_DELETE_ERR;
	char *data;
	IX_ErrorForward(ph.GetData(data));
	IX_OverflowHdr* pHdr = (IX_OverflowHdr*) data;
	char* rids = data + sizeof(IX_OverflowHdr);

	// find the key and delete it
	bool found;
	RID* ptr;
	for (int i = 0; i < pHdr->num_rids; i++) {
		ptr = (RID*) (rids + i * sizeof(RID));
		if (*ptr == rid) {
			found = true;
			if (i < pHdr->num_rids - 1) {
				i += 1;
				int to_shift = pHdr->num_rids - i;
				shiftBytes(rids + i * sizeof(RID), sizeof(RID), to_shift, -1);	
			}
			pHdr->num_rids--;
			break;
		}
	}
	if (found) {
		int temp_pnum;
		IX_ErrorForward(ph.GetPageNum(temp_pnum));
		IX_ErrorForward(pf_fh.MarkDirty(temp_pnum));
	}

	// if key not found
	if (!found) {
		// if this is the last page
		if(pHdr->next_page == IX_SENTINEL) {
			return IX_REC_NOT_FOUND;
		}
		// call on the next page
		PF_PageHandle nextph;
		IX_ErrorForward(pf_fh.GetThisPage(pHdr->next_page, nextph));
		WARN = IX_REC_NOT_FOUND;
		IX_ErrorForward(overflowDelete(nextph, rid, numKeys));
		WARN = IX_OVERFLOW_DELETE_WARN;
		IX_ErrorForward(pf_fh.UnpinPage(pHdr->next_page));
		if (numKeys == 0) {
			// next page has become empty, delete it
			IX_ErrorForward(pf_fh.DisposePage(pHdr->next_page));
		}
		return OK_RC;
	}
	// if key is found, it got deleted
	if (pHdr->num_rids == 0) {
		// if page got empty
		if (pHdr->next_page != IX_SENTINEL) {
			// if next page exists, copy its contents and delete the next page
			PF_PageHandle nextph;
			char *nextdata;
			int nextpnum = pHdr->next_page;
			IX_ErrorForward(pf_fh.GetThisPage(nextpnum, nextph));
			IX_ErrorForward(nextph.GetData(nextdata));
			memcpy(data, nextdata, PF_PAGE_SIZE);
			pf_fh.DisposePage(nextpnum);
		}
		// header automatically got updated as everything was copied
	}
	numKeys = pHdr->num_rids;
	return OK_RC;
}

// copies the contents into char array and null terminates it
void IX_IndexHandle::buffer(void *ptr, char* buff) const{
    buff[fHdr.attrLength] = '\0';
    memcpy(buff, ptr, fHdr.attrLength);
    return;
}


// operators for comparison
bool IX_IndexHandle::eq_op(void* attr1, void* attr2) const{
	IX_operator(==);
}
bool IX_IndexHandle::ne_op(void* attr1, void* attr2) const{
	IX_operator(!=);
}
bool IX_IndexHandle::lt_op(void* attr1, void* attr2) const{
	IX_operator(<);
}
bool IX_IndexHandle::gt_op(void* attr1, void* attr2) const{
	IX_operator(>);
}
bool IX_IndexHandle::le_op(void* attr1, void* attr2) const{
	IX_operator(<=);	
}
bool IX_IndexHandle::ge_op(void* attr1, void* attr2) const{
	IX_operator(>=);
}

/*  Sets res to the index of the first key >= query. Sets it
	to cap if all keys are smaller than query
*/
bool IX_IndexHandle::findKey(char* keys, void* query, int cap, int& res) const {
	bool found = false;
	void *ptr;
	for (res = 0; res < cap; res++) {
		ptr  = (void*) (keys + res * fHdr.attrLength);
		if (ge_op(ptr, query)) {
			found = eq_op(ptr, query);
			return found;
		}
	}
	return found;
}

/*
Shifts num_units units starting from beg by 1 unit. This is used
in insert for making space for a key or pointer. To be used only if 
there is space in the array
*/
void IX_IndexHandle::shiftBytes(char* beg, int unit_size, 
			int num_units, int shift_units) {
	int num_bytes = num_units * unit_size;
	char *dest_st = beg + shift_units * unit_size;
	memmove(dest_st, beg, num_bytes);
}

/*
Finds if there is a duplicate key in the key array. This method is called
while a leaf page is to be split to check whether a split could be avoided
by sending duplicate keys to overflow pages.
*/
int IX_IndexHandle::checkDuplicates(char* keys, int num_keys,
							int &most_repeated_index) {
	most_repeated_index = 0;
	int curr_count = 1, max_count = 1;
	void *prev, *curr;
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
	return max_count;
}

/*
Inserts specified data at the given index by shifting the elements of the
array on the right by one slot. To be called when space is  available and 
duplicates dont need to be eliminated otherwise may overwrite or overflow.
*/
void IX_IndexHandle::arrayInsert(char* array, void* data, int size, 
						int index, int cap) {
	shiftBytes(array + index * size, size, cap - index, 1);
	memcpy(array + index * size, data, size);	
}


/*
Splits a leaf page by distributing half the keys to a new page. Doesn't
do any checks about duplicates. Then updates the pages and unpins the 
new page
*/
RC IX_IndexHandle::splitLeaf(char* page, int pnum, void* &pData, 
						const RID &rid, int &newpage) {
	RC WARN = IX_SPLIT_LEAF_WARN, ERR = IX_SPLIT_LEAF_ERR;
	
	// allocate a new page
	PF_PageHandle newph;
	IX_ErrorForward(pf_fh.AllocatePage(newph));
	
	char* newdata;
	int newpnum;
	IX_ErrorForward(newph.GetPageNum(newpnum));
	
	IX_ErrorForward(newph.GetData(newdata));
	char *newkeys, *newpointers;
	newkeys = newdata + sizeof(IX_LeafHdr);
	newpointers = newkeys + fHdr.attrLength * fHdr.leaf_capacity;

	// Mark both the pages dirty
	IX_ErrorForward(pf_fh.MarkDirty(pnum));
	IX_ErrorForward(pf_fh.MarkDirty(newpnum));

	// get orig page keys and pointers
	char *keys, *pointers;
	keys = page + sizeof(IX_LeafHdr);
	pointers = keys + fHdr.attrLength * fHdr.leaf_capacity;

	int index;
	bool found = findKey(keys, pData, fHdr.leaf_capacity, index);
	if (found) {
		// should not be found, otherwise an overflow page would have been
		// generated
		return WARN;
	}
	
	// update the page headers
	IX_LeafHdr *newpHdr = (IX_LeafHdr*) newdata;
	IX_LeafHdr *pHdr = (IX_LeafHdr*) page;
	newpHdr->left_pnum = pnum;
	newpHdr->right_pnum = pHdr->right_pnum;
	pHdr->right_pnum = newpnum;
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
		arrayInsert(keys, (void*) pData, fHdr.attrLength, index, tokeep);
		arrayInsert(pointers, (void*) &rid, sizeof(RID), index, tokeep);
		pHdr->num_keys++;
	} else {
		// insert in the new page
		index -= tokeep;
		arrayInsert(newkeys, (void*) pData, fHdr.attrLength, index, togive);
		arrayInsert(newpointers, (void*) &rid, sizeof(RID), index, togive);
		newpHdr->num_keys++;
	}
	// set pData to the first key of the new page
	memcpy(pData, (void*) newkeys, fHdr.attrLength);
	newpage = newpnum;
	IX_ErrorForward(pf_fh.UnpinPage(newpnum));
	return OK_RC;	
}

/*
Splits an internal page by distributing half the keys to a new page. A
new key contained in pData and the page number contained in newpage
is inserted at the appropriate place. Page headers are updated and the
new page is unpinned
*/
RC IX_IndexHandle::splitInternal(char* page, void* &pData, 
					int &newpagenum) {
	RC WARN = IX_SPLIT_INT_WARN, ERR = IX_SPLIT_INT_ERR;
	// allocate a new page
	PF_PageHandle newph;
	pf_fh.AllocatePage(newph);
	char* newpage;
	PageNum newpnum;
	IX_ErrorForward(newph.GetPageNum(newpnum));
	IX_ErrorForward(newph.GetData(newpage));
	// Mark the new page dirty, the old page is marked dirty at
	// the place where this method is called
	IX_ErrorForward(pf_fh.MarkDirty(newpnum));
	char *newkeys, *newpointers;
	newkeys = newpage + sizeof(IX_InternalHdr);
	newpointers = newkeys + fHdr.attrLength * fHdr.internal_capacity;

	// get orig page keys and pointers
	char *keys, *pointers;
	keys = page + sizeof(IX_InternalHdr);
	pointers = keys + fHdr.attrLength * fHdr.internal_capacity;
	int index;
	bool found = findKey(keys, pData, fHdr.internal_capacity, index);
	if (found) {
		// should not be found
		return WARN;
	}

	// move half the entries to new page
	int tokeep = fHdr.internal_capacity/2; // num keys to be kept
	int togive = fHdr.internal_capacity - tokeep;
	memcpy(newkeys, keys + fHdr.attrLength * tokeep, 
				fHdr.attrLength * togive);
	memcpy(newpointers, pointers + sizeof(PageNum) * tokeep, 
				sizeof(PageNum) * togive);
	// update the page headers
	IX_InternalHdr *newpHdr = (IX_InternalHdr*) newpage;
	IX_InternalHdr *pHdr = (IX_InternalHdr*) page;
	newpHdr->type = INTERNAL;
	pHdr->num_keys = tokeep;
	newpHdr->num_keys = togive;

	// insert the new record in the appropriate page
	if (index <= tokeep) {
		// insert the new record in the current page
		arrayInsert(keys, (void*) pData, fHdr.attrLength, index, tokeep);
		arrayInsert(pointers, (void*) &newpagenum, sizeof(PageNum), index, tokeep);
		pHdr->num_keys++;
	} else {
		// insert in the new page
		index -= tokeep;
		arrayInsert(newkeys, (void*) pData, fHdr.attrLength, index, togive);
		arrayInsert(newpointers, (void*) &newpagenum, sizeof(PageNum), index, togive);
		newpHdr->num_keys++;
	}

	// set pData to the first key of the new page and left_page of new page
	memcpy(pData, newkeys, fHdr.attrLength);
	memcpy(&newpHdr->left_pnum, newpointers, sizeof(PageNum));
	// delete the first entries from the new page
	shiftBytes(newkeys + fHdr.attrLength, fHdr.attrLength, togive, -1);
	shiftBytes(newpointers + sizeof(PageNum), sizeof(PageNum), togive, -1);
	newpHdr->num_keys--;
	// set the new page
	newpagenum = newpnum;
	IX_ErrorForward(pf_fh.UnpinPage(newpnum));
	return OK_RC;
}

/*
Creates an overflow page if duplicate entries were present. To be called for
filled pages to squeeze out some space if possible.
*/
RC IX_IndexHandle::squeezeLeaf(char* page, int& opnum) {
	RC WARN = IX_SQUEEZE_WARN, ERR = IX_SQUEEZE_ERR;
	// IX_LeafHdr* pHdr = (IX_LeafHdr*) page;
	char *keys = page + sizeof(IX_LeafHdr);
	// char *pointers = keys + fHdr.attrLength * fHdr.leaf_capacity;
	// if the page has duplicates, flush them to a new overflow page
	int dup_index;
	int num_dups = checkDuplicates(keys, fHdr.leaf_capacity, dup_index);
	if (num_dups < 2) {
		opnum = -1;
		return OK_RC;
	}
	void* dup_key = (void*) (page + dup_index * fHdr.attrLength);
	// create an overflow page
	char* op_data;
	IX_ErrorForward(createOverflow(page, opnum, op_data, dup_key)); 
	// mark dirty and unpin the page
	IX_ErrorForward(pf_fh.MarkDirty(opnum));
	IX_ErrorForward(pf_fh.UnpinPage(opnum));
	return OK_RC;
}

/*
Creates an overflow page by putting in all the contents of a given key
in the overflow page. Page to be unpinned in the method calling it
*/
RC IX_IndexHandle::createOverflow(char* page, int& opnum, char* &op_data, void* key) {
	RC WARN = IX_OVERFLOW_WARN, ERR = IX_OVERFLOW_ERR;
	IX_LeafHdr* pHdr = (IX_LeafHdr*) page;
	char *keys = page + sizeof(IX_LeafHdr);
	char *pointers = keys + fHdr.attrLength * fHdr.leaf_capacity;

	// Allocate a new overflow page and put all the matching entries in it
	PF_PageHandle overflow_handle;
	pf_fh.AllocatePage(overflow_handle);
	char *op_rids;
	IX_ErrorForward(overflow_handle.GetPageNum(opnum));
	IX_ErrorForward(overflow_handle.GetData(op_data));
	IX_OverflowHdr *oHdr = (IX_OverflowHdr*) op_data;

	// iterate through all the keys and copy the rids
	op_rids = op_data + sizeof(IX_OverflowHdr);
	int first_seen = -1, num_copied = 0;
	for (int i = 0; i < fHdr.leaf_capacity; i++) {
		if (eq_op(key, (void*) (keys + i * fHdr.attrLength))) {
			memcpy(op_rids, pointers + i * sizeof(RID), sizeof(RID));
			op_rids += sizeof(RID);
			num_copied++;
			if (first_seen < 0) first_seen = i;
		}
	}

	// remove duplicate keys and pointers
	if (num_copied > 1 && first_seen + num_copied < fHdr.leaf_capacity) {
		int to_shift = fHdr.leaf_capacity - first_seen - num_copied;
		shiftBytes(keys + (first_seen + num_copied) * fHdr.attrLength, 
			fHdr.attrLength, to_shift, -(num_copied - 1));
		shiftBytes(pointers + (first_seen + num_copied) * sizeof(RID), 
			sizeof(RID), to_shift, -(num_copied - 1));
	}
	
	// update the page headers
	pHdr->num_keys -= (num_copied - 1);
	oHdr->num_rids = num_copied;
	oHdr->next_page = IX_SENTINEL;
	// update the rid in leaf page to indicate overflow page
	RID *temp_rid = (RID*) (pointers + first_seen * sizeof(RID));
	*temp_rid = RID(opnum, -1);
	return OK_RC;
}

/*
Sets newpage to the page number of new leaf page allocated. pData is also
set to the smallest key in the new page
*/
RC IX_IndexHandle::leafInsert(PF_PageHandle &ph, void *&pData, 
			const RID &rid,	int& newpage) {
	RC WARN = IX_LEAF_INSERT_WARN, ERR = IX_LEAF_INSERT_ERR;
	newpage = -1;
	// fetch the page
	char* page;
	int pnum;
	IX_ErrorForward(ph.GetData(page));
	IX_ErrorForward(ph.GetPageNum(pnum));
	
	IX_LeafHdr *pHdr = (IX_LeafHdr*) page;
	int num_keys = pHdr->num_keys;
	char *keys = page + sizeof(IX_LeafHdr);
	char *pointers = keys + fHdr.attrLength * fHdr.leaf_capacity;
	int index;
	bool found = findKey(keys, pData, num_keys, index);
	// if the page has space
	if (num_keys < fHdr.leaf_capacity) {
		// if the key exists in page
		if (found) {
			// get the rid
			RID *temp_rid = (RID*) (pointers + index * sizeof(RID));
			PageNum opagenum;
			SlotNum slotnum;
			IX_ErrorForward(temp_rid->GetPageNum(opagenum));
			IX_ErrorForward(temp_rid->GetSlotNum(slotnum));
			// if it points to an overflow page
			if (slotnum < 0) {
				// get the overflow page and insert into that
				PF_PageHandle oph;
				pf_fh.GetThisPage(opagenum, oph);
				IX_ErrorForward(overflowInsert(oph, rid));
				IX_ErrorForward(pf_fh.UnpinPage(opagenum));
				return OK_RC;				
			}
			// if there is no overflow page, simply put the key
			else {
				// check for duplicates
				RID *temp_rid;
				for (int i = index; i < pHdr->num_keys; i++) {
					temp_rid = (RID*) (pointers + i * sizeof(RID));
					if (*temp_rid == rid) return IX_DUPLICATE_INSERT;
				}
				// Mark as dirty 
				int temp_pnum;
				IX_ErrorForward(ph.GetPageNum(temp_pnum));
				IX_ErrorForward(pf_fh.MarkDirty(temp_pnum));
				// put the data
				arrayInsert(keys, pData, fHdr.attrLength, index, pHdr->num_keys);
				arrayInsert(pointers, (void*) (&rid), sizeof(RID), index, pHdr->num_keys);
				pHdr->num_keys++;
				return OK_RC;
			}
		}
		// if the key doesn't exist in page, simply put it
		else {
			// Mark the page as dirty
			int temp_pnum;
			IX_ErrorForward(ph.GetPageNum(temp_pnum));
			IX_ErrorForward(pf_fh.MarkDirty(temp_pnum));
			arrayInsert(keys, pData, fHdr.attrLength, index, pHdr->num_keys);
			arrayInsert(pointers, (void*) (&rid), sizeof(RID), index, pHdr->num_keys);
			pHdr->num_keys++;
			return OK_RC;
		}
	}
	// if the page has no space
	else {
		// squeeze the page to get space
		int opnum; // -1 if squeeze not possible
		IX_ErrorForward(squeezeLeaf(page, opnum));
		// if page squeezed, mark dirty and call the function again
		if (opnum >= 0) {
			int temp_pnum;
			IX_ErrorForward(ph.GetPageNum(temp_pnum));
			IX_ErrorForward(pf_fh.MarkDirty(temp_pnum));
			IX_ErrorForward(leafInsert(ph, pData, rid, newpage));
			return OK_RC;
		}
		// page not squeezed
		else {
			// if the key exists, create an overflow page
			if (found) {
				// mark dirty 
				int temp_pnum;
				IX_ErrorForward(ph.GetPageNum(temp_pnum));
				IX_ErrorForward(pf_fh.MarkDirty(temp_pnum));

				// check for duplicates
				RID *temp_rid;
				for (int i = 0; i < pHdr->num_keys; i++) {
					temp_rid = (RID*) (pointers + i * sizeof(RID));
					if (*temp_rid == rid) return IX_DUPLICATE_INSERT;
				}
				int opnum;
				char* op_data;
				IX_ErrorForward(createOverflow(page, opnum, op_data, pData));
				// insert the new entry into the overflow page and unpin it
				IX_OverflowHdr *oHdr = (IX_OverflowHdr*) op_data;
				RID* new_rid = (RID*) (op_data + sizeof(IX_OverflowHdr) 
						+ oHdr->num_rids * sizeof(RID));
				memcpy(new_rid, &rid, sizeof(RID));
				oHdr->num_rids++;
				IX_ErrorForward(pf_fh.UnpinPage(opnum));
				return OK_RC;
			}
			// key doesn't exist in page, needs to be split
			else {
				IX_ErrorForward(splitLeaf(page, pnum, pData, rid, newpage));
				// pData and newpage changed inside splitLeaf
				return OK_RC;
			}
		}
	}
	// should not reach here
	return WARN;
}


RC IX_IndexHandle::overflowInsert(PF_PageHandle &ph, const RID &rid) {
	RC WARN = IX_OVER_INSERT_WARN, ERR = IX_OVER_INSERT_ERR;
	// fetch the page
	char* page;
	int pnum;
	IX_ErrorForward(ph.GetData(page));
	IX_ErrorForward(ph.GetPageNum(pnum));
	
	IX_OverflowHdr *pHdr = (IX_OverflowHdr*) page;
	int num_rids = pHdr->num_rids;
	char *rids = page + sizeof(IX_OverflowHdr);
	// if the page has space, put it there
	if (num_rids < fHdr.overflow_capacity) {
		// check for duplicates
		RID *temp_rid;
		for (int i = 0; i < pHdr->num_rids; i++) {
			temp_rid = (RID*) (rids + i * sizeof(RID));
			if (*temp_rid == rid) return IX_DUPLICATE_INSERT;
		}
		// mark dirty
		IX_ErrorForward(pf_fh.MarkDirty(pnum));
		RID* new_rid = (RID*) (rids	+ num_rids * sizeof(RID));
		*new_rid = rid;
		pHdr->num_rids++;
		return OK_RC;
	}
	// if the page has no space
	else {
		PF_PageHandle overflow_handle;
		// if there is no next page, allocate a new page and update the header
		if (pHdr->next_page == IX_SENTINEL) {
			IX_ErrorForward(pf_fh.AllocatePage(overflow_handle));
			char* op_data;
			IX_ErrorForward(overflow_handle.GetData(op_data));
			// update the header
			IX_OverflowHdr *oHdr = (IX_OverflowHdr*) op_data;
			oHdr->next_page = IX_SENTINEL;
			oHdr->num_rids = 0;
			// mark the current page as dirty, overflow page will be marked
			// dirty in its call
			IX_ErrorForward(pf_fh.MarkDirty(pnum));
		}
		else {
			IX_ErrorForward(pf_fh.GetThisPage(pHdr->next_page, overflow_handle));
		}
		
		// call the function on overflow_handle, it will be marked dirty 
		// during its call
		IX_ErrorForward(overflowInsert(overflow_handle, rid));
		int opnum;
		IX_ErrorForward(overflow_handle.GetPageNum(opnum));
		IX_ErrorForward(pf_fh.UnpinPage(opnum));
		return OK_RC;		
	}
	// should not reach here
	return WARN;
}


RC IX_IndexHandle::treeInsert(PF_PageHandle &ph, void *&pData, 
								const RID &rid, int& newpage) {
	RC WARN = IX_TREE_INSERT_WARN, ERR = IX_TREE_INSERT_ERR;
	// fetch the page
	char* page;
	int pnum;
	IX_ErrorForward(ph.GetPageNum(pnum));
	IX_ErrorForward(ph.GetData(page));
	IPageType* ptype = (IPageType*) page;

	// leaf page insert, base case
	if (*ptype == LEAF) {
		IX_ErrorForward(leafInsert(ph, pData, rid, newpage));
		return OK_RC;
	}

	// internal page insert
	else if (*ptype == INTERNAL) {
		// header is located in the beginning of the page
		IX_InternalHdr *pHdr = (IX_InternalHdr*) page;
		int index, num_keys = pHdr->num_keys;
		char *keys = page + sizeof(IX_InternalHdr);
		char *pointers = keys + fHdr.attrLength * fHdr.internal_capacity;
		// find the appropriate key in the page
		bool found = findKey(keys, pData, num_keys, index);
		// call the function on the appropriate page
		PageNum child_pnum;
		if (found) {
			// the pointer index is correct
			memcpy(&child_pnum, pointers + index * sizeof(PageNum), sizeof(PageNum));
		} else if (index == 0) {
			child_pnum = pHdr->left_pnum;
		} else {
			index--;
			memcpy(&child_pnum, pointers + index * sizeof(PageNum), sizeof(PageNum));
		}

		// get the appropriate child
		PF_PageHandle cph;
		IX_ErrorForward(pf_fh.GetThisPage(child_pnum, cph));
		IX_ErrorForward(treeInsert(cph, pData, rid, newpage));
		IX_ErrorForward(pf_fh.UnpinPage(child_pnum));
		// no new page allocated on the lower level
		if (newpage < 0) {
			return OK_RC;
		}
		else if (num_keys < fHdr.internal_capacity) {
			// there is place in the current page. insert the key contained
			// in pData and the newpage as the pointer
			int nk_index;
			bool found = findKey(keys, pData, num_keys, nk_index);
			if (found) {
				return WARN; // new key should not be found
			}
			IX_ErrorForward(pf_fh.MarkDirty(pnum));
			arrayInsert(keys, pData, fHdr.attrLength, nk_index, num_keys);
			arrayInsert(pointers, (void*) (&newpage), sizeof(PageNum), nk_index, num_keys);
			pHdr->num_keys++;
			newpage = -1;
			return OK_RC;	
		}
		// if the internal page is full, it must be split
		else {
			IX_ErrorForward(splitInternal(page, pData, newpage));
			IX_ErrorForward(pf_fh.MarkDirty(pnum));
			return OK_RC;
		}
	} 
	// invalid page type seen
	else {
		return IX_PAGE_TYPE_ERROR;
	}
}
