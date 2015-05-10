//
// File:        rm_testshell.cc
// Description: Test RM component
// Authors:     Jan Jannink
//              Dallan Quass (quass@cs.stanford.edu)
//              Jason McHugh (mchughj@cs.stanford.edu)
//
// This test shell contains a number of functions that will be useful
// in testing your RM component code.  In addition, a couple of sample
// tests are provided.  The tests are by no means comprehensive, however,
// and you are expected to devise your own tests to test your code.
//
// 1997:  Tester has been modified to reflect the change in the 1997
// interface.  For example, FileHandle no longer supports a Scan over the
// relation.  All scans are done via a FileScan.
//

#include <cstdio>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cstdlib>
#include <stdlib.h>

#include "redbase.h"
#include "pf.h"
#include "rm.h"

using namespace std;

//
// Defines
//
#define FILENAME   "testrel"         // test file name
#define STRLEN      29               // length of string in testrec
#define PROG_UNIT   50               // how frequently to give progress
                                      //   reports when adding lots of recs
#define FEW_RECS   100000                // number of records added in

//
// Computes the offset of a field in a record (should be in <stddef.h>)
//
#ifndef offsetof
#       define offsetof(type, field)   ((size_t)&(((type *)0) -> field))
#endif

//
// Structure of the records we will be using for the tests
//
struct TestRec {
    int   num;
    float r;
    char  str[STRLEN];
};

//
// Global PF_Manager and RM_Manager variables
//
PF_Manager pfm;
RM_Manager rmm(pfm);

//
// Function declarations
//
RC Test1(void);
RC Test2(void);
RC Test3(void);
RC Test4(void);

void PrintError(RC rc);
void LsFile(char *fileName);
void PrintRecord(TestRec &recBuf);
RC AddRecs(RM_FileHandle &fh, int numRecs);
RC VerifyFile(RM_FileHandle &fh, int numRecs);
RC PrintFile(RM_FileHandle &fh);

RC CreateFile(char *fileName, int recordSize);
RC DestroyFile(char *fileName);
RC OpenFile(char *fileName, RM_FileHandle &fh);
RC CloseFile(char *fileName, RM_FileHandle &fh);
RC InsertRec(RM_FileHandle &fh, char *record, RID &rid);
RC UpdateRec(RM_FileHandle &fh, RM_Record &rec);
RC DeleteRec(RM_FileHandle &fh, RID &rid);
RC GetNextRecScan(RM_FileScan &fs, RM_Record &rec);

//
// Array of pointers to the test functions
//
#define NUM_TESTS       4               // number of tests
int (*tests[])() =                      // RC doesn't work on some compilers
{
    Test1,
    Test2,
    Test3,
    Test4
};

//
// main
//
int main(int argc, char *argv[])
{
    RC   rc;
    char *progName = argv[0];   // since we will be changing argv
    int  testNum;

    // Write out initial starting message
    cerr.flush();
    cout.flush();
    cout << "Starting RM component test.\n";
    cout.flush();

    // Delete files from last time
    unlink(FILENAME);

    // If no argument given, do all tests
    if (argc == 1) {
        for (testNum = 0; testNum < NUM_TESTS; testNum++)
            if ((rc = (tests[testNum])())) {

                // Print the error and exit
                PrintError(rc);
                return (1);
            }
    }
    else {

        // Otherwise, perform specific tests
        while (*++argv != NULL) {

            // Make sure it's a number
            if (sscanf(*argv, "%d", &testNum) != 1) {
                cerr << progName << ": " << *argv << " is not a number\n";
                continue;
            }

            // Make sure it's in range
            if (testNum < 1 || testNum > NUM_TESTS) {
                cerr << "Valid test numbers are between 1 and " << NUM_TESTS << "\n";
                continue;
            }

            // Perform the test
            if ((rc = (tests[testNum - 1])())) {

                // Print the error and exit
                PrintError(rc);
                return (1);
            }
        }
    }

    // Write ending message and exit
    cout << "Ending RM component test.\n\n";

    return (0);
}

//
// PrintError
//
// Desc: Print an error message by calling the proper component-specific
//       print-error function
//
void PrintError(RC rc)
{
    if (abs(rc) <= END_PF_WARN)
        PF_PrintError(rc);
    else if (abs(rc) <= END_RM_WARN)
        RM_PrintError(rc);
    else
        cerr << "Error code out of range: " << rc << "\n";
}

