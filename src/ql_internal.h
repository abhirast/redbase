//
// ql_internal.h
//
//   Query Language internal defines


#ifndef QL_INT_H
#define QL_INT_H

enum OpType {
	RM_LEAF = 1,
	IX_LEAF = 2,
	EQ_COND = 3,
	NE_COND = 4,
	RANGE_COND = 5,
	REL_CROSS = -1,
	REL_JOIN = -2
};



/////////////////////////////////////////////////////
// Base Operator Classes
/////////////////////////////////////////////////////
class QL_Op {
public:
	virtual ~QL_Op() {};
    virtual RC Open() = 0;
    virtual RC Next(std::vector<char> &rec) = 0;
    // only the table scans implement this
    virtual RC Next(std::vector<char> &rec, RID &rid) {return QL_EOF;}
    virtual RC Close() = 0; 
    virtual RC Reset() = 0;
    std::vector<DataAttrInfo> attributes;
    OpType opType;
    std::stringstream desc;
};



class QL_UnaryOp : public QL_Op {
public:
    QL_Op* child;
};


class QL_BinaryOp : public QL_Op {
public:
    QL_Op* lchild;
    QL_Op* rchild;
};

/////////////////////////////////////////////////////
// Scan Operators - Leaf operators
/////////////////////////////////////////////////////

class QL_FileScan: public QL_UnaryOp {
public:
	QL_FileScan(RM_Manager *rmm, const char *relName, int attrIndex, 
		CompOp cmp, void* value, ClientHint hint, 
		const std::vector<DataAttrInfo> &attributes);
	QL_FileScan(RM_Manager *rmm, const char *relName,  
		const std::vector<DataAttrInfo> &attributes);
	~QL_FileScan();
	RC Open();
	RC Next(std::vector<char> &rec);
	RC Next(std::vector<char> &rec, RID &rid);
	RC Reset();
	RC Close();
private:
	std::string relName;
	RM_Manager *rmm;
	RM_FileHandle fh;
	AttrType type;
	int len;
	int offset;
	CompOp cmp;
	void *value;
	ClientHint hint;
	RM_FileScan fs;
	bool isOpen;
};


class QL_IndexScan: public QL_UnaryOp {
public:
	QL_IndexScan(RM_Manager *rmm, IX_Manager *ixm, const char *relName, 
		int attrIndex, CompOp cmp, void* value, ClientHint hint, 
		const std::vector<DataAttrInfo> &attributes);
	~QL_IndexScan();
	RC Open();
	RC Next(std::vector<char> &rec);
	RC Next(std::vector<char> &rec, RID &rid);
	RC Reset();
	RC Close();
private:
	std::string relName;
	int indexNo;
	RM_Manager *rmm;
	IX_Manager *ixm;
	RM_FileHandle fh;
	IX_IndexHandle ih;
	IX_IndexScan is;
	CompOp cmp;
	void *value;
	ClientHint hint;
	bool isOpen;
};

/////////////////////////////////////////////////////
// Conditional select operator
/////////////////////////////////////////////////////

class QL_Condition: public QL_UnaryOp {
public:
	QL_Condition(QL_Op &child, const Condition *cond,
		const std::vector<DataAttrInfo> &attributes);
	~QL_Condition();
	RC Open();
	RC Next(std::vector<char> &rec);
	RC Reset();
	RC Close();
private:
	bool isOpen;
	const Condition* cond;
};


/////////////////////////////////////////////////////
// Cross product operator
/////////////////////////////////////////////////////

class QL_Cross: public QL_BinaryOp {
public:
	QL_Cross(QL_Op &left, QL_Op &right);
	~QL_Cross();
	RC Open();
	RC Next(std::vector<char> &rec);
	RC Reset();
	RC Close();
private:
	bool isOpen;
	std::vector<char> leftrec;
	std::vector<char> rightrec;	// for preventing repeated heap allocs
	bool leftValid;
	int leftRecSize;
};

/////////////////////////////////////////////////////
// Function for printing operator tree
/////////////////////////////////////////////////////

void printOperatorTree(QL_Op* root, int tabs);
#endif