#include <cstdio>
#include <iostream>
#include <cmath>
#include <cstring>
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
    4. Allocate a new file header page
    5. Mark the file header page dirty
    6. Fetch the contents of the page and update the header
    7. Unpin the page
    8. Close the file
    Question-
    1. Should the header page be forced to disk?
*/
RC RM_Manager::CreateFile (const char *fileName, int recordSize) {
    RC WARN = RM_CREATE_WARN, ERR = RM_CREATE_ERR; // used by macro
    if (recordSize > PF_PAGE_SIZE - sizeof(RM_PageHdr) - 1 || recordSize <= 0) {
        return RM_BAD_REC_SIZE;
    }
    
    RM_ErrorForward(pf_manager->CreateFile(fileName));
    // define a file handle and page handles to open the file
    PF_FileHandle fh;
    PF_PageHandle header;
    RM_ErrorForward(pf_manager->OpenFile(fileName, fh));
    RM_ErrorForward(fh.AllocatePage(header));
    // get the assigned page number
    PageNum header_pnum;
    RM_ErrorForward(ph.GetPageNum(header_pnum));
    // update the header
    RM_ErrorForward(fh.MarkDirty(header_pnum));
    char *contents;
    RM_ErrorForward(ph.GetData(contents));
    RM_FileHdr fHdr;
    fHdr.record_length = recordSize;
    fHdr.capacity = numRecordsPerPage(recordSize);
    memcpy(contents, &fHdr, sizeof(RM_FileHdr));
    // unpin the header
    RM_ErrorForward(fh.UnpinPage(header_pnum));
    RM_ErrorForward(pf_manager->CloseFile(fh));
    return OK_RC;
}

/*  Destroys the specified file. May result in an error if this method
    is called on an open file.
    Steps - 
    1. Destroy the file using PF_Manager object
    2. Return the appropriate error code based on the result
*/
RC RM_Manager::DestroyFile(const char *fileName) {
    RC WARN = RM_DESTROY_WARN, ERR = RM_DESTROY_ERR; // used by macro
    RM_ErrorForward(pf_manager->DestroyFile(fileName));
    return OK_RC;
}


/* Assumptions-
    1. If file has more than one open instance, each instance may be
        read but will not be modified.
    2. DestroyFile will never be called on an open file.
   Steps - 
    1. Check if the supplied file handle is already open
    2. Make the private PF_FileHandle object of passed RM_FileHandle
       object a handle to the file 
    3. Declare a PF_PageHandle object to read header
    4. Copy the header to RM_FileHandle object
    5. Unpin the PF page handle  
*/
RC RM_Manager::OpenFile(const char *fileName, RM_FileHandle &fileHandle) {
    RC WARN = RM_OPEN_WARN, ERR = RM_OPEN_ERR; // used by macro
    if (fileHandle.bIsOpen) {
        RM_ErrorForward(1); // Positive number for warning
    }
    RM_ErrorForward(pf_manager->OpenFile(fileName, fileHandle.pf_fh));
    PF_PageHandle header;
    // First page is the header page 
    RM_ErrorForward(fileHandle.pf_fh.GetFirstPage(header));
    char *data;
    RM_ErrorForward(header.GetData(data));
    memcpy(&fileHandle.fHdr, data, sizeof(RM_FileHdr));
    fileHandle.bIsOpen = 1;
    fileHandle.bHeaderChanged = 0;
    // get the assigned page number
    PageNum header_pnum;
    RM_ErrorForward(fileHandle.pf_fh.GetPageNum(header_pnum));
    RM_ErrorForward(fileHandle.pf_filehandle.UnpinPage(header_pnum));
}

/*  Close the file instance using PF_Manager object and clean the fileHandle
    Steps-
    1. Check if the file is already closed.
    2. If header has changed, fetch the header, update it
    3. Close the file using PF_Manager object
    4. Set the open flag to 0
    Question-
    1. Should the file handle be destroyed?
*/
RC RM_Manager::CloseFile(RM_FileHandle &fileHandle) {
    RC WARN = RM_CLOSE_WARN, ERR = RM_CLOSE_ERR; // used by macro
    if (fileHandle.bIsOpen == 0) {
        RM_ErrorForward(1); //Positive number for warning
    }
    if (fileHandle.bHeaderChanged) {
        PF_PageHandle header;
        RM_ErrorForward(fileHandle.pf_fh.AllocatePage(header));
        // get the assigned page number
        PageNum header_pnum;
        RM_ErrorForward(fileHandle.pf_fh.GetPageNum(header_pnum));
        // update the header
        RM_ErrorForward(fileHandle.pf_fh.MarkDirty(header_pnum));
        char *contents;
        RM_ErrorForward(fileHandle.pf_fh.GetData(contents));
        memcpy(contents, &fileHandle.fHdr, sizeof(RM_FileHdr));
        // unpin the header
        RM_ErrorForward(fileHandle.pf_fh.UnpinPage(header_pnum));
    }
    RM_ErrorForward(pf_manager->CloseFile(fileHandle.pf_fh));
    // Alternatvily, could call the constructor of fileHandle
    fileHandle.bIsOpen = 0;
    fileHandle.bHeaderChanged = 0;
    return OK_RC;
}

// Function to figure out max number of records that can be put in
// page. A separate function needs to be written because the page
// header has a bitmap whose size depends on the number of pages
int numRecordsPerPage(int rec_size) {
    int num = 0;
    int effective_psize = PF_PAGE_SIZE - sizeof(RM_PageHdr);
    while (num * rec_size +  ceil(num/8) < effective_psize) num++;
    return num - 1;
}