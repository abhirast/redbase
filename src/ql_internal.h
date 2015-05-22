//
// ql_internal.h
//
//   Query Language internal defines


#ifndef QL_INT_H
#define QL_INT_H

enum OpType {
	RM_LEAF,
	IX_LEAF,
	EQ_COND,
	NE_COND,
	RANGE_COND,
	REL_CROSS,
	REL_JOIN
};



/////////////////////////////////////////////////////
// Base Operator Classes
/////////////////////////////////////////////////////
class QL_Op {
public:
	virtual ~QL_Op() {};
    std::vector<DataAttrInfo> attributes;
    virtual RC Open() = 0;
    virtual RC Next(std::vector<char> &rec) = 0;
    // only the table scans implement this
    virtual RC Next(std::vector<char> &rec, RID &rid) {return QL_EOF;}
    virtual RC Close() = 0; 
    virtual RC Reset() = 0;
    OpType opType;
};



class QL_UnaryOp : public QL_Op {
public:
    std::shared_ptr<QL_Op> child;
};


class QL_BinaryOp : public QL_Op {
public:
    std::shared_ptr<QL_Op> lchild;
    std::shared_ptr<QL_Op> rchild;
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
	QL_Condition(QL_Op &child, Condition cond,
		const std::vector<DataAttrInfo> &attributes);
	~QL_Condition();
	RC Open();
	RC Next(std::vector<char> &rec);
	RC Reset();
	RC Close();
private:
	bool isOpen;
	Condition cond;
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

#endif