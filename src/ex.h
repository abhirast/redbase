//
// ex.h
//
//   Extenstion


#ifndef EX_H
#define EX_H

const float sortFF = 0.9;

/////////////////////////////////////////////////////
// Sorting operator
/////////////////////////////////////////////////////

class EX_Sort: public QL_UnaryOp {
public:
	EX_Sort(QL_Op &child);
	~EX_Sort();
	RC Open();
	RC Next(std::vector<char> &rec);
	RC Reset();
	RC Close();
private:
	bool isOpen;
};


class EX_Sorter {
public:
	EX_Sorter(RM_Manager &rmm, char* fileName, QL_Op &scan, int attrIndex);
	~EX_Sorter();
	RC sort();
private:
	PF_Manager *pfm;
	RM_Manager *rmm;
	QL_Op *scan;
	int attrIndex;
	AttrType attrType;
	int offset;
	int attrLength;
	int recsize;
	int recsPerPage;
	int bufferSize;
	char *buffer;
	void pageSort(char* page, int lo, int hi);
	int partition(char* page, int lo, int hi);
	bool less(char *v, char *w);
	void exch(char *v, char *w);
	RC fillBuffer(std::vector<char*> &pages, std::vector<int> &numrecs);
	RC createSortedChunk(char* fileName, float ff, std::vector<char*> &pages, 
												std::vector<int> &numrecs);
	void mergeSortedChunks();
	int pagesToReserve(QL_Op* node);
	void cleanUp(std::vector<char*> &pages, std::vector<int> &numrecs);
};


////////////////////////////////////////////////////////////////////
// RM style classes for managing sorted files
////////////////////////////////////////////////////////////////////

class EX_SortHandle {
    friend class EX_SortManager;
    friend class EX_SortScan;
public:
    EX_SortHandle ();
    ~EX_SortHandle();
    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record
    RC DeleteRec  (const RID &rid);                    // Delete a record
private:
    RM_FileHdr fHdr;
    PF_FileHandle pf_fh;
    int bIsOpen;
    int bHeaderChanged;
};

class EX_SortManager {
    friend class EX_Sorter;
public:
    EX_SortManager(PF_Manager &pfm);
    ~EX_SortManager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, EX_SortHandle &sh);

    RC CloseFile  (EX_SortHandle &sh);
private:
    PF_Manager *pf_manager;
};





class EX_SortScanOp {
public:
    EX_SortScanOp  (const EX_SortHandle &sh, CompOp compOp, void *value);
    ~EX_SortScanOp ();

    RC Open(); 
    RC Next(RM_Record &rec);
    
    RC Close();
private:

};




















// Macro for error forwarding
// WARN and ERR to be defined in the context where macro is used
#define EX_ErrorForward(expr) do { \
RC tmp_rc = (expr);\
if (tmp_rc != OK_RC) \
    return ((tmp_rc > 0) ? WARN : ERR); \
} while (0)

#endif