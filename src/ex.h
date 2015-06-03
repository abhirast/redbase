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


struct EX_PageHdr {
	int numrecs;
};

// loads into a file maintaining a fill factor
class EX_Loader {
public:
	EX_Loader(PF_Manager &pfm);
	~EX_Loader();
	RC Create(char *fileName, float ff, int recsize);
	RC PutRec(char* data);
	RC Close();
private:
	PF_Manager* pfm;
	float ff;
	PF_FileHandle fh;
	bool isOpen;
	int recsize;
	int currPage;
	int recsInCurrPage;
	int capacity;
};

// reads from a file created by loader
class EX_Scanner {
public:
	EX_Scanner(PF_Manager &pfm);
	~EX_Scanner();
	RC Open(char* fileName, int startPage, bool goRight, int recsize);
	RC Next(std::vector<char> &rec);
	RC Reset();
	RC Close();
private:
	PF_Manager* pfm;
	PF_FileHandle fh;
	bool isOpen;
	int recsize;
	int currPage;
	int currSlot;
	int increment;
};


// Macro for error forwarding
// WARN and ERR to be defined in the context where macro is used
#define EX_ErrorForward(expr) do { \
RC tmp_rc = (expr);\
if (tmp_rc != OK_RC) \
    return ((tmp_rc > 0) ? WARN : ERR); \
} while (0)

//
// Print-error function
//
void EX_PrintError(RC rc);





#endif