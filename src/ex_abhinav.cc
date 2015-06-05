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

RC EX_Loader::Create(char *fileName, float ff, int recsize, 
		bool makeIndex, int attrLength) {
	RC WARN = 504, ERR = -504;
	if (isOpen) return WARN;
	this->ff = ff;
	this->recsize = recsize;
	this->capacity = (PF_PAGE_SIZE - sizeof(EX_PageHdr)) / recsize;
	if (recsize > PF_PAGE_SIZE - (int) sizeof(EX_PageHdr)) return WARN;
	EX_ErrorForward(pfm->CreateFile(fileName));
	EX_ErrorForward(pfm->OpenFile(fileName, fh));
	isOpen = true;
	this->makeIndex = makeIndex;
	this->attrLength = attrLength;
	currPage = 0;
	recsInCurrPage = 0;
	// create a new file for index if makeIndex is true
	char indFile[3*MAXNAME + 10];
	sprintf(indFile, "%s.idx", fileName);
	// cout << "Making an index " << makeIndex << endl;
	// if (makeIndex) {
	// 	this->index = new EX_Loader(*pfm);
	// 	index->Create(indFile, 1.0, attrLength + sizeof(int), 
	// 					false, 0);
	// 	index->Close();
	// }
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
	this->seenEOF = false;
}

EX_Sorter::~EX_Sorter() {
	delete[] buffer;
}

