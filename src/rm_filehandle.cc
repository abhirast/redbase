#include <cstdio>
#include <iostream>
#include <cstring>
#include <cmath>
#include "rm.h"
#include "rm_internal.h"

using namespace std;


RM_FileHandle::RM_FileHandle() {
	bIsOpen = 0;
	bHeaderChanged = 0;
}

RM_FileHandle::~RM_FileHandle() {
	// No dynamic memory allocated. Nothing to do.
}

/*  Fetches the corresponding record for a given record id. The
	record is a deep copy of the actual record. The page is arraged
	as follows - Page Header, bitmap of size capacity bytes followed
	starting from bitmap_offset, records each of size record_length
	located contiguosly starting from first_record_offset
	Steps - 
	1. Check if the fileHandle refers to an open file.
	2. Fetch the page containing the record
	3. Check if the page is valid (TODO)
	4. Check if the record exists
	5. Update rec accordingly
	6. Unpin the page
*/
RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const {
	RC WARN = RM_INVALID_RID, ERR = RM_FILEHANDLE_FATAL; // used by macro
	if (bIsOpen == 0) return RM_FILE_NOT_OPEN;
	int pnum;
	int snum;
	RM_ErrorForward(rid.GetPageNum(pnum));
	RM_ErrorForward(rid.GetSlotNum(snum));
	PF_PageHandle page;
    RM_ErrorForward(pf_fh.GetThisPage(pnum, page));
    char *data;
    RM_ErrorForward(page.GetData(data));
    int rec_exists;
    RM_ErrorForward(GetBit(data+fHdr.bitmap_offset, snum, rec_exists));
    if (rec_exists == 0) RM_ErrorForward(1); // Record doesn't exist, warn
	// If record object belonged to another record, free the memory
	if (rec.bIsAllocated) delete[] rec.record;
	rec.record = new char[fHdr.record_length];
	// get the record start location and copy the record
	RM_ErrorForward(FetchRecord(data, rec.record, snum));
	rec.bIsAllocated = 1;
	// copy the record id
	rec.rid = rid;
	RM_ErrorForward(pf_fh.UnpinPage(pnum));
	return OK_RC;
}
/*	Steps-
	1. Check if the file is open
	2. If a free page doesn't exist, allocate a new page
	3. Else fetch the first free page
	4. Mark the page dirty
	4. Update the page header if a new page was allocated
	5. Insert the record
	6. Update the record count in the page header
	7. Update the bitmap
	8. If the page becomes full, remove if from free page list. This
		is done by updating the file header
	9. Update rid to reflect the record's location
   10. Unpin page
*/
RC RM_FileHandle::InsertRec  (const char *pData, RID &rid) {
	RC WARN = RM_INSERT_FAIL, ERR = RM_FILEHANDLE_FATAL; // used by macro
	if (bIsOpen == 0) return RM_FILE_NOT_OPEN;
	if (!pData) return RM_NULL_INSERT;
	PF_PageHandle pf_ph;
	int dest_page;
	if (fHdr.first_free == RM_SENTINEL) {
		pf_fh.AllocatePage(pf_ph);
		pf_ph.GetPageNum(dest_page);
	} else {
		dest_page = fHdr.first_free;
		pf_fh.GetThisPage(dest_page, pf_ph);
	}
	// Now pf_ph is the page handle to the page where record will 
	// be inserted and its page number is dest_page
	char* data;
	RM_ErrorForward(pf_ph.GetData(data));
	// Mark the page dirty
	RM_ErrorForward(pf_fh.MarkDirty(dest_page));
	// Update the page header and file header if a new page was
	// allocated
	if (fHdr.first_free == RM_SENTINEL) {
		fHdr.first_free = dest_page;
		((RM_PageHdr*) data)->next_free = RM_SENTINEL;
		bHeaderChanged = 1;
	}
	int dest_slot = FindSlot(data + fHdr.bitmap_offset);
	// Update the record on the file
	RM_ErrorForward(DumpRecord(data, pData, dest_slot));
	// update the record count and bitmap
	((RM_PageHdr*) data)->num_recs ++;
	RM_ErrorForward(SetBit(data + fHdr.bitmap_offset, dest_slot));
	// Remove the page from free list if the page became full
	if (((RM_PageHdr*) data)->num_recs == fHdr.capacity) {
		fHdr.first_free = ((RM_PageHdr*) data)->next_free;
		bHeaderChanged = 1;
	}
	rid = RID(dest_page, dest_slot);
	RM_ErrorForward(pf_fh.UnpinPage(dest_page));
	return OK_RC;
}

