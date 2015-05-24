#include <sys/times.h>
#include <sys/types.h>
#include <cassert>
#include <unistd.h>
#include <sstream>
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
	parent = 0;
	opType = RM_LEAF;
	desc << "FILE SCAN " << relName << " ON ";
	desc << attributes[attrIndex].attrName;
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
	opType = RM_LEAF;
	desc << "FILE SCAN " << relName;
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

RC QL_FileScan::Next(vector<char> &rec) {
	RC WARN = QL_EOF, ERR = QL_FILESCAN_ERR;
	if (!isOpen) return WARN;
	RM_Record record;
	char *temp;
	rec.resize(attributes.back().offset + attributes.back().attrLength, 0);
	QL_ErrorForward(fs.GetNextRec(record));
	QL_ErrorForward(record.GetData(temp));
	memcpy(&rec[0], temp, rec.size());
	return OK_RC;
}

RC QL_FileScan::Next(vector<char> &rec, RID &rid) {
	RC WARN = QL_EOF, ERR = QL_FILESCAN_ERR;
	if (!isOpen) return WARN;
	RM_Record record;
	char *temp;
	rec.resize(attributes.back().offset + attributes.back().attrLength, 0);
	QL_ErrorForward(fs.GetNextRec(record));
	QL_ErrorForward(record.GetData(temp));
	QL_ErrorForward(record.GetRid(rid));
	memcpy(&rec[0], temp, rec.size());
	return OK_RC;
}

RC QL_FileScan::Reset() {
	RC WARN = QL_FILESCAN_WARN, ERR = QL_FILESCAN_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(fs.CloseScan());
	QL_ErrorForward(fs.OpenScan(fh, type, len,
            offset, cmp, value, hint));
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
	parent = 0;
	opType = IX_LEAF;
	desc << "INDEX SCAN " << relName << " ON ";
	desc << attributes[attrIndex].attrName;
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

RC QL_IndexScan::Next(vector<char> &rec) {
	RC WARN = QL_EOF, ERR = QL_IXSCAN_ERR;
	if (!isOpen) return WARN;
	RID rid;
	RM_Record record;
	char *temp;
	rec.resize(attributes.back().offset + attributes.back().attrLength, 0);
	QL_ErrorForward(is.GetNextEntry(rid));
	QL_ErrorForward(fh.GetRec(rid, record));
	QL_ErrorForward(record.GetData(temp));
	memcpy(&rec[0], temp, attributes.back().offset + 
		attributes.back().attrLength);
	return OK_RC;
}

RC QL_IndexScan::Next(vector<char> &rec, RID &rid) {
	RC WARN = QL_EOF, ERR = QL_IXSCAN_ERR;
	if (!isOpen) return WARN;
	RM_Record record;
	char *temp;
	rec.resize(attributes.back().offset + attributes.back().attrLength, 0);
	QL_ErrorForward(is.GetNextEntry(rid));
	QL_ErrorForward(fh.GetRec(rid, record));
	QL_ErrorForward(record.GetData(temp));
	memcpy(&rec[0], temp, attributes.back().offset + 
		attributes.back().attrLength);
	return OK_RC;
}

RC QL_IndexScan::Reset() {
	RC WARN = QL_IXSCAN_WARN, ERR = QL_IXSCAN_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(is.CloseScan());
	QL_ErrorForward(is.OpenScan(ih, cmp, value, hint));
	return OK_RC;
}

RC QL_IndexScan::Reset(void *value) {
	RC WARN = QL_IXSCAN_WARN, ERR = QL_IXSCAN_ERR;
	if (!isOpen) return WARN;
	this->value = value;
	QL_ErrorForward(is.CloseScan());
	QL_ErrorForward(is.OpenScan(ih, cmp, value, hint));
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

QL_Condition::QL_Condition(QL_Op &child, const Condition *cond,
		const std::vector<DataAttrInfo> &attributes) {
	this->attributes = attributes;
	this->child = &child;
	child.parent = this;
	this->cond = cond;
	isOpen = false;
	// change indexNo of all attributes
	for (unsigned int i = 0; i < this->attributes.size(); i++) {
		this->attributes[i].indexNo = -1;
	}
	opType = COND;
	desc << "FILTER BY " << cond->lhsAttr << cond->op;
	if (cond->bRhsIsAttr) {
		desc << cond->rhsAttr;
	} else {
		desc << cond->rhsValue;
	}
}

QL_Condition::~QL_Condition() {
	if (child != 0) delete child;
}

RC QL_Condition::Open() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (isOpen) return WARN;
	QL_ErrorForward(child->Open());
	isOpen = true;
	return OK_RC;
}

RC QL_Condition::Next(vector<char> &rec) {
	RC WARN = QL_EOF, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	rec.resize(attributes.back().offset + attributes.back().attrLength, 0);
	do {
		QL_ErrorForward(child->Next(rec));
	} while(!QL_Manager::evalCondition((void*) &rec[0], *cond, attributes));
	return OK_RC;
}

RC QL_Condition::Reset() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(child->Reset());
	return OK_RC;
}