////////////////////////////////////////////////////////////////////
// The following functions may be useful in tests that you devise //
////////////////////////////////////////////////////////////////////

//
// LsFile
//
// Desc: list the filename's directory entry
//
void LsFile(char *fileName)
{
    char command[80];

    sprintf(command, "ls -l %s", fileName);
    printf("doing \"%s\"\n", command);
    system(command);
}

//
// PrintRecord
//
// Desc: Print the TestRec record components
//
void PrintRecord(TestRec &recBuf)
{
    printf("[%s, %d, %f]\n", recBuf.str, recBuf.num, recBuf.r);
}

//
// AddRecs
//
// Desc: Add a number of records to the file
//
RC AddRecs(RM_FileHandle &fh, int numRecs)
{
    RC      rc;
    int     i;
    TestRec recBuf;
    RID     rid;
    PageNum pageNum;
    SlotNum slotNum;

    // We set all of the TestRec to be 0 initially.  This heads off
    // warnings that Purify will give regarding UMR since sizeof(TestRec)
    // is 40, whereas actual size is 37.
    memset((void *)&recBuf, 0, sizeof(recBuf));

    printf("\nadding %d records\n", numRecs);
    for (i = 0; i < numRecs; i++) {
        memset(recBuf.str, ' ', STRLEN);
        sprintf(recBuf.str, "a%d", i);
        recBuf.num = i;
        recBuf.r = (float)i;
        if ((rc = InsertRec(fh, (char *)&recBuf, rid)) ||
            (rc = rid.GetPageNum(pageNum)) ||
            (rc = rid.GetSlotNum(slotNum)))
            return (rc);

        if ((i + 1) % PROG_UNIT == 0){
            printf("%d  ", i + 1);
            fflush(stdout);
        }
    }
    if (i % PROG_UNIT != 0)
        printf("%d\n", i);
    else
        putchar('\n');

    // Return ok
    return (0);
}

//
// VerifyFile
//
// Desc: verify that a file has records as added by AddRecs
//
RC VerifyFile(RM_FileHandle &fh, int numRecs)
{
    RC        rc;
    int       n;
    TestRec   *pRecBuf;
    RID       rid;
    char      stringBuf[STRLEN];
    char      *found;
    RM_Record rec;

    found = new char[numRecs];
    memset(found, 0, numRecs);

    printf("\nverifying file contents\n");

    RM_FileScan fs;
    if ((rc=fs.OpenScan(fh,INT,sizeof(int),offsetof(TestRec, num),
                        NO_OP, NULL, NO_HINT)))
        return (rc);

    // For each record in the file
    for (rc = GetNextRecScan(fs, rec), n = 0;
         rc == 0;
         rc = GetNextRecScan(fs, rec), n++) {

        // Make sure the record is correct
        if ((rc = rec.GetData((char *&)pRecBuf)) ||
            (rc = rec.GetRid(rid)))
            goto err;

        memset(stringBuf,' ', STRLEN);
        sprintf(stringBuf, "a%d", pRecBuf->num);

        if (pRecBuf->num < 0 || pRecBuf->num >= numRecs ||
            strcmp(pRecBuf->str, stringBuf) ||
            pRecBuf->r != (float)pRecBuf->num) {
            printf("VerifyFile: invalid record = [%s, %d, %f]\n",
                   pRecBuf->str, pRecBuf->num, pRecBuf->r);
            exit(1);
        }

        if (found[pRecBuf->num]) {
            printf("VerifyFile: duplicate record = [%s, %d, %f]\n",
                   pRecBuf->str, pRecBuf->num, pRecBuf->r);
            exit(1);
        }

        found[pRecBuf->num] = 1;
    }

    if (rc != RM_EOF)
        goto err;

    if ((rc=fs.CloseScan()))
        return (rc);

    // make sure we had the right number of records in the file
    if (n != numRecs) {
        printf("%d records in file (supposed to be %d)\n",
               n, numRecs);
        exit(1);
    }

    // Return ok
    rc = 0;

err:
    fs.CloseScan();
    delete[] found;
    return (rc);
}

