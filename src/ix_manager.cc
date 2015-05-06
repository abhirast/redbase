#include <cstdio>
#include <iostream>
#include <cstring>
// #include <cmath>
#include "ix.h"
#include "ix_internal.h"

using namespace std;

// Store the reference to the passed PF_Manager
IX_Manager::IX_Manager(PF_Manager &pfm) {
	pf_manager = &pfm;
}

IX_Manager::~IX_Manager() {
	// no dynamic memory allocated, so nothing to be done
}

/* Create a new Index
	Steps- 
	1. check if all the attributes are valid
	2. Call pfmanager to create a file named filename.indexNo
    3. Open the created file
    4. Allocate a new file header page
    5. Mark the file header page dirty
    6. Fetch the contents of the page and update the header
    7. Unpin the page
    8. Close the file
*/
RC IX_Manager::CreateIndex(const char *fileName, int indexNo,
    AttrType attrType, int attrLength) {
	// define default errors to be forwarded
	RC WARN = IX_MANAGER_CREATE_WARN, ERR = IX_MANAGER_CREATE_ERR;
    // check validity of inputs
    if (!fileName) return IX_INVALID_CREATE_PARAM;
    if (sizeof(fileName) > MAXNAME) return IX_INVALID_CREATE_PARAM;
    if (indexNo < 0) return IX_INVALID_CREATE_PARAM;
    if (attrType < INT || attrType > STRING) return IX_INVALID_CREATE_PARAM;
    if (attrType != STRING && attrLength != 4) return IX_INVALID_CREATE_PARAM;
    if (attrType == STRING && (attrLength < 1 || attrLength > MAXSTRINGLEN)) {
    	return IX_INVALID_CREATE_PARAM;
    }
    // create the file
    char fname[MAXNAME + 10];
    sprintf(fname, "%s.%d", fileName, indexNo);
    IX_ErrorForward(pf_manager->CreateFile(fname));
    // define a file handle and page handles to open the file
    PF_FileHandle fh;
    PF_PageHandle header;
    IX_ErrorForward(pf_manager->OpenFile(fname, fh));
    IX_ErrorForward(fh.AllocatePage(header));
    // get the assigned page number
    PageNum header_pnum;
    IX_ErrorForward(header.GetPageNum(header_pnum));
    // update the header
    IX_ErrorForward(fh.MarkDirty(header_pnum));
    char *contents;
    IX_ErrorForward(header.GetData(contents));
    IX_FileHdr fHdr;
    fHdr.attrLength = attrLength;
    fHdr.root_pnum = -1;
    fHdr.leaf_capacity =  numKeysPerPage(attrLength, sizeof(RID), 
    						sizeof(IX_LeafHdr));
    fHdr.internal_capacity = numKeysPerPage(attrLength, sizeof(PageNum), 
    						sizeof(IX_InternalHdr));
    // overflow page has only RIDs
    fHdr.overflow_capacity = numKeysPerPage(0, sizeof(RID), sizeof(PageNum));
    fHdr.header_pnum = header_pnum;
    fHdr.attrType = attrType;
    memcpy(contents, &fHdr, sizeof(IX_FileHdr));
    // unpin the header
    IX_ErrorForward(fh.UnpinPage(header_pnum));
    // RM_ErrorForward(fh.ForcePages(header_pnum));
    IX_ErrorForward(pf_manager->CloseFile(fh));
    return OK_RC;
}

/*  Destroys the specified index. May result in an error if this method
    is called on an open index.
    Steps - 
    1. Destroy the index using PF_Manager object
    2. Return the appropriate error code based on the result
*/
RC IX_Manager::DestroyIndex(const char *fileName, int indexNo) {
	// define default errors to be forwarded
	RC WARN = IX_MANAGER_DESTROY_WARN, ERR = IX_MANAGER_DESTROY_ERR;
    if (!fileName) return IX_NULL_FILENAME;
    char fname[MAXNAME + 10];
    sprintf(fname, "%s.%d", fileName, indexNo);
    IX_ErrorForward(pf_manager->DestroyFile(fname));
    return OK_RC;
}

/* Steps - 
    1. Check if the supplied index handle is already open
    2. Check if the filename is valid
    3. Make the private PF_FileHandle object of passed RM_FileHandle
       object a handle to the file 
    4. Declare a PF_PageHandle object to read header
    5. Copy the header to RM_FileHandle object
    6. Unpin the PF page handle  
*/
RC IX_Manager::OpenIndex(const char *fileName, int indexNo,
    IX_IndexHandle &indexHandle) {
	RC WARN = IX_MANAGER_OPEN_WARN, ERR = IX_MANAGER_OPEN_ERR; // used by macro
    if (indexHandle.bIsOpen) {
        IX_ErrorForward(1); // Positive number for warning
    }
    if (!fileName) return IX_NULL_FILENAME;
    char fname[MAXNAME + 10];
    sprintf(fname, "%s.%d", fileName, indexNo);
    IX_ErrorForward(pf_manager->OpenFile(fname, indexHandle.pf_fh));
    PF_PageHandle header;
    // First page is the header page 
    IX_ErrorForward(indexHandle.pf_fh.GetFirstPage(header));
    char *data;
    IX_ErrorForward(header.GetData(data));
    memcpy(&indexHandle.fHdr, data, sizeof(IX_FileHdr));
    // get the assigned page number
    PageNum header_pnum;
    IX_ErrorForward(header.GetPageNum(header_pnum));
    IX_ErrorForward(indexHandle.pf_fh.UnpinPage(header_pnum));
    indexHandle.bIsOpen = 1;
    indexHandle.bHeaderChanged = 0;
    return OK_RC;
}

/*  Close the index instance using PF_Manager object and clean the indexHandle
    Steps-
    1. Check if the index is already closed.
    2. If header has changed, fetch the header, update it
    3. Close the file using PF_Manager object
    4. Set the open flag to 0
    */
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle) {
	RC WARN = IX_MANAGER_CLOSE_WARN, ERR = IX_MANAGER_CLOSE_ERR; // used by macro
    if (indexHandle.bIsOpen == 0) {
        IX_ErrorForward(1); //Positive number for warning
    }
    if (indexHandle.bHeaderChanged) {
        PF_PageHandle header;
        int header_pnum = indexHandle.fHdr.header_pnum;
        IX_ErrorForward(indexHandle.pf_fh.GetThisPage(header_pnum, header));
        // update the header
        IX_ErrorForward(indexHandle.pf_fh.MarkDirty(header_pnum));
        char *contents;
        IX_ErrorForward(header.GetData(contents));
        memcpy(contents, &indexHandle.fHdr, sizeof(IX_FileHdr));
        // unpin the header
        IX_ErrorForward(indexHandle.pf_fh.UnpinPage(header_pnum));
    }
    IX_ErrorForward(indexHandle.ForcePages());
    IX_ErrorForward(pf_manager->CloseFile(indexHandle.pf_fh));
    indexHandle.bIsOpen = 0;
    indexHandle.bHeaderChanged = 0;
    return OK_RC;
}

/*  Function to figure out how many keys can reside in a page. Each
	page contains a page header, x keys and (x+1) pointers. This 
	function calculates the maximum possible value of x.
*/
int IX_Manager::numKeysPerPage(int key_size, int pointer_size, int header) {
    int num = 0;
    int effective_psize = PF_PAGE_SIZE - header;
    while (num * (key_size + pointer_size) <= effective_psize) num++;
    return num - 1;
}