RC QL_Condition::Close() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(child->Close());
	isOpen = false;
	return OK_RC;
}

CompOp QL_Condition::getOp() {
	return cond->op;
}

/////////////////////////////////////////////////////
// Projection operator
/////////////////////////////////////////////////////

QL_Projection::QL_Projection(QL_Op &child, int nSelAttrs, 
	const RelAttr* selAttrs, const vector<DataAttrInfo> &attributes) {
	this->inputAttr = attributes;
	this->child = &child;
	child.parent = this;
	isOpen = false;
	// construct the output schema, loop through input attrs to see which
	// of them are present in the selAttrs
	for (unsigned int i = 0; i < attributes.size(); i++) {
		for (int j = 0; j < nSelAttrs; j++) {
			if (strcmp(attributes[i].attrName, selAttrs[j].attrName) == 0 &&
			 (selAttrs[j].relName == 0 ||
			  strcmp(attributes[i].relName, selAttrs[j].relName) == 0)) {
				this->attributes.push_back(attributes[i]);
				this->position.push_back(i);
				break;
			}
		}
	}
	// modify the offsets of the output schema
	int cum = 0;
	for (unsigned int i = 0; i < this->attributes.size(); i++) {
		this->attributes[i].offset = cum;
		cum += this->attributes[i].attrLength;
	}
	// change indexNo of all attributes
	for (unsigned int i = 0; i < this->attributes.size(); i++) {
		this->attributes[i].indexNo = -1;
	}
	opType = PROJ;
	desc << "PROJECT ";
	if (this->attributes[0].relName != 0) {
		desc << this->attributes[0].relName << ".";
	}
	desc << (this->attributes[0].attrName);
	for (unsigned int i = 1; i < this->attributes.size(); i++) {
		desc << ", ";
		desc << this->attributes[i].relName << ".";
		desc << this->attributes[i].attrName;
	}
}

QL_Projection::~QL_Projection() {
	if (child != 0) delete child;
}

RC QL_Projection::Open() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (isOpen) return WARN;
	QL_ErrorForward(child->Open());
	isOpen = true;
	return OK_RC;
}

RC QL_Projection::Next(vector<char> &rec) {
	RC WARN = QL_EOF, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	rec.resize(attributes.back().offset + attributes.back().attrLength, 0);
	vector<char> temp(inputAttr.back().offset + inputAttr.back().attrLength);
	QL_ErrorForward(child->Next(temp));
	int st = 0, sz = 0, curr = 0;
	for (unsigned int i = 0; i < attributes.size(); i++) {
		st = inputAttr[this->position[i]].offset;
		sz = inputAttr[this->position[i]].attrLength;
		memcpy(&rec[curr], &temp[st], sz);
		curr += sz;
	}
	return OK_RC;
}

RC QL_Projection::Reset() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(child->Reset());
	return OK_RC;
}

RC QL_Projection::Close() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(child->Close());
	isOpen = false;
	return OK_RC;
}

/////////////////////////////////////////////////////
// Permute/Duplicate operator
/////////////////////////////////////////////////////

QL_PermDup::QL_PermDup(QL_Op &child, int nSelAttrs, 
	const RelAttr* selAttrs, const vector<DataAttrInfo> &attributes) {
	this->inputAttr = attributes;
	this->child = &child;
	child.parent = this;
	isOpen = false;
	// construct the output schema, loop through input attrs to see which
	// of them are present in the selAttrs
	for (int i = 0; i < nSelAttrs; i++) {
		for (unsigned int j = 0; j < attributes.size(); j++) {
			if (strcmp(attributes[j].attrName, selAttrs[i].attrName) == 0 &&
			 (selAttrs[i].relName == 0 ||
			  strcmp(attributes[j].relName, selAttrs[i].relName) == 0)) {
				this->attributes.push_back(attributes[j]);
				this->position.push_back(j);
				break;
			}
		}
	}
	// modify the offsets of the output schema
	int cum = 0;
	for (unsigned int i = 0; i < this->attributes.size(); i++) {
		this->attributes[i].offset = cum;
		cum += this->attributes[i].attrLength;
	}
	// change indexNo of all attributes
	for (unsigned int i = 0; i < this->attributes.size(); i++) {
		this->attributes[i].indexNo = -1;
	}
	opType = PERM_DUP;
	desc << "PERMUTE/DUPLICTE ATTRIBUTES";
}

QL_PermDup::~QL_PermDup() {
	if (child != 0) delete child;
}

