#include <sys/times.h>
#include <sys/types.h>
#include <cassert>
#include <unistd.h>
#include <memory>
#include "redbase.h"
#include "ql.h"
#include "sm.h"
#include "ix.h"
#include "rm.h"
#include "parser.h"
#include "ql_internal.h"

using namespace std;

////////////////////////////////////////////////////////
//	QL_FileScan
////////////////////////////////////////////////////////

QL_FileScan::QL_FileScan(RM_Manager *rmm, const char *relName, int attrIndex, 
		CompOp cmp, void* value, ClientHint hint, 
		const std::vector<DataAttrInfo> &attributes) {
	this->relName.assign(relName);
	this->rmm = rmm;
	this->type = attributes[attrIndex].attrType;
	this->len = attributes[attrIndex].attrLength;
	this->offset = attributes[attrIndex].offset;
	this->cmp = cmp;
	this->value = value;
	this->hint = hint;
	this->attributes = attributes;
	isOpen = false;
	child = 0;
}

QL_FileScan::QL_FileScan(RM_Manager *rmm, const char *relName,
		const std::vector<DataAttrInfo> &attributes) {
	this->relName.assign(relName);
	this->rmm = rmm;
	this->type = INT;
	this->len = 4;
	this->offset = 0;
	this->cmp = NO_OP;
	this->value = 0;
	this->hint = NO_HINT;
	this->attributes = attributes;
	isOpen = false;
	child = 0;
}

QL_FileScan::~QL_FileScan() {
}

RC QL_FileScan::Open() {
	RC WARN = QL_FILESCAN_WARN, ERR = QL_FILESCAN_ERR;
	if (isOpen) return WARN;
	QL_ErrorForward(rmm->OpenFile(relName.c_str(), fh));
	QL_ErrorForward(fs.OpenScan(fh, type, len,
            offset, cmp, value, hint));
	isOpen = true;
	return OK_RC;
}

RC QL_FileScan::Next(shared_ptr<char> &rec) {
	RC WARN = QL_FILESCAN_WARN, ERR = QL_FILESCAN_ERR;
	if (!isOpen) return WARN;
	RM_Record record;
	char *temp;
	QL_ErrorForward(fs.GetNextRec(record));
	QL_ErrorForward(record.GetData(temp));
	memcpy(rec.get(), temp, attributes.back().offset + attributes.back().attrLength);
	return OK_RC;
}

RC QL_FileScan::Next(shared_ptr<char> &rec, RID &rid) {
	RC WARN = QL_FILESCAN_WARN, ERR = QL_FILESCAN_ERR;
	if (!isOpen) return WARN;
	RM_Record record;
	char *temp;
	QL_ErrorForward(fs.GetNextRec(record));
	QL_ErrorForward(record.GetData(temp));
	QL_ErrorForward(record.GetRid(rid));
	memcpy(rec.get(), temp, attributes.back().offset + 
		attributes.back().attrLength);
	return OK_RC;
}

RC QL_FileScan::Close() {
	RC WARN = QL_FILESCAN_WARN, ERR = QL_FILESCAN_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(fs.CloseScan());
	QL_ErrorForward(rmm->CloseFile(fh));
	isOpen = false;
	return OK_RC;
}

////////////////////////////////////////////////////////
//	QL_IndexScan
////////////////////////////////////////////////////////

QL_IndexScan::QL_IndexScan(RM_Manager *rmm, IX_Manager *ixm, 
		const char *relName, int attrIndex, CompOp cmp, void* value, 
		ClientHint hint, const std::vector<DataAttrInfo> &attributes) {
	this->relName.assign(relName);
	this->rmm = rmm;
	this->ixm = ixm;
	this->cmp = cmp;
	this->value = value;
	this->hint = hint;
	this->attributes = attributes;
	this->indexNo = attributes[attrIndex].indexNo;
	isOpen = false;
	child = 0;
}

QL_IndexScan::~QL_IndexScan() {
}

RC QL_IndexScan::Open() {
	RC WARN = QL_IXSCAN_WARN, ERR = QL_IXSCAN_ERR;
	if (isOpen) return WARN;
	QL_ErrorForward(rmm->OpenFile(relName.c_str(), fh));
	QL_ErrorForward(ixm->OpenIndex(relName.c_str(), indexNo, ih));
	QL_ErrorForward(is.OpenScan(ih, cmp, value, hint));
	isOpen = true;
	return OK_RC;
}

RC QL_IndexScan::Next(shared_ptr<char> &rec) {
	RC WARN = QL_IXSCAN_WARN, ERR = QL_IXSCAN_ERR;
	if (!isOpen) return WARN;
	RID rid;
	RM_Record record;
	char *temp;
	QL_ErrorForward(is.GetNextEntry(rid));
	QL_ErrorForward(fh.GetRec(rid, record));
	QL_ErrorForward(record.GetData(temp));
	memcpy(rec.get(), temp, attributes.back().offset + 
		attributes.back().attrLength);
	return OK_RC;
}

RC QL_IndexScan::Next(shared_ptr<char> &rec, RID &rid) {
	RC WARN = QL_IXSCAN_WARN, ERR = QL_IXSCAN_ERR;
	if (!isOpen) return WARN;
	RM_Record record;
	char *temp;
	QL_ErrorForward(is.GetNextEntry(rid));
	QL_ErrorForward(fh.GetRec(rid, record));
	QL_ErrorForward(record.GetData(temp));
	memcpy(rec.get(), temp, attributes.back().offset + 
		attributes.back().attrLength);
	return OK_RC;
}

RC QL_IndexScan::Close() {
	RC WARN = QL_IXSCAN_WARN, ERR = QL_IXSCAN_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(is.CloseScan());
	QL_ErrorForward(ixm->CloseIndex(ih));
	QL_ErrorForward(rmm->CloseFile(fh));
	isOpen = false;
	return OK_RC;
}

/////////////////////////////////////////////////////
// Conditional select operator
/////////////////////////////////////////////////////

QL_Condition::QL_Condition(QL_Op &child, Condition cond,
		const std::vector<DataAttrInfo> &attributes) {
	this->attributes = attributes;
	this->child.reset(&child);
	isOpen = false;
}

QL_Condition::~QL_Condition() {

}

RC QL_Condition::Open() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (isOpen) return WARN;
	QL_ErrorForward((child.get())->Open());
	isOpen = true;
	return OK_RC;
}

RC QL_Condition::Next(shared_ptr<char> &rec) {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	do {
		QL_ErrorForward((child.get())->Next(rec));
	} while(!QL_Manager::evalCondition((void*) rec.get(), cond, attributes));
	return OK_RC;
}

RC QL_Condition::Close() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward((child.get())->Close());
	isOpen = false;
	return OK_RC;
}