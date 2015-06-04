/* Redbase Extension - Sort based optimization

1. 	Quicksort code adapted from java quicksort code on
	http://algs4.cs.princeton.edu/23quicksort/Quick.java.html

*/


#include <memory>
#include <sstream>
#include <queue>
#include <assert.h>
#include <unistd.h>
#include <stdio.h>
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


////////////////////////////////////////////////////////////
// RM style classes for managing sorted files
////////////////////////////////////////////////////////////

/* 	Modify the RM classes to construct a file which allows creation of 
	files that can do inserts into a file maintaining the fill factor.
	1. The page numbers should be sequenctially assigned. 
	2. The number of pages in the file is stores in the file header
	
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
	if (recsize > PF_PAGE_SIZE - (int) sizeof(EX_PageHdr)) return WARN;
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

RC EX_Scanner::Open(const char* fileName, int startPage, 
									bool goRight, int recsize) {
	RC WARN = 507, ERR = -507;
	if (isOpen) return WARN;
	this->recsize = recsize;
	EX_ErrorForward(pfm->OpenFile(fileName, fh));
	isOpen = true;
	if (startPage < 0) {
		currPage = (goRight) ? 0 : fh.hdr.numPages - 1;
	} else {
		currPage = startPage;
	}
	currPageForReset = currPage;
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
	currPage = currPageForReset;
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
/*	Read a page from the file and return the number of records
*/
RC EX_Scanner::NextBlock(char *block, int &numrecs) {
	RC WARN = 510, ERR = -510;
	if (!isOpen) return WARN;
	// check for end of file
	if ((currPage < 0 && increment < 0) ||
		(currPage == fh.hdr.numPages && increment > 0)) {
		return QL_EOF;
	}
	// get the current page and read its data
	char *contents;
	PF_PageHandle ph;
	EX_ErrorForward(fh.GetThisPage(currPage, ph));
	EX_ErrorForward(ph.GetData(contents));
	EX_PageHdr *pHdr = (EX_PageHdr*) contents;
	numrecs = pHdr->numrecs;
	char *src = contents + sizeof(EX_PageHdr);
	memcpy(block, src, numrecs * recsize);
	EX_ErrorForward(fh.UnpinPage(currPage));

	// change the page number
	currPage += increment;
	return OK_RC;
}


////////////////////////////////////////////////////////////
// Sorter class - creates sorted files, used by operators
////////////////////////////////////////////////////////////