//
// PrintFile
//
// Desc: Print the contents of the file
//
RC PrintFile(RM_FileScan &fs)
{
    RC        rc;
    int       n;
    TestRec   *pRecBuf;
    RID       rid;
    RM_Record rec;

    printf("\nprinting file contents\n");

    // for each record in the file
    for (rc = GetNextRecScan(fs, rec), n = 0;
         rc == 0;
         rc = GetNextRecScan(fs, rec), n++) {

        // Get the record data and record id
        if ((rc = rec.GetData((char *&)pRecBuf)) ||
            (rc = rec.GetRid(rid)))
            return (rc);

        // Print the record contents
        PrintRecord(*pRecBuf);
    }

    if (rc != RM_EOF)
        return (rc);

    printf("%d records found\n", n);

    // Return ok
    return (0);
}

////////////////////////////////////////////////////////////////////////
// The following functions are wrappers for some of the RM component  //
// methods.  They give you an opportunity to add debugging statements //
// and/or set breakpoints when testing these methods.                 //
////////////////////////////////////////////////////////////////////////

//
// CreateFile
//
// Desc: call RM_Manager::CreateFile
//
RC CreateFile(char *fileName, int recordSize)
{
    // printf("\ncreating %s\n", fileName);
    return (rmm.CreateFile(fileName, recordSize));
}

//
// DestroyFile
//
// Desc: call RM_Manager::DestroyFile
//
RC DestroyFile(char *fileName)
{
    // printf("\ndestroying %s\n", fileName);
    return (rmm.DestroyFile(fileName));
}

//
// OpenFile
//
// Desc: call RM_Manager::OpenFile
//
RC OpenFile(char *fileName, RM_FileHandle &fh)
{
    // printf("\nopening %s\n", fileName);
    return (rmm.OpenFile(fileName, fh));
}

//
// CloseFile
//
// Desc: call RM_Manager::CloseFile
//
RC CloseFile(char *fileName, RM_FileHandle &fh)
{
    //if (fileName != NULL)
        //printf("\nClosing %s\n", fileName);
    return (rmm.CloseFile(fh));
}

//
// InsertRec
//
// Desc: call RM_FileHandle::InsertRec
//
RC InsertRec(RM_FileHandle &fh, char *record, RID &rid)
{
    return (fh.InsertRec(record, rid));
}

//
// DeleteRec
//
// Desc: call RM_FileHandle::DeleteRec
//
RC DeleteRec(RM_FileHandle &fh, RID &rid)
{
    return (fh.DeleteRec(rid));
}

//
// UpdateRec
//
// Desc: call RM_FileHandle::UpdateRec
//
RC UpdateRec(RM_FileHandle &fh, RM_Record &rec)
{
    return (fh.UpdateRec(rec));
}

//
// GetNextRecScan
//
// Desc: call RM_FileScan::GetNextRec
//
RC GetNextRecScan(RM_FileScan &fs, RM_Record &rec)
{
    return (fs.GetNextRec(rec));
}

void PF_Statistics();
/////////////////////////////////////////////////////////////////////
// Sample test functions follow.                                   //
/////////////////////////////////////////////////////////////////////

//
// Test1 tests simple creation, opening, closing, and deletion of files
//
RC Test1(void)
{
    RC            rc;
    RM_FileHandle fh;

    printf("test1 starting ****************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile(FILENAME, fh)) ||
        (rc = CloseFile(FILENAME, fh)))
        return (rc);

    LsFile(FILENAME);

    if ((rc = DestroyFile(FILENAME)))
        return (rc);
    PF_Statistics();
    printf("\ntest1 done ********************\n");
    return (0);
}