/* 	Unset the bitmap bit.
	Steps-
	1. Check if the file is open
	2. Check if the record is valid
	3. Mark the page dirty
	3. Unset the bit in bitmap
	4. If the page becomes empty, do something (TODO)
	5. If the page was initially full, append it to the head of
		free page list by changing the page and file header
	6. Update the record count of the page
	7. Unpin the page
*/
RC RM_FileHandle::DeleteRec(const RID &rid) {
	RC WARN = RM_INVALID_RID, ERR = RM_FILEHANDLE_FATAL; // used by macro
	if (bIsOpen == 0) return RM_FILE_NOT_OPEN;
	// Read in the record page
	int pnum;
	int snum;
	RM_ErrorForward(rid.GetPageNum(pnum));
	RM_ErrorForward(rid.GetSlotNum(snum));
	PF_PageHandle page;
    RM_ErrorForward(pf_fh.GetThisPage(pnum, page));
    char *data;
    RM_ErrorForward(page.GetData(data));
    int rec_exists;
    RM_ErrorForward(GetBit(data+fHdr.bitmap_offset, snum, rec_exists));
    if (rec_exists == 0) {
    	RM_ErrorForward(pf_fh.UnpinPage(pnum));
    	RM_ErrorForward(1); // Record doesn't exist, warn
	}
	// Mark the page dirty and unset the bit in bitmap
	RM_ErrorForward(pf_fh.MarkDirty(pnum));
	RM_ErrorForward(UnsetBit(data + fHdr.bitmap_offset, snum));
	if (((RM_PageHdr*) data)->num_recs == fHdr.capacity) {
		((RM_PageHdr*) data)->next_free = fHdr.first_free;
		fHdr.first_free = pnum;
		bHeaderChanged = 1;
	} 
	((RM_PageHdr*) data)->num_recs --;
	RM_ErrorForward(pf_fh.UnpinPage(pnum));
	return OK_RC;
}

/* 	Update a record if it exists.
	Steps- 
	1. Check if the file is open
	2. Check if the record is valid
	3. Mark the page dirty
	3. Update the record
	4. Unpin the page
*/
RC RM_FileHandle::UpdateRec(const RM_Record &rec) {
	RC WARN = RM_INVALID_RECORD, ERR = RM_FILEHANDLE_FATAL; // used by macro
	if (bIsOpen == 0) return RM_FILE_NOT_OPEN;
	if (rec.bIsAllocated == 0) return RM_INVALID_RECORD;
	// Read in the record page
	int pnum;
	int snum;
	RM_ErrorForward(rec.rid.GetPageNum(pnum));
	RM_ErrorForward(rec.rid.GetSlotNum(snum));
	PF_PageHandle page;
    RM_ErrorForward(pf_fh.GetThisPage(pnum, page));
    char *data;
    RM_ErrorForward(page.GetData(data));
    int rec_exists;
    RM_ErrorForward(GetBit(data+fHdr.bitmap_offset, snum, rec_exists));
    if (rec_exists == 0) RM_ErrorForward(1); // Record doesn't exist, warn
	// Mark the page dirty and update the record
	RM_ErrorForward(pf_fh.MarkDirty(pnum));
	RM_ErrorForward(DumpRecord(data, rec.record, snum));
	RM_ErrorForward(pf_fh.UnpinPage(pnum));
	return OK_RC;
}

