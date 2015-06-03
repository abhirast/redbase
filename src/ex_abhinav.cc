/* Redbase Extension - Sort based optimization

1. 	Quicksort code adapted from java quicksort code on
	http://algs4.cs.princeton.edu/23quicksort/Quick.java.html

*/


#include <memory>
#include <sstream>
#include "redbase.h"
#include "ql.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "parser.h"
#include "ql_internal.h"
#include "ex.h"
#include "pf_internal.h" // for buffer size


using namespace std;


EX_Sort::EX_Sort(QL_Op &child) {

}

EX_Sort::~EX_Sort() {

}

RC EX_Sort::Open() {
	return OK_RC;
}

RC EX_Sort::Next(vector<char> &rec) {
	return OK_RC;
}

RC EX_Sort::Reset() {
	return OK_RC;
}

RC EX_Sort::Close() {
	return OK_RC;
}


EX_Sorter::EX_Sorter(RM_Manager &rmm, char *fileName, 
								QL_Op &scan, int attrIndex) {
	this->pfm = rmm.pf_manager;
	this->rmm = &rmm;
	this->scan = &scan;
	this->attrIndex = attrIndex;
	DataAttrInfo info = scan.attributes[attrIndex];
	this->attrType = info.attrType;
	this->offset = info.offset;
	this->attrLength = info.attrLength;
	this->recsize = scan.attributes.back().offset + 
							scan.attributes.back().attrLength;
	this->recsPerPage = PF_PAGE_SIZE / (this->recsize);
	this->bufferSize = PF_BUFFER_SIZE - pagesToReserve(&scan);
	this->buffer = new char[this->recsize];
}

EX_Sorter::~EX_Sorter() {
	delete[] buffer;
}

RC EX_Sorter::sort() {
	RC WARN = 501, ERR = -501;

   	// open the scan
	EX_ErrorForward(scan->Open());
   	
   	// allocate buffer pages for sorting
   	

    // load the records in the buffer
    
	// sort the records

	// write the sorted records to file
	return OK_RC;
}


/*	Sort a page using quick sort without shuffle step
	To sort a page, call pageSort(page, 0, numrecs-1)
*/
void EX_Sorter::pageSort(char* page, int lo, int hi) {
	if (hi <= lo) return;
    int j = partition(page, lo, hi);
    pageSort(page, lo, j-1);
    pageSort(page, j+1, hi);
}



/* Do quick sort partitioning. lo and hi are indices in the buffer page 
	starting at pointer page.
*/
int EX_Sorter::partition(char *page, int lo, int hi) {	
	int i = lo;
    int j = hi + 1;
    char* v = page + lo * recsize;
    while (true) { 
    	// find item on lo to swap
        while (less(page + (++i) * recsize, v))
            if (i == hi) break;
            
        // find item on hi to swap
        while (less(v, page + (--j) * recsize))
            if (j == lo) break;      // redundant since lo acts as sentinel

        // check if pointers cross
        if (i >= j) break;
		exch(page + i * recsize, page + j*recsize);
    }

    // put partitioning item v at jth slot
    exch(page + lo * recsize, page + j * recsize);

    // now, page[lo .. j-1] <= page[j] <= a[j+1 .. hi]
    return j;
}


bool EX_Sorter::less (char* v, char *w) {
	// compare v + offset and w + offset
	return QL_Manager::lt_op((void*) (v + offset), (void*) (w + offset), 
				attrLength, attrLength, attrType);
}

void EX_Sorter::exch(char* v, char* w) {
	memcpy(buffer, v, recsize);
	memcpy(v, w, recsize);
	memcpy(w, buffer, recsize);
}

/*  Find the number of pages to be reserved for relation access. 
	The remaining buffer pages can be used for sorting
*/
int EX_Sorter::pagesToReserve(QL_Op* node) {
	if (node == 0) return 0;
	int numpages = 0;
	if (node->opType >= 0) {
		auto unode = (QL_UnaryOp*) node;
		if (node->opType == RM_LEAF) {
			numpages = 1;
		} else if (node->opType == IX_LEAF) {
			numpages = 2;
		} else if (node->opType == SORTED_LEAF) {
			numpages = 1;
		}
		return numpages + pagesToReserve(unode->child);
	} else {
		auto bnode = (QL_BinaryOp*) node;
		return pagesToReserve(bnode->lchild) + pagesToReserve(bnode->rchild);
	}
	return 0;
}

