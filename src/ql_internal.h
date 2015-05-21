//
// ql_internal.h
//
//   Query Language internal defines


#ifndef QL_INT_H
#define QL_INT_H

class QL_Op {
public:
	virtual ~QL_Op() {};
    std::vector<DataAttrInfo> attributes;
    virtual RC Open() = 0;
    virtual RC Next(std::shared_ptr<char> &rec) = 0;
    // only the table scans implement this
    virtual RC Next(std::shared_ptr<char> &rec, RID &rid) {return QL_EOF;}
    virtual RC Close() = 0; 
};



class QL_UnaryOp : public QL_Op {
protected:
    QL_Op* child;
};


class QL_BinaryOp : public QL_Op {
public:
    QL_BinaryOp(QL_Op &left, QL_Op &right) {
        lchild = &left;
        rchild = &right;
    }
protected:
    QL_Op* lchild;
    QL_Op* rchild;
};

class QL_IndexScan: public QL_UnaryOp {
	QL_IndexScan();
};

class QL_FileScan: public QL_UnaryOp {
public:
	QL_FileScan(RM_FileHandle &fh, AttrType type, int len, int offset, 
		CompOp cmp, void* value, ClientHint hint, 
		const std::vector<DataAttrInfo> &attributes);
	~QL_FileScan();
	RC Open();
	RC Next(std::shared_ptr<char> &rec);
	RC Next(std::shared_ptr<char> &rec, RID &rid);
	RC Close();
private:
	RM_FileHandle *fh;
	AttrType type;
	int len;
	int offset;
	CompOp cmp;
	void *value;
	ClientHint hint;
	RM_FileScan fs;
	bool isOpen;
};


#endif