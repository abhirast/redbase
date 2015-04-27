#include <cerrno>
#include <cstdio>
#include <iostream>
#include "ix.h"

using namespace std;

//
// Error table
//
static char *IX_WarnMsg[] = {
  (char*)"recoverable error while creating index",
  (char*)"invalid create index parameter(s)",
  (char*)"recoverable error while destroying index",
  (char*)"recoverable error while opening index",
  (char*)"recoverable error while closing index",
  (char*)"null filename passed",
  (char*)"recoverable error while splitting leaf",
  (char*)"recoverable error while splitting internal node",
  (char*)"recoverable error while squeezing leaf page",
  (char*)"recoverable error while overflow page creation",
  (char*)"recoverable error while inserting in leaf",
  (char*)"recoverable error while inserting in overflow page",
  (char*)"recoverable error while inserting in internal or leaf node",
  (char*)"invalid page type",
  (char*)"recoverable error while inserting",
  (char*)"invalid insert parameter",
  (char*)"index is closed/never opened",
  (char*)"recoverable error while opening scan",
  (char*)"failed to open scan",
  (char*)"invalid scan operator",
  (char*)"recoverable error in scanner",
  (char*)"end of file reached",
  (char*)"scan is closed/never opened",
  (char*)"recoverable error while forcing pages",
  (char*)"recoverable error while deleting a key",
  (char*)"error during deletion in an internal or leaf node",
  (char*)"error during deletion from leaf node",
  (char*)"error during deletion from overflow page",
  (char*)"record not found",
  (char*)"null key passed while deletion",
  (char*)"attempting duplicate insert, aborted"
};

static char *IX_ErrorMsg[] = {
  (char*)"fatal error during index creation",
  (char*)"fatal error during index destruction",
  (char*)"fatal error while opening index",
  (char*)"fatal error while closing index",
  (char*)"fatal error during leaf splitting",
  (char*)"fatal error during internal node splitting",
  (char*)"fatal error during leaf squeezing",
  (char*)"fatal error during overflow page creation",
  (char*)"fatal error during insertion into leaf",
  (char*)"fatal error during insertion into overflow page",
  (char*)"fatal error during insertion into internal/leaf node",
  (char*)"fatal error during insertion of a key",
  (char*)"fatal error while opening scanner",
  (char*)"fatal error during scan",
  (char*)"fatal error during forcing pages to disk",
  (char*)"fatal error while deleting from index",
  (char*)"fatal error while deleting from internal/leaf node",
  (char*)"fatal error while deleting from leaf",
  (char*)"fatal error while deleting from overflow page"
};

// Sends a message to cerr which corresponds to an error code
void IX_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_IX_WARN && rc <= IX_LASTWARN)
    // Print warning
    cerr << "IX warning: " << IX_WarnMsg[rc - START_IX_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_IX_ERR && -rc <= -IX_LASTERROR)
    // Print error
    cerr << "IX error: " << IX_ErrorMsg[-rc + START_IX_ERR] << "\n";
  else if (rc == 0)
    cerr << "IX_PrintError called with return code of 0\n";
  else
    cerr << "IX error: " << rc << " is out of bounds\n";
}