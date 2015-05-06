#include <cstdio>
#include <iostream>
#include <cstring>
#include <cmath>
#include "ix.h"
#include "ix_internal.h"

using namespace std;

/* macro for writing comparison operators
    This occurs in the body of the function having signature
    bool IX_IndexScan::xx_op(void* attr)
    where xx = eq, lt, gt etc.
*/
#define IX_indexscan_operator(expr) do { \
    char attr_value[fHdr.attrLength+1];\
    buffer(attr, attr_value);\
    switch (fHdr.attrType) { \
        case INT:\
            return *((int*) attr_value) expr *((int*) query_value);\
        case FLOAT: \
            return *((float*) attr_value) expr *((float*) query_value);\
        case STRING: \
            return string(attr_value) expr string(query_value);\
        default: return 1 == 1;\
    }\
}while(0)

IX_IndexScan::IX_IndexScan() {
    bIsOpen = 0;
    leaf_index = 0;
}
IX_IndexScan::~IX_IndexScan() {
    if (bIsOpen) {
        delete[] query_value;
    }
}

// Open index scan
RC IX_IndexScan::OpenScan(const IX_IndexHandle &indexHandle,
    CompOp compOp, void *value, ClientHint  pinHint) {
    RC WARN = IX_OPEN_SCAN_WARN, ERR = IX_OPEN_SCAN_ERR;
    // Set all the private members to initialize the scan
    if (indexHandle.bIsOpen == 0) return IX_INDEX_CLOSED;
    // check if the scan is already open
    if (bIsOpen) return IX_SCAN_OPEN_FAIL;
    // check if the compOp is valid
    if (compOp < NO_OP || compOp > GE_OP || compOp == NE_OP) 
        return IX_SCAN_INVALID_OPERATOR;
    if (!value) compOp = NO_OP;
    comp_op = compOp;
    ix_ih = &indexHandle;
    pf_fh = &(indexHandle.pf_fh);
    fHdr = indexHandle.fHdr;
    query_value = new char[fHdr.attrLength + 1];
    if (value) buffer(value, query_value);
    pin_hint = pinHint;
    switch (compOp) {
        case NO_OP:
            comp = &IX_IndexScan::no_op;
            break;
        case EQ_OP:
            comp = &IX_IndexScan::eq_op;
            break;
        case LT_OP:
            comp = &IX_IndexScan::lt_op;
            break;
        case GT_OP:
            comp = &IX_IndexScan::gt_op;
            break;
        case LE_OP:
            comp = &IX_IndexScan::le_op;
            break;
        case GE_OP:
            comp = &IX_IndexScan::ge_op;
            break;
        default:
            return IX_SCAN_INVALID_OPERATOR;
            break;
    }
    // seek to the first leaf page
    int pnum = fHdr.root_pnum;
    if (pnum == IX_SENTINEL) return IX_EOF;
    PF_PageHandle ph;
    char *data;

    do {
        IX_ErrorForward(pf_fh->GetThisPage(pnum, ph));
        IX_ErrorForward(ph.GetData(data));
        IX_InternalHdr *pHdr = (IX_InternalHdr*) data;
        if (pHdr->type == LEAF) break;
        // update pnum according to the given operator
        int next_page;
        if (compOp == NO_OP || compOp == LE_OP || compOp == LT_OP) {
            // go to the leftmost page
            next_page = pHdr->left_pnum;
        } else {
            int index, num_keys = pHdr->num_keys;
            char *keys = data + sizeof(IX_InternalHdr);
            char *pointers = keys + fHdr.attrLength * fHdr.internal_capacity;
            // find the appropriate key in the page
            bool match = ix_ih->findKey(keys, value, pHdr->num_keys, index);
            // call the function on the appropriate page
            if (match) {
                // the pointer index is correct
                memcpy(&next_page, pointers + index * sizeof(PageNum), sizeof(PageNum));
            } else if (index == 0) {
                next_page = pHdr->left_pnum;
            } else {
                index--;
                memcpy(&next_page, pointers + index * sizeof(PageNum), sizeof(PageNum));
            }
        }
        // unpin the page
        IX_ErrorForward(pf_fh->UnpinPage(pnum));
        pnum = next_page;
    } while (pnum != IX_SENTINEL);
    // initialize the indices
    current_leaf = pnum;
    // check if a viable key exists in the leaf page
    IX_LeafHdr* pHdr = (IX_LeafHdr*) data;
    next_leaf = pHdr->right_pnum;
    char* keys = data + sizeof(IX_LeafHdr);
    char *pointers = keys + fHdr.attrLength * fHdr.leaf_capacity;
    found = false;
    for (int i = 0; i < pHdr->num_keys; i++) {
        if ((this->*comp)(keys + i * fHdr.attrLength)) {
            found = true;
            leaf_index = i;
            break;
        }
    }
    if (!found && comp_op == GT_OP) {
        found = true;
        leaf_index = 0;
        current_leaf = next_leaf;
    }
    // Get the pointer corresponding to this key to see if we will go 
    // to an overflow page
    RID *rid = (RID*) (pointers + leaf_index * sizeof(RID));
    int page = -1;
    int slot = -1;
    IX_ErrorForward(rid->GetPageNum(page));
    IX_ErrorForward(rid->GetSlotNum(slot));
    if (slot < 0) {
        onOverflow = true;
        current_overflow = page;
        overflow_index = 0;
    } else {
        onOverflow = false;
        current_overflow = IX_SENTINEL;
    }
    // unpin the page
    if (pnum != IX_SENTINEL) IX_ErrorForward(pf_fh->UnpinPage(pnum));
    bIsOpen = 1;
    // set last deleted to invalid rec id
    RID temp(-1,-1);
    (const_cast<IX_IndexHandle*>(ix_ih))->last_deleted = temp;
    return OK_RC;
}