/* Sorts the tuples output by scan based on the attribute attrIndex
	in ascending order. The sorted file is stored in the file named
	fileName.
*/
EX_Sorter::EX_Sorter(PF_Manager &pfm, QL_Op &scan, int attrIndex) {
	this->pfm = &pfm;
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

RC EX_Sorter::sort(const char *fileName, float ff) {
	RC WARN = 501, ERR = -501;
   	// open the scan
	EX_ErrorForward(scan->Open());
	vector<char*> pages;
   	vector<int> numrecs;
   	RC rc = fillBuffer(pages, numrecs);
   	int chunkNum = 0;
   	queue<int> fileq;
   	while (rc == OK_RC) {
   		EX_ErrorForward(createSortedChunk(fileName, chunkNum, 1.0,
    			pages, numrecs));
   		fileq.push(chunkNum);
    	chunkNum++;
    	cleanUp(pages, numrecs);
   		rc = fillBuffer(pages, numrecs);
   	}
   	if (rc != QL_EOF) EX_ErrorForward(rc);
   	// at this point, sorted chunks have been created
	EX_ErrorForward(scan->Close());
	cleanUp(pages, numrecs);
	// now merge the sorted chunks
	// use all except one buffer page to read the chunks. This
	// page will be used to read a new block when a page from a 
	// file is exhausted
	int buffSize = PF_BUFFER_SIZE - 1;
	vector<EX_Scanner*> scanners;
	vector<int> chunkIndices;
	while (fileq.size() > 1) {
		// merge files from the front of the queue and enqueue
		// the result at the back
		scanners.clear();
		pages.clear();
		numrecs.clear();
		char temp[3*MAXNAME];
		int qsize = fileq.size();
		// open scan on files and read one page from each file
		for (int i = 0; i < min(qsize, buffSize); i++) {
			EX_Scanner* scn = new EX_Scanner(*pfm);
			sprintf(temp, "_%s.%d", fileName, fileq.front());
			chunkIndices.push_back(fileq.front());
			fileq.pop();
			EX_ErrorForward(scn->Open(temp, 0, true, recsize));
			char *buffpage;
			int num;
			EX_ErrorForward(pfm->AllocateBlock(buffpage));
			EX_ErrorForward(scn->NextBlock(buffpage, num));
			pages.push_back(buffpage);
			numrecs.push_back(num);
			scanners.push_back(scn);
		}
		// merge blocks and output to a merged file
		EX_Loader loader(*pfm);
		sprintf(temp, "_%s.%d", fileName, chunkNum);
		fileq.push(chunkNum);
		chunkNum++;
		float fillFactor = (qsize > buffSize) ? 1.0 : ff;
		EX_ErrorForward(loader.Create(temp, fillFactor, recsize));
		vector<int> index(scanners.size(), 0);
		unsigned int exhaustedCount = 0;
		RC rc;
		while (exhaustedCount != scanners.size()) {
			// get the index of the minimum element
			int idx = findIndex(pages, numrecs, index);
			EX_ErrorForward(loader.PutRec(pages[idx] + index[idx] * recsize));
			index[idx]++;
			if (index[idx] == numrecs[idx]) {
				rc = scanners[idx]->NextBlock(pages[idx], numrecs[idx]);
				if (rc == OK_RC) index[idx] = 0;
				else if (rc != QL_EOF){
					cleanUp(pages, numrecs);
					EX_ErrorForward(rc);
				}
				else exhaustedCount++; 
			}
		}
		cleanUp(pages, numrecs);
		// close the scanners and delete the files
		for (unsigned int i = 0; i < scanners.size(); i++) {
			EX_ErrorForward(scanners[i]->Close());
			delete scanners[i];
			char temp[3 * MAXNAME];
			sprintf(temp, "_%s.%d", fileName, chunkIndices[i]);
			if (unlink(temp) != 0) return WARN;
		}
		chunkIndices.clear();
		EX_ErrorForward(loader.Close());
	}
	// now there is exactly one file in the fileq, rename it to desired file
	// rename the chunk file to fileName
	char temp[3 * MAXNAME];
	sprintf(temp, "_%s.%d", fileName, fileq.front());
	if (rename(temp, fileName) != 0) return WARN;
	// verify the sorted file 
	DataAttrInfo* attrs = &(scan->attributes[0]);
	Printer p(attrs, scan->attributes.size());
   	p.PrintHeader(cout);
    EX_Scanner escn(*pfm);
    escn.Open(fileName, 0, true, recsize);
    vector<char> data;
    while (escn.Next(data) == OK_RC) {
    	p.Print(cout, &data[0]);
    }
    p.PrintFooter(cout);
    escn.Close();
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
    if (lo != j) exch(page + lo * recsize, page + j * recsize);

    // now, page[lo .. j-1] <= page[j] <= a[j+1 .. hi]
    return j;
}


bool EX_Sorter::less (char* v, char *w) {
	// compare v + offset and w + offset
	if (v == 0 || w == 0) return true; // used in findIndex
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
	for (int pn = 0; pn < bufferSize; pn++) {
		// allocate a new scan page
		char *page;
		rc = pfm->AllocateBlock(page);
		if (rc != OK_RC) {
			pfm->DisposeBlock(page);
			cleanUp(pages, numrecs);
			return rc;
		}
		vector<char> data;
		int num = 0;
		for (int j = 0; j < recsPerPage; j++) {
			rc = scan->Next(data);
			if (rc == QL_EOF) {
				// stop iterating through the file
				break;
			} else if (rc != OK_RC) {
				// dispose scratch pages and return error
				pfm->DisposeBlock(page);
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
			// no record was read
			if (pn == 0) return QL_EOF;
			break;
		}
		pages.push_back(page);
		numrecs.push_back(num);
		// sort the page
		pageSort(page, 0, num - 1);
	}
	return OK_RC;
}

// to be called to free allocated scratch pages if error occurs
void EX_Sorter::cleanUp(vector<char*> &pages, vector<int> &numrecs) {
	for (unsigned int k = 0; k < pages.size(); k++) {
		// dont need to forward error because one has already occured
		pfm->DisposeBlock(pages[k]);
	}
	pages.clear();
	numrecs.clear();
}

/* 	finds the page containing the smallest record at the indices pointed to
	by the vector index.
*/

int EX_Sorter::findIndex(vector<char*> &pages, vector<int> &numrecs, 
													vector<int> &index) {
	// find the first valid page, which has not exhausted all its records
	int valid = -1;
	assert(index.size() == numrecs.size());
	char *record = 0;
	for (unsigned int i = 0; i < index.size(); i++) {
		char *curr = pages[i] + index[i] * recsize;
		if (index[i] < numrecs[i] && less(curr, record)) {
			record = curr;
			valid = i;
		}
	}
	return valid;
}




RC EX_Sorter::createSortedChunk(const char *fileName, int chunkNum, float ff, 
					vector<char*> &pages, vector<int> &numrecs) {
	// allocate a new file
	RC WARN = 503, ERR = -503;
	if (pages.size() == 0) return OK_RC;
	EX_Loader loader(*pfm);
	vector<char> fname(3*MAXNAME);
	sprintf(&fname[0], "_%s.%d", fileName, chunkNum);
	EX_ErrorForward(loader.Create(&fname[0], ff, recsize));
	vector<int> index(pages.size(), 0);
	int idx = findIndex(pages, numrecs, index);
	while (idx >= 0) {
		EX_ErrorForward(loader.PutRec(pages[idx] + index[idx] * recsize));
		index[idx]++;
		idx = findIndex(pages, numrecs, index);
	}
	EX_ErrorForward(loader.Close());
	return OK_RC;
}




//////////////////////////////////////////////////////
// Sorting based operators
//////////////////////////////////////////////////////

EX_Sort::EX_Sort(PF_Manager* pfm, QL_Op &child, int attrIndex) {
	this->attributes = child.attributes;
	this->child = &child;
	this->parent = 0;
	child.parent = this;
	isOpen = false;
	deleteAtClose = (child.opType != RM_LEAF);
	if (deleteAtClose) {
		// generate a tmp name
		fileName = new char[L_tmpnam];
		tmpnam(fileName);
	} else {
		// use relname.attrname.sorted
		fileName = new char[2*MAXNAME + 10];
		sprintf(fileName, "%s.%s.sorted", attributes[attrIndex].relName, 
			attributes[attrIndex].attrName);
	}
	this->attrIndex = attrIndex;
	this->recsize = attributes.back().attrLength + attributes.back().offset;
	this->pfm = pfm;
	this->scanner = new EX_Scanner(*pfm);
	// change indexNo of all attributes
	for (unsigned int i = 0; i < this->attributes.size(); i++) {
		this->attributes[i].indexNo = -1;
	}
	desc << "SORT BY " << attributes[attrIndex].relName << ".";
	desc << attributes[attrIndex].attrName;
}

EX_Sort::~EX_Sort() {
	if (fileName) delete[] fileName;
	if (scanner) delete scanner;
}

/*  exhaust all the tuples of the child and make a new file. The
	new file is temporary if the child is not a filescan. If child
	is a filescan, check if a sorted file exists. If it does then open
	it otherwise create a new sorted file.
*/
RC EX_Sort::Open() {
	RC WARN = 511, ERR = -511;
	if (deleteAtClose || access(fileName, F_OK) != 0) {
		// create the sorted file
		EX_Sorter sorter(*pfm, *child, attrIndex);
		EX_ErrorForward(sorter.sort(fileName, 1.0));
	}
	// open the file for scan
	scanner = new EX_Scanner(*pfm);
	EX_ErrorForward(scanner->Open(fileName, 0, true, recsize));
	return OK_RC;
}

RC EX_Sort::Next(vector<char> &rec) {
	return scanner->Next(rec);
}

RC EX_Sort::Reset() {
	return scanner->Reset();
}

RC EX_Sort::Close() {
	return scanner->Close();
}