#include <cerrno>
#include <cstdio>
#include <iostream>
#include "rm.h"

using namespace std;

//
// Error table
//
static char *RM_WarnMsg[] = {
  (char*)"invalid record size",
  (char*)"recoverable error during file creation",
  (char*)"recoverable error during file deletion",
  (char*)"recoverable error while opening file",
  (char*)"recoverable error while closing file",
  (char*)"attempt to access a closed file",
  (char*)"invalid record id",
  (char*)"invalid or uninitialized record",
  (char*)"invalid page",
  (char*)"recoverable error while inserting record",
  (char*)"recoverable error while forcing pages to disk",
  (char*)"attempt to access a unopened scanner",
  (char*)"location out of page bounds",
  (char*)"attempt to insert a null record",
  (char*)"failure while opening scan, perhaps an invalid parameter",
  (char*)"file name invalid",
  (char*)"end of file reached"
};

static char *RM_ErrorMsg[] = {
  (char*)"fatal error during file creation",
  (char*)"fatal error during file deletion",
  (char*)"fatal error while opening file",
  (char*)"fatal error while closing file",
  (char*)"fatal error in rm filehandle, probably coming from pf",
  (char*)"fatal error in rm scanner, probably coming from pf"
};

// Sends a message to cerr which corresponds to an error code
void RM_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_RM_WARN && rc <= RM_LASTWARN)
    // Print warning
    cerr << "RM warning: " << RM_WarnMsg[rc - START_RM_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_RM_ERR && -rc <= -RM_LASTERROR)
    // Print error
    cerr << "RM error: " << RM_ErrorMsg[-rc + START_RM_ERR] << "\n";
  else if (rc == 0)
    cerr << "RM_PrintError called with return code of 0\n";
  else
    cerr << "RM error: " << rc << " is out of bounds\n";
}