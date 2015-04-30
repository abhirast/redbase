//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H

// Please do not include any other files than the ones below in this file.

#include "redbase.h"  // Please don't change these lines
#include "rm_rid.h"  // Please don't change these lines
#include "pf.h"

//
// Structure for index header
//

struct IX_FileHdr {
    int attrLength;
    int root_pnum;
    int leaf_capacity;
    int internal_capacity;
    int overflow_capacity;
    int header_pnum;
    AttrType attrType;
};

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
    friend class IX_Manager;
    friend class IX_IndexScan;
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Force index files to disk
    RC ForcePages();
private:
    int bIsOpen;
    PF_FileHandle pf_fh;
    IX_FileHdr fHdr;
    int bHeaderChanged;
    RID last_deleted;
    void buffer(void *ptr, char* buff) const;
    bool eq_op(void* attr1, void* attr2) const;
    bool ne_op(void* attr1, void* attr2) const;
    bool lt_op(void* attr1, void* attr2) const;
    bool gt_op(void* attr1, void* attr2) const;
    bool ge_op(void* attr1, void* attr2) const;
    bool le_op(void* attr1, void* attr2) const;
    bool findKey(char* keys, void* query, int cap, int& res) const;
    void shiftBytes(char* beg, int unit_size, int num_units, int shift_units);
    int checkDuplicates(char* keys, int num_keys, int &most_repeated_index);
    void arrayInsert(char* array, void* data, int size, int index, int cap);
    RC splitLeaf(char* page, int pnum, void* &pData, const RID &rid, int &newpage);
    RC splitInternal(char* page, void* &pData, int &newpage);
    RC squeezeLeaf(char* page, int& opnum);
    RC createOverflow(char* page, int& opnum, char* &op_data, void* key);
    RC leafInsert(PF_PageHandle &ph, void *&pData, const RID &rid, int& newpage);
    RC overflowInsert(PF_PageHandle &ph, const RID &rid);
    RC treeInsert(PF_PageHandle &ph, void *&pData, const RID &rid, int& newpage);    
    RC treeDelete(PF_PageHandle &ph, void *pData, const RID& rid, int& numKeys);
    RC leafDelete(PF_PageHandle &ph, void *pData, const RID& rid, int& numKeys);
    RC overflowDelete(PF_PageHandle &ph, const RID& rid, int& numKeys);
};

//
// IX_IndexScan: condition-based scan of index entries
//
class IX_IndexScan {
public:
    IX_IndexScan();
    ~IX_IndexScan();

    // Open index scan
    RC OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT);

    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID &rid);

    // Close index scan
    RC CloseScan();
private:
    const PF_FileHandle *pf_fh;
    const IX_IndexHandle *ix_ih;
    RID last_emitted;
    IX_FileHdr fHdr;
    int bIsOpen;
    char *query_value;
    ClientHint pin_hint;
    bool found;
    bool onOverflow;
    int current_leaf;
    int next_leaf;
    int current_overflow;
    int leaf_index;
    int overflow_index;
    CompOp comp_op;
    
    // pointer to a member function
    bool (IX_IndexScan::*comp)(void* attr);
    
    // Functions for comparison
    void buffer(void *ptr, char* buff);
    bool no_op(void* attr);
    bool eq_op(void* attr);
    bool lt_op(void* attr);
    bool gt_op(void* attr);
    bool le_op(void* attr);
    bool ge_op(void* attr);
};

//
// IX_Manager: provides IX index file management
//
class IX_Manager {
public:
    IX_Manager(PF_Manager &pfm);
    ~IX_Manager();

    // Create a new Index
    RC CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength);

    // Destroy and Index
    RC DestroyIndex(const char *fileName, int indexNo);

    // Open an Index
    RC OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle);

    // Close an Index
    RC CloseIndex(IX_IndexHandle &indexHandle);
private:
    PF_Manager *pf_manager;
    int numKeysPerPage(int key_size, int pointer_size, int header);
};

//
// Print-error function
//
void IX_PrintError(RC rc);