/*	Fill buffer pages with data read from the operator. scan must be
	opened before calling this method
*/
RC EX_Sorter::fillBuffer(vector<char*> &pages, vector<int> &numrecs) {
	pages.clear();
	numrecs.clear();
	RC rc = OK_RC;
	for (int i = 0; i < bufferSize; i++) {
		// allocate a new scan page
		char *page;
		int num = 0;
		rc = pfm->AllocateBlock(page);
		if (rc != OK_RC) {
			cleanUp(pages, numrecs);
			return rc;
		}
		vector<char> data;
		for (int j = 0; j < recsPerPage; j++) {
			rc = scan->Next(data);
			if (rc == QL_EOF) {
				// stop iterating through the file
				break;
			} else if (rc != OK_RC) {
				// dispose scratch pages and return error
				cleanUp(pages, numrecs);
				return rc;
			} else {
				// put the record in the scratch page
				memcpy(page + num * recsize, &data[0], recsize);
				num++;
			}
		}
		// at this point rc can be QL_EOF or OK_RC
		if (rc == QL_EOF && num == 0) {
			pfm->DisposeBlock(page);
			break;
		}
		pages.push_back(page);
		numrecs.push_back(num);
		// sort the page
		pageSort(page, 0, num - 1);
	}
	// at this point rc can be QL_EOF or OK_RC
	return rc;
}

// to be called to free allocated scratch pages if error occurs
void EX_Sorter::cleanUp(vector<char*> &pages, vector<int> &numrecs) {
	for (unsigned int k = 0; k < pages.size(); k++) {
		// dont need to forward error because one has already occured
		pfm->DisposeBlock(pages[k]);
		pages.clear();
		numrecs.clear();
	}
}

/* 	Modify the RM classes to construct a file which allows creation of 
	files that can do inserts into a file maintaining the fill factor.
	1. The page numbers should be sequenctially assigned. 
	2. The number of pages in the file is stores in the file header
	3. Set up the free 

	Make a very lightweight class for doing sorted scans
	EX_SortManager, EX_SortHandle and EX_SortScan
	Functionalities needed-
	1. Load Insert record - store last inserted RID
	2. Insert record
	3. Delete record
	4. Each file has a corresponding index file in which minimum records
		of each page are stored
	EX_SortManager - Open, Delete, Create and Close File
	EX_SortHandle - InsertRec DeleteRec - Calls Delete file when overflow 
					occures
	EX_SortScan - Accesses the appropriate page from the catalog to fetch
					the desired reccord. Can be used for scan. Only supports
					scan on operators ==, <=, >=, >, <
*/

RC EX_Sorter::createSortedChunk(char *fileName, float ff, 
					vector<char*> &pages, vector<int> &numrecs) {
	// allocate a new file
	RC WARN = 503, ERR = -503;
	if (pages.size() == 0) return OK_RC;
	EX_ErrorForward(rmm->CreateFile(fileName, recsize));
	RM_FileHandle fh;
	EX_ErrorForward(rmm->OpenFile(fileName, fh));
	return OK_RC;
}

////////////////////////////////////////////////////////////
// RM style classes for managing sorted files
////////////////////////////////////////////////////////////


EX_Loader::EX_Loader(PF_Manager &pfm) {
	this->pfm = &pfm;
	isOpen = false;
}

EX_Loader::~EX_Loader() {
	// nothing to be done
}

