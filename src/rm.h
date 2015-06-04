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
// Structure for file header 
//
struct RM_FileHdr {
    int record_length;          // length of each record
    int capacity;               // record capacity of a data page
    int bitmap_size;            // size of botmap in bytes
    int bitmap_offset;          // byte offset of bitmap
    int first_record_offset;    // byte offset of first record
    int empty_page_count;       // number of empty pages
    int header_pnum;            // page number of file header page
    int first_free;             // page number of first free page
};

//
// RM_Record: RM Record interface
//
class RM_Record {
    friend class RM_FileHandle;
    friend class RM_FileScan;
    friend class QL_Manager;
public:
    RM_Record ();
    ~RM_Record();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;
private:
    RID rid;
    char *record;
    int bIsAllocated;
};

//
// RM_FileHandle: RM File interface
//
class RM_FileHandle {
    friend class RM_Manager;
    friend class RM_FileScan;
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
    RC ForcePages (int pageNum) const;
private:
    RM_FileHdr fHdr;
    PF_FileHandle pf_fh;
    int bIsOpen;
    int bHeaderChanged;
    // Functions for handling bit array
    RC SetBit(char *bitarray, int position) const;              // sets the bit 1
    RC UnsetBit(char *bitarray, int position) const;            // sets the bit 0
    RC GetBit(char *bitarray, int position, int &status) const; // gets the bit state
    int FindSlot(char *bitmap) const;
    // Functions for updating records
    RC FetchRecord(char *page, char *buffer, int slot) const;
    RC DumpRecord(char *page, const char *buffer, int slot);
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
private:
    const RM_FileHandle *rm_fh;
    int bIsOpen;
    int attr_offset;
    int attr_length;
    AttrType attr_type;
    char *query_value;
    ClientHint pin_hint;
    CompOp comp_op;
    // variables for file scan
    int recs_seen;
    int num_recs;
    int current;
    char *bitmap_copy;
    PF_PageHandle pf_ph;
    // pointer to a member function
    bool (RM_FileScan::*comp)(void* attr);
    
    // Functions for comparison
    void buffer(void *ptr, char* buff);
    bool no_op(void* attr);
    bool eq_op(void* attr);
    bool ne_op(void* attr);
    bool lt_op(void* attr);
    bool gt_op(void* attr);
    bool le_op(void* attr);
    bool ge_op(void* attr);
    RC GiveNewPage(char *&data);
};

//
// RM_Manager: provides RM file management
//
class RM_Manager {
    friend class QL_Manager;        // for accessing pf manager
public:
    RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

    RC CloseFile  (RM_FileHandle &fileHandle);
    
private:
    PF_Manager *pf_manager;
    // Function to calculate number of records per page
    int numRecordsPerPage(int recordSize);
};





//
// Print-error function
//
void RM_PrintError(RC rc);


// Define the error codes

#define RM_BAD_REC_SIZE             (START_RM_WARN + 0) // record size larger than page
#define RM_MANAGER_CREATE_WARN      (START_RM_WARN + 1) // Unable to create file
#define RM_MANAGER_DESTROY_WARN     (START_RM_WARN + 2) // Unable to destroy file
#define RM_MANAGER_OPEN_WARN        (START_RM_WARN + 3) // 
#define RM_MANAGER_CLOSE_WARN       (START_RM_WARN + 4)
#define RM_FILE_NOT_OPEN            (START_RM_WARN + 5)
#define RM_INVALID_RID              (START_RM_WARN + 6)
#define RM_INVALID_RECORD           (START_RM_WARN + 7)        
#define RM_INVALID_PAGE             (START_RM_WARN + 8)        
#define RM_INSERT_FAIL              (START_RM_WARN + 9)
#define RM_FORCEPAGE_FAIL           (START_RM_WARN + 10)    
#define RM_SCAN_NOT_OPEN            (START_RM_WARN + 11)    
#define RM_PAGE_OVERFLOW            (START_RM_WARN + 12)
#define RM_NULL_INSERT              (START_RM_WARN + 13)
#define RM_SCAN_OPEN_FAIL           (START_RM_WARN + 14)
#define RM_NULL_FILENAME            (START_RM_WARN + 15)
#define RM_EOF                      (START_RM_WARN + 16)        
#define RM_LASTWARN                 RM_EOF

#define RM_MANAGER_CREATE_ERR       (START_RM_ERR - 0)
#define RM_MANAGER_DESTROY_ERR      (START_RM_ERR - 1)
#define RM_MANAGER_OPEN_ERR         (START_RM_ERR - 2)
#define RM_MANAGER_CLOSE_ERR        (START_RM_ERR - 3)
#define RM_FILEHANDLE_FATAL         (START_RM_ERR - 4)      
#define RM_FILESCAN_FATAL           (START_RM_ERR - 5)      
#define RM_LASTERROR                RM_FILESCAN_FATAL
#endif