//
// Test2 tests adding a few records to a file.
//
#define filename "testrel2" // invalid filename
#define fname "testrel3" // invalid filename
RC Test2(void)
{
    RC            rc;
    RM_FileHandle fh1, fh2;
    RM_FileScan fs;
    RM_Record rec;
    RID rid(1,0), rid2(2,1);

    printf("test2 starting ****************\n");

    cout<<"\ncreating file with recsize = 0\n";
    if ((rc = CreateFile(fname, 0)))  RM_PrintError(rc);

    cout<<"\ncreating file with recsize > PF_PAGE_SIZE\n";
    if ((rc = CreateFile(fname, PF_PAGE_SIZE+1)))  RM_PrintError(rc);

    cout<<"\ncreating file with invalid filename\n";
    if ((rc = CreateFile("abc/abc", sizeof(TestRec))))  RM_PrintError(rc);

    cout<<"\ncreating the same file twice\n";
    if ((rc = CreateFile(fname, sizeof(TestRec))))  RM_PrintError(rc);
    if ((rc = CreateFile(fname, sizeof(TestRec))))  RM_PrintError(rc);
    
    cout<<"\nopening file with invalid filename\n";
    if ((rc = OpenFile("asda/asda", fh1)))  RM_PrintError(rc);
    
    cout<<"\ngetting first record of empty file\n";
    void*    x;
    RM_Record rec2;
    if ((rc = OpenFile(fname, fh1)))  RM_PrintError(rc);
    if ((rc = fs.OpenScan(fh1, INT, 4, 0, NE_OP, x, NO_HINT))) RM_PrintError(rc);
    if ((rc = fs.GetNextRec(rec2))) RM_PrintError(rc);
    if ((rc = fs.CloseScan()))  RM_PrintError(rc);
    // if ((rc = fh1.GetRec(rid, rec2))) RM_PrintError(rc);
    
    cout<<  "\nremoving nonexistent record\n";
    if ((rc = fh1.DeleteRec(rid2)))  RM_PrintError(rc);
    
    cout<<"\nadding record to unopened file handle\n";
    TestRec trec;
    memset((void *)&trec, 0, sizeof(trec));
    char * temp_rec = (char*) &trec;
    if ((rc = InsertRec(fh2, temp_rec, rid))) RM_PrintError(rc);

    cout<<"\nremoving record from invalid file handle\n";
    if ((rc = DeleteRec(fh2, rid))) RM_PrintError(rc);

    cout<<"\ngetting first record with invalid file handle\n";
    RM_Record rec1;
    if ((rc = fh2.GetRec(rid, rec1))) RM_PrintError(rc);

    cout<<"\nopening scan with invalid file handle\n";
    if ((rc = fs.OpenScan(fh2, INT, 4, 0, NE_OP, x, NO_HINT))) RM_PrintError(rc);


    cout<<"\ncalling RM_PrintError with corrupted return code\n";
    RM_PrintError(9999);

    TestRec t;
    memset((void *)&t, 0, sizeof(t));
    for (int i = 0; i < 1; i++) {
        if ((rc = fh1.InsertRec((char*) &t, rid))) RM_PrintError(rc);
    }
    // for (int i = 0; i < 1000; i++) {
        if ((rc = fh1.DeleteRec(rid))) RM_PrintError(rc);
        if ((rc = fh1.DeleteRec(rid))) RM_PrintError(rc);
    // }
    if ((rc = CloseFile(fname, fh1)))  RM_PrintError(rc);
    if ((rc = DestroyFile(fname))) RM_PrintError(rc);
    
    printf("\ntest2 done ********************\n");
    return (0);
}


/////////////////////////////////////////////////////////////////////////
//                          New Tests                                  //
/////////////////////////////////////////////////////////////////////////



