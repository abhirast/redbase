//
// rm.h
//
//   Record Manager component interface
//
// This file does not include the interface for the RID class.  This is
// found in rm_rid.h
//

#ifndef RM_H
#define RM_H

// Please DO NOT include any files other than redbase.h and pf.h in this
// file.  When you submit your code, the test program will be compiled
// with your rm.h and your redbase.h, along with the standard pf.h that
// was given to you.  Your rm.h, your redbase.h, and the standard pf.h
// should therefore be self-contained (i.e., should not depend upon
// declarations in any other file).

// Do not change the following includes
#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"

//
// Structures for file and page headers 
//
struct RM_FileHdr {
    unsigned short int record_length; // length of each record
    unsigned short int capacity;      // record capacity of a data page
    PageNum first_data_page;          // page number of first data page
};
struct RM_PageHdr {
    PageNum next;           // page number of the next page
};



//
// RM_Record: RM Record interface
//
class RM_Record {
public:
    RM_Record ();
    ~RM_Record();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;
};

//
// RM_FileHandle: RM File interface
//
class RM_FileHandle {
public:
    RM_FileHandle ();
    ~RM_FileHandle();

    // Given a RID, return the record
    RC GetRec     (const RID &rid, RM_Record &rec) const;

    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record

    RC DeleteRec  (const RID &rid);                    // Delete a record
    RC UpdateRec  (const RM_Record &rec);              // Update a record

    // Forces a page (along with any contents stored in this class)
    // from the buffer pool to disk.  Default value forces all pages.
    RC ForcePages (PageNum pageNum = ALL_PAGES);
private:
    RM_FileHdr fHdr;
    PF_Manager *pf_manager;
    int bIsOpen;
    int bHeaderChanged;
};

//
// RM_FileScan: condition-based scan of records in the file
//
class RM_FileScan {
public:
    RM_FileScan  ();
    ~RM_FileScan ();

    RC OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT); // Initialize a file scan
    RC GetNextRec(RM_Record &rec);               // Get next matching record
    RC CloseScan ();                             // Close the scan
};



//
// RM_Manager: provides RM file management
//
class RM_Manager {
public:
    RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

    RC CloseFile  (RM_FileHandle &fileHandle);
    
private:
    PF_Manager *pf_manager;
};

// Function to calculate number of records per page
int numRecordsPerPage(int recordSize);



//
// Print-error function
//
void RM_PrintError(RC rc);




// Macro for error forwarding
#define RM_ErrorForward(expr) if ((RC rc = expr) != OK_RC) return rc

// Sentinel Page Number indicating end of linked list of pages
#define RM_SENTINEL_PAGE -1

// Define the error codes
#define RM_BAD_REC_SIZE (START_RM_ERR - 0) // record size larger than page
#define RM_PAGE_UNINIT (START_RM_ERR - 1) // RID page uninitialized
#define RM_SLOT_UNINIT (START_RM_ERR - 2) // RID slot uninitialized


#endif