RC QL_PermDup::Open() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (isOpen) return WARN;
	QL_ErrorForward(child->Open());
	isOpen = true;
	return OK_RC;
}

RC QL_PermDup::Next(vector<char> &rec) {
	RC WARN = QL_EOF, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	rec.resize(attributes.back().offset + attributes.back().attrLength, 0);
	vector<char> temp(inputAttr.back().offset + inputAttr.back().attrLength);
	QL_ErrorForward(child->Next(temp));
	int st = 0, sz = 0, curr = 0;
	for (unsigned int i = 0; i < attributes.size(); i++) {
		st = inputAttr[this->position[i]].offset;
		sz = inputAttr[this->position[i]].attrLength;
		memcpy(&rec[curr], &temp[st], sz);
		curr += sz;
	}
	return OK_RC;
}

RC QL_PermDup::Reset() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(child->Reset());
	return OK_RC;
}

RC QL_PermDup::Close() {
	RC WARN = QL_COND_WARN, ERR = QL_COND_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(child->Close());
	isOpen = false;
	return OK_RC;
}

/////////////////////////////////////////////////////
// Cross product operator
/////////////////////////////////////////////////////

QL_Cross::QL_Cross(QL_Op &left, QL_Op &right) {
	this->lchild = &left;
	this->rchild = &right;
	left.parent = this;
	right.parent = this;
	isOpen = false;
	leftValid = false;
	attributes = lchild->attributes;
	auto temp = rchild->attributes;
	// change offset of rchild attributes
	leftRecSize = attributes.back().offset + attributes.back().attrLength;
	for (unsigned int i = 0; i < temp.size(); i++) {
		temp[i].offset += leftRecSize;
	}
	attributes.insert(attributes.end(), temp.begin(), temp.end());
	// change indexNo of all attributes
	for (unsigned int i = 0; i < attributes.size(); i++) {
		attributes[i].indexNo = -1;
	}
	opType = REL_CROSS;
	desc << "CROSS PROD";
}

QL_Cross::~QL_Cross() {
	if (lchild != 0) delete lchild;
	if (rchild != 0) delete rchild;
}

RC QL_Cross::Open() {
	RC WARN = QL_CROSS_WARN, ERR = QL_CROSS_ERR;
	if (isOpen) return WARN;
	QL_ErrorForward(lchild->Open());
	QL_ErrorForward(rchild->Open());
	isOpen = true;
	leftValid = false;
	return OK_RC;
}

RC QL_Cross::Next(vector<char> &rec) {
	RC WARN = QL_EOF, ERR = QL_CROSS_ERR;
	if (!leftValid) {
		QL_ErrorForward(lchild->Next(leftrec));
		leftValid = true;
	}
	RC rc = rchild->Next(rightrec);
	if (rc == QL_EOF) {
		QL_ErrorForward(rchild->Reset());
		QL_ErrorForward(rchild->Next(rightrec));
		QL_ErrorForward(lchild->Next(leftrec));
	} else if (rc != OK_RC) {
		return rc;
	}
	rec.resize(leftrec.size() + rightrec.size(), 0);
	memcpy(&rec[0], &leftrec[0], leftrec.size());
	memcpy(&rec[leftrec.size()], &rightrec[0], rightrec.size());
	return OK_RC;
}

RC QL_Cross::Reset() {
	RC WARN = QL_CROSS_WARN, ERR = QL_CROSS_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(lchild->Reset());
	QL_ErrorForward(rchild->Reset());
	leftValid = false;
	return OK_RC;
}

RC QL_Cross::Close() {
	RC WARN = QL_CROSS_WARN, ERR = QL_CROSS_ERR;
	if (!isOpen) return WARN;
	QL_ErrorForward(lchild->Close());
	QL_ErrorForward(rchild->Close());
	return OK_RC;
}


/////////////////////////////////////////////////////
// Print Operator Tree
/////////////////////////////////////////////////////

void printOperatorTree(QL_Op* root, int tabs) {
	if (root == 0) return;
	for (int i = 0; i < tabs; i++) cout<<"  ";
	if (root->opType > 0) {
		// unary operator
		auto temp = (QL_UnaryOp*) root;
		cout << root->desc.str() << endl;
		printOperatorTree(temp->child, tabs);
	} else {
		// binary operator
		auto temp = (QL_BinaryOp*) root;
		cout << root->desc.str() << " { " << endl;
		printOperatorTree(temp->lchild, tabs+1);
		for (int i = 0; i < tabs + 1; i++) cout<<"  ";
		cout << "," << endl;
		printOperatorTree(temp->rchild, tabs+1);
		for (int i = 0; i < tabs; i++) cout<<"  ";
		cout << " } " << endl;
	}
}