void gen_random(char *s, const int len) {
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


//
// Test3 tests adding random strings 
//

#define err(expr) if ((rc = (expr))) return (rc)

RC Test3(void)
{
    RC            rc;
    RM_FileHandle fh;
    RM_FileScan fs;
    RM_Record temp_rec;

    int     i;
    TestRec recBuf;
    RID     rid;
    PageNum pageNum;
    SlotNum slotNum;
    int numRecs = 120000;

    printf("test3 starting ****************\n");

    if ((rc = CreateFile(FILENAME, sizeof(TestRec))) ||
        (rc = OpenFile(FILENAME, fh)))
        return (rc);

    // We set all of the TestRec to be 0 initially.  This heads off
    // warnings that Purify will give regarding UMR since sizeof(TestRec)
    // is 40, whereas actual size is 37.
    memset((void *)&recBuf, 0, sizeof(recBuf));

    printf("\nadding %d random records\n", numRecs);
    for (i = 0; i < numRecs; i++) {
        gen_random(recBuf.str, STRLEN - 1);
        recBuf.num = i;
        recBuf.r = (float) i/3;
        if ((rc = InsertRec(fh, (char *)&recBuf, rid)) ||
            (rc = rid.GetPageNum(pageNum)) ||
            (rc = rid.GetSlotNum(slotNum)))
            return (rc);
    }
    err(CloseFile(FILENAME, fh));
    
    // Select using null data
    err(OpenFile(FILENAME, fh));
    void *val;
    err(fs.OpenScan(fh, INT, 4, 0, NE_OP, val, NO_HINT));
    int count = 0;
    while (fs.GetNextRec(temp_rec) == OK_RC) count ++;
    printf("\nTotal %d out of %d records found\n", count, numRecs);
    err(fs.CloseScan());

    // select for <
    int lim = 40;
    err(fs.OpenScan(fh, INT, 4, 0, LT_OP, (void*) &lim, NO_HINT));
    count = 0;
    while (fs.GetNextRec(temp_rec) == OK_RC) count ++;
    printf("\nTotal %d out of %d records found\n", count, numRecs);
    err(fs.CloseScan());

    // select for <=
    lim = 40;
    err(fs.OpenScan(fh, INT, 4, 0, LE_OP, (void*) &lim, NO_HINT));
    count = 0;
    while (fs.GetNextRec(temp_rec) == OK_RC) count ++;
    printf("\nTotal %d out of %d records found\n", count, numRecs);
    err(fs.CloseScan());

    err(rc = DestroyFile(FILENAME));
    printf("\ntest3 done ********************\n");
    return (0);
}


//
// Test 4
//

RC Test4(void)
{
    RC            rc;
    RM_FileHandle fh;
    RM_FileScan fs;
    RM_Record temp_rec;

    int     i;
    char record[10];
    RID     rid;
    PageNum pageNum;
    SlotNum slotNum;
    int numRecs = 120;
    int lim = 80;
    CompOp op = NE_OP;

    printf("test4 starting ****************\n");
    cout<<"size of record is " << sizeof(record)<<endl;
    if ((rc = CreateFile(FILENAME, sizeof(record))) ||
        (rc = OpenFile(FILENAME, fh)))
        return (rc);

    // We set all of the TestRec to be 0 initially.  This heads off
    // warnings that Purify will give regarding UMR since sizeof(TestRec)
    // is 40, whereas actual size is 37.
    memset((void *)record, 0, sizeof(record));

    printf("\nadding %d random records\n", numRecs);
    for (i = 0; i < numRecs; i++) {
        memcpy(record+2, &i, 4);
        cout<<*((int*) (record+2))<<"\t";
        if ((rc = InsertRec(fh, record, rid)) ||
            (rc = rid.GetPageNum(pageNum)) ||
            (rc = rid.GetSlotNum(slotNum)))
            return (rc);
    }
    err(CloseFile(FILENAME, fh));
    
    err(OpenFile(FILENAME, fh));
    void *val;
    int count = 0;
    char *result;

    // select for <=
    lim = 40;
    op = EQ_OP;
    err(fs.OpenScan(fh, INT, 4, 2, op, (void*) &lim, NO_HINT));
    count = 0;
    while (fs.GetNextRec(temp_rec) == OK_RC) {
        count ++;
        err(temp_rec.GetData(result));
        //cout<<((TestRec*) result)->num<<endl;
    }

    printf("\nTotal %d out of %d records found\n", count, numRecs);
    err(fs.CloseScan());
    op = (CompOp) 1;
    err(fs.OpenScan(fh, INT, 4, 2, op, (void*) &lim, NO_HINT));
    count = 0;
    while (fs.GetNextRec(temp_rec) == OK_RC) {
        count ++;
        err(temp_rec.GetRid(rid));
        err(fh.DeleteRec(rid));
        //cout<<((TestRec*) result)->num<<endl;
    }
    cout<<"Deleted records : "<<count<<endl;
    err(fs.CloseScan());
    err(rc = CloseFile(FILENAME, fh));

    err(OpenFile(FILENAME, fh));
    op = NO_OP;
    err(fs.OpenScan(fh, INT, 4, 2, op, (void*) &lim, NO_HINT));
    PrintError(fs.GetNextRec(temp_rec));
    err(fs.CloseScan());

    err(CloseFile(FILENAME, fh));
    err(DestroyFile(FILENAME));

    // PF_Statistics();
    printf("\ntest4 done ********************\n");
    return (0);
}