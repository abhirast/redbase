#include <cstdio>
#include <iostream>
#include "rm.h"

using namespace std;

RM_Manager::RM_Manager(PF_Manager &pfm) {
    // Store the reference to pfm
    pf_manager = &pfm;
}


RM_Manager::~RM_Manager() {
    // do necessary cleanup
    }

/*  The macro RM_Error_Forward is defined in rm.h. If the PF module
    returns a non-zero error code, that code is returned.
    Steps - 
    1. Check if the record size is valid
    2. Call pfmanager to create a file named filename
    3. Open the created file
    4. Allocate a new header page and a new data page
    5. Mark the header page dirty
    6. Fetch the contents of the page and update the header
    7. Unpin the pages
    8. Close the file
*/
RC RM_Manager::CreateFile (const char *fileName, int recordSize) {
    
    if (recordSize > PF_PAGE_SIZE - sizeof(RM_PageHdr)) {
        return RM_BAD_REC_SIZE;
    }
    
    RM_ErrorForward(pf_manager->CreateFile(fileName));
    // define a file handle and page handles to open the file
    PF_FileHandle fh;
    PF_PageHandle header, pg1;
    RM_ErrorForward(pf_manager->OpenFile(fileName, fh));
    RM_ErrorForward(fh.AllocatePage(header));
    RM_ErrorForward(fh.AllocatePage(pg1));
    // get the assigned page numbers
    PageNum header_pnum, pg1_pnum;
    RM_ErrorForward(ph.GetPageNum(header_pnum));
    RM_ErrorForward(ph.GetPageNum(pg1_pnum));
    // update the header
    RM_ErrorForward(fh.MarkDirty(header_pnum));
    char *contents;
    RM_ErrorForward(ph.GetData(contents));
    RM_FileHdr fHdr;
    fHdr.record_length = recordSize;
    fHdr.first_data_page = pg1_pnum;
    memcpy(contents, &fHdr, sizeof(RM_FileHdr));
    // unpin the pages
    RM_ErrorForward(fh.UnpinPage(header_pnum));
    RM_ErrorForward(fh.UnpinPage(pg1_pnum));
    RM_ErrorForward(pf_manager->CloseFile(fh));
    return OK_RC;
}


RC RM_Manager::DestroyFile(const char *fileName) {
    // destroy file using pfmanager
    RM_ErrorForward(pf_manager->DestroyFile(fileName));
    // if successful, recurn the success code
    return OK_RC;
}

RC RM_Manager::OpenFile(const char *fileName, RM_FileHandle &fileHandle) {
    /* Assumptions-
        1. If file has more than one open instance, each instance may be
        read but will not be modified.
        2. DestroyFile will never be called on an open file.
    */

    // open file by using pfmanager

    //make fileHandle a handle for the open file

    // each call creates a new instance of open file
}

RC RM_Manager::CloseFile(RM_FileHandle &fileHandle) {
    // close the file instance using pfmanager
}
