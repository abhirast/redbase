//
// ix_internal.h
//
//   Index Manager internal defines

#ifndef IX_INT_H
#define IX_INT_H

enum IPageType {
	INTERNAL,
	LEAF
};

struct IX_InternalHdr {
	IPageType type;
	int num_keys;
	int left_pnum;
};

struct IX_LeafHdr {
	IPageType type;
	int num_keys;
	int left_pnum;
	int right_pnum;
};

struct IX_OverflowHdr {
	int num_rids;
	int next_page;
};

#define IX_SENTINEL -1


// Macro for error forwarding
// WARN and ERR to be defined in the context where macro is used
#define IX_ErrorForward(expr) do { \
RC tmp_rc = (expr);\
if (tmp_rc != OK_RC) \
		return ((tmp_rc > 0) ? WARN : ERR); \
} while (0)




#endif