// Define the error codes
#define IX_MANAGER_CREATE_WARN          (START_IX_WARN + 0)
#define IX_INVALID_CREATE_PARAM         (START_IX_WARN + 1)
#define IX_MANAGER_DESTROY_WARN         (START_IX_WARN + 2)
#define IX_MANAGER_OPEN_WARN            (START_IX_WARN + 3)
#define IX_MANAGER_CLOSE_WARN           (START_IX_WARN + 4)
#define IX_NULL_FILENAME                (START_IX_WARN + 5)
#define IX_SPLIT_LEAF_WARN              (START_IX_WARN + 6)
#define IX_SPLIT_INT_WARN               (START_IX_WARN + 7)
#define IX_SQUEEZE_WARN                 (START_IX_WARN + 8)
#define IX_OVERFLOW_WARN                (START_IX_WARN + 9)
#define IX_LEAF_INSERT_WARN             (START_IX_WARN + 10)
#define IX_OVER_INSERT_WARN             (START_IX_WARN + 11)
#define IX_TREE_INSERT_WARN             (START_IX_WARN + 12)
#define IX_PAGE_TYPE_ERROR              (START_IX_WARN + 13)
#define IX_INSERT_WARN                  (START_IX_WARN + 14)          
#define IX_INVALID_INSERT_PARAM         (START_IX_WARN + 15)        
#define IX_INDEX_CLOSED                 (START_IX_WARN + 16)
#define IX_OPEN_SCAN_WARN               (START_IX_WARN + 17)
#define IX_SCAN_OPEN_FAIL               (START_IX_WARN + 18)    
#define IX_SCAN_INVALID_OPERATOR        (START_IX_WARN + 19)        
#define IX_SCAN_WARN                    (START_IX_WARN + 20)            
#define IX_EOF                          (START_IX_WARN + 21)                    
#define IX_SCAN_CLOSED                  (START_IX_WARN + 22)    
#define IX_FORCEPAGE_WARN               (START_IX_WARN + 23)
#define IX_DELETE_WARN                  (START_IX_WARN + 24)    
#define IX_TREE_DELETE_WARN             (START_IX_WARN + 25)    
#define IX_LEAF_DELETE_WARN             (START_IX_WARN + 26)    
#define IX_OVERFLOW_DELETE_WARN         (START_IX_WARN + 27)    
#define IX_REC_NOT_FOUND                (START_IX_WARN + 28)    
#define IX_NULL_KEY                     (START_IX_WARN + 29)
#define IX_DUPLICATE_INSERT             (START_IX_WARN + 30)
#define IX_LASTWARN                     IX_DUPLICATE_INSERT


#define IX_MANAGER_CREATE_ERR           (START_IX_ERR - 0)
#define IX_MANAGER_DESTROY_ERR          (START_IX_ERR - 1)
#define IX_MANAGER_OPEN_ERR             (START_IX_ERR - 2)
#define IX_MANAGER_CLOSE_ERR            (START_IX_ERR - 3)
#define IX_SPLIT_LEAF_ERR               (START_IX_ERR - 4)
#define IX_SPLIT_INT_ERR                (START_IX_ERR - 5)
#define IX_SQUEEZE_ERR                  (START_IX_ERR - 6)
#define IX_OVERFLOW_ERR                 (START_IX_ERR - 7)    
#define IX_LEAF_INSERT_ERR              (START_IX_ERR - 8)
#define IX_OVER_INSERT_ERR              (START_IX_ERR - 9)    
#define IX_TREE_INSERT_ERR              (START_IX_ERR - 10)
#define IX_INSERT_ERR                   (START_IX_ERR - 11)        
#define IX_OPEN_SCAN_ERR                (START_IX_ERR - 12)                
#define IX_SCAN_ERR                     (START_IX_ERR - 13)        
#define IX_FORCEPAGE_ERR                (START_IX_ERR - 14)
#define IX_DELETE_ERR                   (START_IX_ERR - 15)    
#define IX_TREE_DELETE_ERR              (START_IX_ERR - 16)        
#define IX_LEAF_DELETE_ERR              (START_IX_ERR - 17)                    
#define IX_OVERFLOW_DELETE_ERR          (START_IX_ERR - 18)        
#define IX_LASTERROR                    IX_OVERFLOW_DELETE_ERR

#endif