RC EX_Loader::Create(char *fileName, float ff, int recsize) {
	RC WARN = 504, ERR = -504;
	if (isOpen) return WARN;
	this->ff = ff;
	this->recsize = recsize;
	this->capacity = (PF_PAGE_SIZE - sizeof(EX_PageHdr)) / recsize;
	if (recsize > PF_PAGE_SIZE - sizeof(EX_PageHdr)) return WARN;
	EX_ErrorForward(pfm->CreateFile(fileName));
	EX_ErrorForward(pfm->OpenFile(fileName, fh));
	isOpen = true;
	currPage = 0;
	recsInCurrPage = 0;
	return OK_RC;
}

RC EX_Loader::PutRec(char *data) {
	RC WARN = 505, ERR = -505;
	PF_PageHandle ph;
	if (!isOpen) return WARN;
	if (recsInCurrPage == 0) {
		// allocate a new page
		EX_ErrorForward(fh.AllocatePage(ph));
	} else {
		EX_ErrorForward(fh.GetThisPage(currPage, ph));
	}
	char *contents;
	EX_ErrorForward(ph.GetData(contents));
	EX_PageHdr *pHdr = (EX_PageHdr*) contents;
	char *dest = contents + sizeof(EX_PageHdr) + recsInCurrPage * recsize;
	memcpy(dest, data, recsize);
	recsInCurrPage++;
	pHdr->numrecs = recsInCurrPage;
	EX_ErrorForward(fh.MarkDirty(currPage));
	EX_ErrorForward(fh.UnpinPage(currPage));
	if (recsInCurrPage + 1 > ff * this->capacity) {
		// assign a new page if fill factor assigned
		recsInCurrPage = 0;
		currPage++;
	}
	return OK_RC;
}

RC EX_Loader::Close() {
	RC WARN = 506, ERR = -506;
	if (!isOpen) return WARN;
	EX_ErrorForward(pfm->CloseFile(fh));
	isOpen = false;
	return OK_RC;
}


EX_Scanner::EX_Scanner(PF_Manager &pfm) {
	this->pfm = &pfm;
	isOpen = false;
}

EX_Scanner::~EX_Scanner() {
	// nothing to be done
}

RC EX_Scanner::Open(char* fileName, int startPage, bool goRight, int recsize) {
	RC WARN = 507, ERR = -507;
	if (isOpen) return WARN;
	this->recsize = recsize;
	EX_ErrorForward(pfm->OpenFile(fileName, fh));
	isOpen = true;
	currPage = (goRight) ? 0 : fh.hdr.numPages - 1;
	currSlot = -1;
	increment = (goRight) ? 1 : -1;
	return OK_RC;
}

RC EX_Scanner::Next(vector<char> &rec) {
	RC WARN = 508, ERR = -508;
	if (!isOpen) return WARN;
	// check for end of file
	if ((currPage < 0 && increment < 0) ||
		(currPage == fh.hdr.numPages && increment > 0)) {
		return QL_EOF;
	}
	rec.resize(recsize);
	// get the current page and read its data
	char *contents;
	PF_PageHandle ph;
	EX_ErrorForward(fh.GetThisPage(currPage, ph));
	EX_ErrorForward(ph.GetData(contents));
	EX_PageHdr *pHdr = (EX_PageHdr*) contents;
	if (currSlot == -1) {
		currSlot = (increment > 0) ? 0 : pHdr->numrecs - 1;
	}
	int numrecs = pHdr->numrecs;
	char *src = contents + sizeof(EX_PageHdr) + currSlot * recsize;
	memcpy(&rec[0], src, recsize);
	EX_ErrorForward(fh.UnpinPage(currPage));

	// change the slot number
	currSlot += increment;
	if (currSlot < 0 || currSlot >= numrecs) {
		// assign a new slot and go to next page
		currSlot = -1;
		currPage += increment;
	}
	return OK_RC;
}

RC EX_Scanner::Reset() {
	RC WARN = 509;
	if (!isOpen) return WARN;
	currPage = (increment > 0) ? 0 : fh.hdr.numPages - 1;
	currSlot = -1;
	return OK_RC;
}

RC EX_Scanner::Close() {
	RC WARN = 510, ERR = -510;
	if (!isOpen) return WARN;
	EX_ErrorForward(pfm->CloseFile(fh));
	isOpen = false;
	return OK_RC;
}