RC EX_Sorter::sort(const char *fileName, float ff, bool makeIndex) {
	RC WARN = 501, ERR = -501;
   	// open the scan
	EX_ErrorForward(scan->Open());
	vector<char*> pages;
   	vector<int> numrecs;
   	RC rc = fillBuffer(pages, numrecs);
   	if (pages.size() == 0) return QL_EOF;
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
		EX_ErrorForward(loader.Create(temp, fillFactor, recsize, 
							makeIndex && qsize <= buffSize, attrLength));
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
	// DataAttrInfo* attrs = &(scan->attributes[0]);
	// Printer p(attrs, scan->attributes.size());
 //   	p.PrintHeader(cout);
 //    EX_Scanner escn(*pfm);
 //    escn.Open(fileName, 0, true, recsize);
 //    vector<char> data;
 //    while (escn.Next(data) == OK_RC) {
 //    	p.Print(cout, &data[0]);
 //    }
 //    p.PrintFooter(cout);
 //    escn.Close();
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
	if (seenEOF) return QL_EOF;
	for (int pn = 0; pn < bufferSize; pn++) {
		if (seenEOF) break;
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
				seenEOF = true;
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
		if (num == 0) {
			pfm->DisposeBlock(page);
			// no record was read
			if (pn == 0) return QL_EOF;
		} else {
			pages.push_back(page);
			numrecs.push_back(num);
		}
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
	EX_ErrorForward(loader.Create(&fname[0], ff, recsize, false, 0));
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
// Utility class for Buffer IO
//////////////////////////////////////////////////////

BufferIterator::BufferIterator(PF_Manager *pfm, int recsize, int bufferSize) {
	this->pfm = pfm;
	this->recsize = recsize;
	this->pagecap = PF_PAGE_SIZE / recsize;
	this->bufferSize = bufferSize;
	this->numrecs = 0;
	this->currPage = 0;
	this->currSlot = 0;
}

BufferIterator::~BufferIterator() {
	if (numrecs > 0) Clear();
}

RC BufferIterator::Clear() {
	RC WARN = 512, ERR = -512;
	for (unsigned int i = 0; i < pages.size(); i++) {
		EX_ErrorForward(pfm->DisposeBlock(pages[i]));
	}
	pages.clear();
	currPage = 0;
	currSlot = 0;
	numrecs = 0;
	return OK_RC;
}

RC BufferIterator::PutRec(vector<char> &rec) {
	RC WARN = 512, ERR = -512;
	if (currSlot == 0) {
		if (currPage == bufferSize) return WARN;
		char *page;
		EX_ErrorForward(pfm->AllocateBlock(page));
		pages.push_back(page);
	}
	// put the record in the current (page, slot)
	memcpy(pages[currPage] + recsize * currSlot, &rec[0], recsize);
	currSlot++;
	numrecs++;
	if (currSlot == pagecap) {
		currSlot = 0;
		currPage++;
	}
	return OK_RC;
}

int BufferIterator::Size() {
	return numrecs;
}

char* BufferIterator::GetRec(int i) {
	if (i >= numrecs) return 0;
	int pnum = i/pagecap;
	int snum = i % pagecap;
	return pages[pnum] + recsize * snum;
}
//////////////////////////////////////////////////////
// Sorting based operators
//////////////////////////////////////////////////////

/* Operator used in joins or ORDER BY queries */

EX_Sort::EX_Sort(PF_Manager* pfm, QL_Op &child, int attrIndex) {
	this->attributes = child.attributes;
	this->child = &child;
	this->parent = 0;
	child.parent = this;
	isOpen = false;
	this->isEmpty = false;
	deleteAtClose = (child.opType != RM_LEAF);
	if (deleteAtClose) {
		// generate a unique fileName
		fileName = new char[21];
		gen_random(fileName, 20);
		while (access(fileName, F_OK) == 0) {
			gen_random(fileName, 2*MAXNAME);
		}
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
	opType = SORT;
	desc << "SORT BY " << attributes[attrIndex].relName << ".";
	desc << attributes[attrIndex].attrName;
}

EX_Sort::~EX_Sort() {
	if (fileName) delete[] fileName;
	if (scanner) delete scanner;
	if (child) delete child;
}

/*  exhaust all the tuples of the child and make a new file. The
	new file is temporary if the child is not a filescan. If child
	is a filescan, check if a sorted file exists. If it does then open
	it otherwise create a new sorted file.
*/
RC EX_Sort::Open() {
	RC WARN = 511, ERR = -511;
	RC rc = OK_RC;
	if (deleteAtClose || access(fileName, F_OK) != 0) {
		// create the sorted file
		EX_Sorter sorter(*pfm, *child, attrIndex);
		rc = sorter.sort(fileName, 1.0, !deleteAtClose);
		if (rc != OK_RC && rc != QL_EOF) EX_ErrorForward(rc);
	}
	// open the file for scan
	if (rc == OK_RC) {
		scanner = new EX_Scanner(*pfm);
		EX_ErrorForward(scanner->Open(fileName, 0, true, recsize));
	} else {
		isEmpty = true;
	}
	return OK_RC;
}

RC EX_Sort::Next(vector<char> &rec) {
	rec.resize(recsize);
	if (isEmpty) return QL_EOF;
	return scanner->Next(rec);
}

RC EX_Sort::Reset() {
	return scanner->Reset();
}

RC EX_Sort::Close() {
	if (isEmpty) return OK_RC;
	if (deleteAtClose) {
		// delete the file
		unlink(fileName);
	}
	return scanner->Close();
}

void EX_Sort::gen_random(char *s, const int len) {
    srand(time(0));
    for (int i = 0; i < len; ++i) {
        int randomChar = rand()%(26+26+10);
        if (randomChar < 26)
            s[i] = 'a' + randomChar;
        else if (randomChar < 26+26)
            s[i] = 'A' + randomChar - 26;
        else
            s[i] = '0' + randomChar - 26 - 26;
    }
    s[len] = 0;
}


/* Merge Join operator - assumes children give records in ascending
	order of the join attribute
 */

EX_MergeJoin::EX_MergeJoin(PF_Manager *pfm, QL_Op &left, QL_Op &right, 
												const Condition *cond) {
	this->lchild = &left;
	this->rchild = &right;
	this->parent = 0;
	left.parent = this;
	right.parent = this;
	isOpen = false;
	int lindex = QL_Manager::findAttr(cond->lhsAttr.relName, 
				cond->lhsAttr.attrName, lchild->attributes);
	int rindex = QL_Manager::findAttr(cond->rhsAttr.relName, 
				cond->rhsAttr.attrName, rchild->attributes);
	if (lindex < 0 && rindex < 0) {
		lindex = QL_Manager::findAttr(cond->rhsAttr.relName, 
				cond->rhsAttr.attrName, lchild->attributes);
		rindex = QL_Manager::findAttr(cond->lhsAttr.relName, 
				cond->lhsAttr.attrName, rchild->attributes);
	}

	lattr = lchild->attributes[lindex];
	rattr = rchild->attributes[rindex];
	attributes = lchild->attributes;
	auto temp = rchild->attributes;
	// change offset of rchild attributes
	leftRecSize = attributes.back().offset + attributes.back().attrLength;
	for (unsigned int i = 0; i < temp.size(); i++) {
		temp[i].offset += leftRecSize;
	}
	attributes.insert(attributes.end(), temp.begin(), temp.end());
	rightRecSize = attributes.back().offset + 
			attributes.back().attrLength - leftRecSize;
	// change indexNo of all attributes
	for (unsigned int i = 0; i < attributes.size(); i++) {
		attributes[i].indexNo = -1;
	}
	// 2 for reading sorted relations and one for results
	int bufferSize = PF_BUFFER_SIZE - 3;
	this->buffit = new BufferIterator(pfm, rightRecSize, bufferSize);
	opType = MERGE_JOIN;
	desc << "MERGE JOIN ON " << "";
}

EX_MergeJoin::~EX_MergeJoin() {
	if (lchild != 0) delete lchild;
	if (rchild != 0) delete rchild;
	if (buffit != 0) delete buffit;
}

RC EX_MergeJoin::Open() {
	RC WARN = EX_MERGE_WARN, ERR = EX_MERGE_ERR;
	if (isOpen) return WARN;
	QL_ErrorForward(lchild->Open());
	QL_ErrorForward(rchild->Open());
	isOpen = true;
	leftrec.clear();
	rightrec.clear();
	buffit->Clear();
	bufferIndex = 0;
	rightEOF = false;
	return OK_RC;
}

RC EX_MergeJoin::Next(vector<char> &rec) {
	RC WARN = QL_EOF, ERR = EX_MERGE_ERR;
	if (leftrec.size() == 0) {
		leftrec.resize(leftRecSize);
		EX_ErrorForward(lchild->Next(leftrec));
	}
	if (rightrec.size() == 0) {
		rightrec.resize(rightRecSize);
		EX_ErrorForward(rchild->Next(rightrec));	
	}
	if (bufferIndex == buffit->Size() && bufferIndex > 0) {
		RC rc = lchild->Next(leftrec);
		if (rc != OK_RC) {
			EX_ErrorForward(buffit->Clear()); 
			return rc; 
		}
		bufferIndex = 0;
		// clear the buffer if the new record doesn't fit join criteria
		if (!equal(&leftrec[0], buffit->GetRec(0))) {
			EX_ErrorForward(buffit->Clear());
		}
	}
	// if buffer is not empty
	if (buffit->Size() > 0) {
		// join with the record at bufferIdx
		rec.resize(leftRecSize + rightRecSize);
		memcpy(&rec[0], &leftrec[0], leftRecSize);
		memcpy(&rec[leftRecSize], buffit->GetRec(bufferIndex), rightRecSize);
		bufferIndex++;
		return OK_RC;
	}
	if (rightEOF) return QL_EOF;
	bool found = false;
	while (!found) {
		if (less(&leftrec[0], &rightrec[0])) {
			EX_ErrorForward(lchild->Next(leftrec));
		} else if (more(&leftrec[0], &rightrec[0])) {
			EX_ErrorForward(rchild->Next(rightrec));
		} else {
			// fill buffer with right page
			RC rc;
			do {
				EX_ErrorForward(buffit->PutRec(rightrec));
				rc = rchild->Next(rightrec);
			} while (rc == OK_RC && equal(&leftrec[0], &rightrec[0]));
			if (rc == QL_EOF) rightEOF = true;
			else if (rc != OK_RC) return rc;
			// output join with the first record in buffer
			rec.resize(leftRecSize + rightRecSize);
			memcpy(&rec[0], &leftrec[0], leftRecSize);
			memcpy(&rec[leftRecSize], buffit->GetRec(0), rightRecSize);
			bufferIndex = 1;
			found = true;
		}
	}
	return OK_RC;
}

RC EX_MergeJoin::Reset() {
	RC WARN = EX_MERGE_WARN, ERR = EX_MERGE_ERR;
	if (!isOpen) return WARN;
	EX_ErrorForward(lchild->Reset());
	EX_ErrorForward(rchild->Reset());
	leftrec.clear();
	rightrec.clear();
	bufferIndex = 0;
	EX_ErrorForward(buffit->Clear());
	rightEOF = false;
	return OK_RC;
}

RC EX_MergeJoin::Close() {
	RC WARN = EX_MERGE_WARN, ERR = EX_MERGE_ERR;
	if (!isOpen) return WARN;
	EX_ErrorForward(lchild->Close());
	EX_ErrorForward(rchild->Close());
	EX_ErrorForward(buffit->Clear());
	return OK_RC;
}

bool EX_MergeJoin::less(char* v, char *w) {
	return QL_Manager::lt_op((void*) (v + lattr.offset), 
		(void*) (w + rattr.offset),	lattr.attrLength, rattr.attrLength, 
		lattr.attrType);
}

bool EX_MergeJoin::more(char* v, char *w) {
	return QL_Manager::gt_op((void*) (v + lattr.offset), 
		(void*) (w + rattr.offset),	lattr.attrLength, rattr.attrLength, 
		lattr.attrType);
}

bool EX_MergeJoin::equal(char* v, char *w) {
	return QL_Manager::eq_op((void*) (v + lattr.offset), 
		(void*) (w + rattr.offset),	lattr.attrLength, rattr.attrLength, 
		lattr.attrType);
}

///////////////////////////////////////////////////
// Query tree optimizers
///////////////////////////////////////////////////

EX_Optimizer::EX_Optimizer(PF_Manager* pfm) {
	this->pfm = pfm;
}

EX_Optimizer::~EX_Optimizer() {
	// nothing to do
}

void EX_Optimizer::mergeProjections (QL_Op* &root) {
	// check if the root is a condition openrator with equality attribute
	if (!root) return;
	// transform the child tree(s)
	if (root->opType >= 0) {
		auto temp = (QL_UnaryOp*) root;
		mergeProjections(temp->child);
	}
	else {
		auto temp = (QL_BinaryOp*) root;
		mergeProjections(temp->lchild);
		mergeProjections(temp->rchild);
	}
	if (root->opType != PROJ) return;
	auto proj = (QL_Projection*) root;
	if (proj->child->opType != PROJ) return;
	// the proj node and its child can be merged
	auto down = (QL_Projection*) proj->child;
	proj->child = down->child;
	proj->child->parent = root;
	proj->inputAttr = proj->child->attributes;
	proj->position.clear();
	for (unsigned int i = 0; i < proj->attributes.size(); i++) {
		DataAttrInfo* dtr = &proj->attributes[i];
		int idx = QL_Manager::findAttr(dtr->relName, dtr->attrName, 
			proj->inputAttr);
		proj->position.push_back((unsigned int) idx);
	}
	// detatch the down node and delete it
	down->child = 0;
	delete down;
	mergeProjections(root);
}


void EX_Optimizer::pushSort(QL_Op* &root) {
	if (!root) return;
	if (root->opType < 0) {
		auto bin = (QL_BinaryOp*) root;
		pushSort(bin->lchild);
		pushSort(bin->rchild);
	} else {
		auto uop = (QL_UnaryOp*) root;
		pushSort(uop->child);
	}
	if (root->opType != SORT) return;
	auto sort = (EX_Sort*) root;
	if (sort->child->opType == PROJ) {
		auto down = (QL_Projection*) sort->child;
		DataAttrInfo attr = sort->attributes[sort->attrIndex];
		int idx = QL_Manager::findAttr(attr.relName, attr.attrName,
			down->child->attributes);
		QL_Op* newsort = new EX_Sort(sort->pfm, *down->child, idx);
		newsort->parent = down;
		down->child = newsort;
		if (root->parent) {
			if (root->parent->opType >= 0) {
				// its a unary op
				auto uop = (QL_UnaryOp*) root->parent;
				uop->child = down;
			} 
			else if (root->parent->opType < 0) {
				auto bop = (QL_BinaryOp*) root->parent;
				if (bop->lchild == root) bop->lchild = down;
				if (bop->rchild == root) bop->rchild = down;
			}	
		}
		down->parent = root->parent;
		root = down;
		sort->child = 0;
		delete sort;
		pushSort(down->child);
	} else if (sort->child->opType == COND) {
		auto down = (QL_Condition*) sort->child;
		QL_Optimizer::swapUnUnOpPointers(sort, down);
		root = down;
		pushSort(down->child);
	} else {
		return;
	}
}

/*	Look for equality attribute conditions over cross product
	operators and replace cross product by merge join op and push
	sort between itself and its children. The previous optimization
	steps ensure that condition operator exists right above the
	cross product operator
*/
void EX_Optimizer::doSortMergeJoin(QL_Op* &root) {
	if (!root) return;
	if (root->opType < 0) {
		auto bin = (QL_BinaryOp*) root;
		doSortMergeJoin(bin->lchild);
		doSortMergeJoin(bin->rchild);
	} 
	else if (root->opType != COND) {
		auto un = (QL_UnaryOp*) root;
		doSortMergeJoin(un->child);
	}
	else {
		auto condOp = (QL_Condition*) root;
		if (condOp->child->opType != REL_CROSS) return;
		if (!condOp->cond->bRhsIsAttr) return;
		// at this point, there is a condition operator involving
		// two attributes, one from each side of the cross product op
		auto down = (QL_Cross*) condOp->child;
		// check if the children are worth sorting
		if (!(okToSort(down->lchild) && okToSort(down->rchild))) return;
		const Condition* cond = condOp->cond;
		// put sort operator above the two children
		int lindex = QL_Manager::findAttr(cond->lhsAttr.relName, 
						cond->lhsAttr.attrName, down->lchild->attributes);
		int rindex = QL_Manager::findAttr(cond->rhsAttr.relName, 
						cond->rhsAttr.attrName, down->rchild->attributes);
		if (lindex < 0 && rindex < 0) {
			lindex = QL_Manager::findAttr(cond->rhsAttr.relName, 
						cond->rhsAttr.attrName, down->lchild->attributes);
			rindex = QL_Manager::findAttr(cond->lhsAttr.relName, 
						cond->lhsAttr.attrName, down->rchild->attributes);
		}
		QL_Op* lsort = new EX_Sort(pfm, *down->lchild, lindex);
		QL_Op* rsort = new EX_Sort(pfm, *down->rchild, rindex);
		QL_Op* merge = new EX_MergeJoin(pfm, *lsort, *rsort, cond);
		merge->parent = root;
		condOp->child = merge;
		down->lchild = 0;
		down->rchild = 0;
		delete down;
	}
	return;
}

// check if it is worth sorting the output of an operator
bool EX_Optimizer::okToSort(QL_Op* root) {
	if (root->opType < 0) return true;
	if (root->opType == RM_LEAF) return true;
	if (root->opType == COND) {
		auto condOp = (QL_Condition*) root;
		CompOp cmp = condOp->cond->op;
		return okToSort(condOp->child) && (cmp != EQ_OP);
	}
	if (root->opType == PROJ) {
		auto projOp = (QL_Projection*) root;
		return okToSort(projOp->child);
	}
	return false;
}

