//
// ix_internal.h
//
//   Index Manager internal defines

#ifndef IX_INT_H
#define IX_INT_H




// Macro for error forwarding
// WARN and ERR to be defined in the context where macro is used
#define IX_ErrorForward(expr) do { \
RC tmp_rc = (expr);\
if (tmp_rc != OK_RC) \
        return ((tmp_rc > 0) ? WARN : ERR); \
} while (0)



#endif