/*  Forces a page (along with any contents stored in this class)
	from the buffer pool to disk.  Default value forces all pages.
	Steps-
	1. Check if the file is open
	2. Call the pf filehandle object to force the pages
*/
RC RM_FileHandle::ForcePages (int pageNum) const{
	RC WARN = RM_FORCEPAGE_FAIL, ERR = RM_FILEHANDLE_FATAL; // used by macro
	if (bIsOpen == 0) return RM_FILE_NOT_OPEN;
	if (bHeaderChanged) {
        PF_PageHandle header;
        int header_pnum = fHdr.header_pnum;
        RM_ErrorForward(pf_fh.GetThisPage(header_pnum, header));
        // update the header
        RM_ErrorForward(pf_fh.MarkDirty(header_pnum));
        char *contents;
        RM_ErrorForward(header.GetData(contents));
        memcpy(contents, &fHdr, sizeof(RM_FileHdr));
        // unpin the header
        RM_ErrorForward(pf_fh.UnpinPage(header_pnum));
    }
    if (pageNum == ALL_PAGES) {
    	RM_ErrorForward(pf_fh.FlushPages());
    } else {
		RM_ErrorForward(pf_fh.ForcePages(pageNum));
	}
	return OK_RC;
}

/*  Copy the contents of record residing in slot# slot of page
	beginning at memory location page into memory pointed by
	buffer
*/
RC RM_FileHandle::FetchRecord(char *page, char *buffer, int slot) const{
	if (slot >= fHdr.capacity) return RM_INVALID_RID;
	char *location = page + fHdr.first_record_offset 
						+ slot * fHdr.record_length;
	memcpy(buffer, location, fHdr.record_length);
	return OK_RC;
}

/*  Copy the contents of memory pointed by buffer to the record 
	residing in slot# slot of page beginning at memory location 
	page
*/
RC RM_FileHandle::DumpRecord(char *page, const char *buffer, int slot) {
	if (slot >= fHdr.capacity) return RM_INVALID_RID;
	char *location = page + fHdr.first_record_offset 
						+ slot * fHdr.record_length;
	memcpy(location, buffer, fHdr.record_length);
	return OK_RC;
}


// Functions for modifying bitmap

RC RM_FileHandle::SetBit(char *bitmap, int index) const {
    if (index >= fHdr.capacity) return RM_PAGE_OVERFLOW;
    int byte = index/8;
    int b_ind = index % 8;
    *(bitmap + byte) |= (1<<(7-b_ind));
    return OK_RC;
}

RC RM_FileHandle::UnsetBit(char *bitmap, int index) const {
    if (index >= fHdr.capacity) return RM_PAGE_OVERFLOW;
    int byte = index/8;
    int b_ind = index % 8;
    *(bitmap + byte) &= (~(1<<(7-b_ind)));
    return OK_RC;
}

RC RM_FileHandle::GetBit(char *bitmap, int index, int& status) const {
    if (index >= fHdr.capacity) return RM_PAGE_OVERFLOW;
    int byte = index/8;
    int b_ind = index%8;
    status = (*(bitmap + byte) & (1<<(7-b_ind))) > 0;
    return OK_RC;
}

// Function to find the first empty slot in a page
// Slots are numbered from 0, 1, ..., capacity-1
int RM_FileHandle::FindSlot(char *bitmap) const{
	int slot = -1, base = 0;
	unsigned char byte;
	for (int i = 0; i < fHdr.bitmap_size; i++) {
		byte = (unsigned char) ~*(bitmap + i);
		if (byte == 0) {
			base += 8;
			continue;
		}
		slot = base + 7 - floor(log2(byte));
		break;
	}
	if (slot >= fHdr.capacity) slot = -1;
	return slot;
}

/*  Keeping track of free pages-
 	Free pages are the pages that have space for at least one record.
 	The maximum number of free pages available is limited by a number
 	MAX_FREE_PAGE. If the number of free pages exceeds this limit,
 	some records are transferred if needed so that some pages become
 	empty/full. Then the full pages are removed from the free page
 	list and the empty pages are flushed off and removed from the 
 	free page list.   
*/

/*
Get page next to header

*/