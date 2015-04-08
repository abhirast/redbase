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

RC RM_Manager::CreateFile (const char *fileName, int recordSize) {
    // check if the record length is valid
    if (recordSize > PF_PAGE_SIZE - sizeof(RM_PageHdr)) {
        return RM_BAD_REC_SIZE;
    }
    // call pfmanager to create a file named filename
    if ( RC create = pf_manager->CreateFile(fileName)) {
        return create;
    }
    // define file handle and page handle to update the
    // contents of file with header
    PF_FileHandle fh;
    PF_PageHandle ph;
    // open the file
    if (RC open = pf_manager->OpenFile(fileName, fh)) {
        return open;
    }
    // allocate a new page, the page is automatically pinned
    if (RC allocate = fh.AllocatePage(ph)) {
        return allocate;
    }
    // get the assigned page number
    PageNum pgnum;
    if (RC pnum = ph.GetPageNum(pgnum)) {
        return pnum;
    }
    // mark the page as dirty
    if (RC dirty = fh.MarkDirty(pgnum)) {
        return dirty;
    }
    // get the contents of page to modify it
    char *contents;
    if (RC fetch = ph.GetData(contents)) {
        return fetch;
    }
    // define the page header and write it to the file
    RM_PageHdr pHdr;
    pHdr.RecordLength = recordSize;
    memcpy(contents, *pHdr, sizeof(RM_PageHdr));
    // unpin the page
    if (RC unpin = fh.UnpinPage(pgnum)) {
        return unpin;
    }
    /*TODO
    1. Do the fh and ph objects need to be deleted?
    2. Is this a good programming structure?
    3. What do I do with PF errors?
    */
    return OK_RC;
}

RC RM_Manager::DestroyFile(const char *fileName) {
    // destroy file using pfmanager
    if (RC destroy = pf_manager->DestroyFile(fileName)) {
        return destroy;
    }
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