// Get the next matching entry return IX_EOF if no more matching
// entries.
RC IX_IndexScan::GetNextEntry(RID &rid) {
    RC WARN = IX_SCAN_WARN, ERR = IX_SCAN_ERR;
    if (!bIsOpen) return IX_SCAN_CLOSED;
    if (!(ix_ih->bIsOpen)) return IX_SCAN_CLOSED;
    if (!found) return IX_EOF;
    PF_PageHandle ph;
    // if currently on an overflow page
    if (onOverflow) {
        if (last_emitted == ix_ih->last_deleted && overflow_index > 0) {
            overflow_index--;
        }
        // get the overflow page
        IX_ErrorForward(pf_fh->GetThisPage(current_overflow, ph));
        int to_unpin = current_overflow;
        char *data;
        IX_ErrorForward(ph.GetData(data));
        IX_OverflowHdr* pHdr = (IX_OverflowHdr*) data;
        char* rids = data + sizeof(IX_OverflowHdr);
        memcpy(&rid, rids + overflow_index * sizeof(RID), sizeof(RID));
        overflow_index++;
        // if finished the overflow page
        if (overflow_index == pHdr->num_rids) {
            // if no new overflow page
            if (pHdr->next_page == IX_SENTINEL) {
                leaf_index++;
                // if this was the last element on the leaf
                if (leaf_index == fHdr.leaf_capacity) {
                    leaf_index = 0;
                    current_leaf = next_leaf;
                }
            }
            // go to the next overflow page
            else {
                overflow_index = 0;
                current_overflow = pHdr->next_page;
                onOverflow = true;
            }
        }
        // unpin the current page
        IX_ErrorForward(pf_fh->UnpinPage(to_unpin));
        last_emitted = rid;
        return OK_RC;
    }
    // if not on overflow
    else {
        if (last_emitted == ix_ih->last_deleted && leaf_index > 0) {
            leaf_index--;
        }
        // get the current leaf page
        IX_ErrorForward(pf_fh->GetThisPage(current_leaf, ph));
        int to_unpin = current_leaf;
        char *data;
        IX_ErrorForward(ph.GetData(data));
        IX_LeafHdr* pHdr = (IX_LeafHdr*) data;
        next_leaf = pHdr->right_pnum;
        char* keys = data + sizeof(IX_LeafHdr);
        char* rids = keys + fHdr.attrLength * fHdr.leaf_capacity;
        if (!(this->*comp)(keys + leaf_index * fHdr.attrLength)) {
            // unpin page and raise eof
            IX_ErrorForward(pf_fh->UnpinPage(to_unpin));
            return IX_EOF;
        }
        // get the rid
        memcpy(&rid, rids + leaf_index * sizeof(RID), sizeof(RID));
        int slot;
        IX_ErrorForward(rid.GetSlotNum(slot));
        // if the leaf entry denotes an overflow page
        if (slot < 0) {
            // set in slot mode, unpin page and call again
            int page;
            IX_ErrorForward(rid.GetPageNum(page));
            current_overflow = page;
            overflow_index = 0;
            onOverflow = true;
            IX_ErrorForward(pf_fh->UnpinPage(to_unpin));
            IX_ErrorForward(GetNextEntry(rid));
            last_emitted = rid;
            return OK_RC;
        } 
        leaf_index++;
        // if leaf finished
        if (leaf_index == pHdr->num_keys) {
            if (next_leaf == IX_SENTINEL) {
                // unpin page, end of scan
                // IX_ErrorForward(pf_fh->UnpinPage(to_unpin));
                found = false;
            }
            if (comp_op == EQ_OP) found = false;
            current_leaf = next_leaf;
            leaf_index = 0;
            onOverflow = false;
        }
        IX_ErrorForward(pf_fh->UnpinPage(to_unpin));
        last_emitted = rid;
        return OK_RC;
    }

    return WARN; //should not reach here
}

// Close index scan
RC IX_IndexScan::CloseScan() {
    if (!bIsOpen) return IX_SCAN_CLOSED;
    bIsOpen = 0;
    delete[] query_value;
    return OK_RC;
}

// copies the contents into char array and null terminates it
void IX_IndexScan::buffer(void *ptr, char* buff) {
    buff[fHdr.attrLength] = '\0';
    memcpy(buff, ptr, fHdr.attrLength);
    return;
}

// operators for comparison
bool IX_IndexScan::no_op(void* attr) {
    return true;
}
bool IX_IndexScan::eq_op(void* attr) {
    IX_indexscan_operator(==);
}
bool IX_IndexScan::lt_op(void* attr) {
    IX_indexscan_operator(<);
}
bool IX_IndexScan::gt_op(void* attr) {
    IX_indexscan_operator(>);
}
bool IX_IndexScan::le_op(void* attr) {
    IX_indexscan_operator(<=);
}
bool IX_IndexScan::ge_op(void* attr) {
    IX_indexscan_operator(>=);
}