//
// rm_internal.h
//
//   Record Manager internal defines

#ifndef RM_INT_H
#define RM_INT_H

//
// Structures for page header 
//

struct RM_PageHdr {
    int next_free;      // Page number of next free page
    int num_recs;       // Number of valid records in page
};



// Macro for error forwarding
// WARN and ERR to be defined in the context where macro is used
#define RM_ErrorForward(expr) do { \
RC tmp_rc = (expr);\
if (tmp_rc != OK_RC) \
        return ((tmp_rc > 0) ? WARN : ERR); \
} while (0)



// Sentinel value for free page linked list
#define RM_SENTINEL -1

#endif