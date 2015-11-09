# ex_DOC #

#### Overview ####
This part primarily focuses on query optimization using an external sort operator. The external sort operator has been designed by keeping in mind that it can be used by as many database operations as possible. The sorting operator provides functionality to build a sorted file with a given fill factor. Further, clients can specify whether to keep the sorted file or delete it after iterating through it. 

A different file scanner has also been implemented for sorted files which allows directional scans (i.e, both ascending and descending order) starting from a specific page of the sorted file. The scanner also allows page wise scans. Using the sorting operator, merge join algorithm has been implemented. Some operator tree optimization methods have also been written to make query execution more efficient.

#### External Sorting ####
Quick-sort without an  initial random shuffle has been used for external sorting since it is a in-place algorithm. The external sorting algorithm is as follows-

1. Determine the number of buffer pages available for sorting. This is the total number of buffer pages minus the number of relations present as the child of the operator which has to be sorted.
2. Read tuples from the relation and put them in the available buffer pages.
3. Sort each buffer page using quick sort. 
4. Merge the sorted buffer pages and write the result to a chunk file on the disk with a fill factor of 1. 
5. Repeat steps 2 to 4 till the relation gets exhausted.
6. While the number of chunks in the queue is greater than 1, do steps 7 to 9.
7. Load the first page of first B-1 relations from the queue in the buffer pages, where B is the total number of buffer pages. Merge these pages and write the result to another sorted file. If the number of chunks in the queue is greater than B then a fill factor of 1 is used, otherwise the new chunk is created with the specified fill factor.
8. If a page gets exhausted during the merge, another page is read from the same sorted chunk if available. 
9. Delete the chunk files after the new chunk has been created. Put the new chunk in the queue.
10. Rename the last chunk to the file name given by the client. This is the output sorted relation

#### Sort Merge Join ####
Sort Merge join is implemented by a combination of two unary sorting operators and a binary merge operator. The merge operator assumes that the tuple stream coming from its children are sorted in ascending order in the join attribute. This separation of operators allows us to do more efficient query optimization, details of which are given in the next section.
For the merge operator, the bottleneck occurs when successive tuples in the stream arriving from one of its children (say right) have the same join key. Consider the case given below, where the join key in the tuple stream from left and right child has been listed. 
~~~~
left:  1 2 2 3 4 
right: 0 2 2 2 2 2 2 2 2 5 
~~~~
As soon as we encounter the second occurrence of 2 in the left stream, we need to "go back" in the right stream to retrieve the tuples. Since the operators have been implemented as forward iterators, this is not possible. So in this case, pages from the buffer pool have been used to store the duplicate tuples in the right stream. If the same join key is encountered in the next tuple of the left stream, the tuple is joined with all the tuples in the buffer. A class BufferIterator has been implemented to easily carry out this job, which converts the buffer pool into a random access iterator over the tuples.

#### Query Optimization ####
The query tree optimization in the QL part started with a left leaning operator tree consisting of file scan as leaves and cross product operators as internal nodes. After that a condition node corresponding to each condition was places above the root node, then a projection operator followed by a permute/duplicate operator. Pushing the conditions followed by pushing the projections down the tree achieved substantial gain in performance. In this part, three optimization functions have been implemented. The current optimization sequence, of which, the first two steps were implemented in QL part, is as follows-
(i) Push Conditions (ii) Push Projections (iii) Push Conditions (iv) Merge Projections (v) Introduce Sort Merge join (vi) Push Sorts

1. Merging Projections - After projections have been pushed down completely, many new projection operators get formed. These projection operators don't affect disk access but they cause redundant data movement. To avoid this, after pushing projections down, conditions are pushed down again so that the projections surface up. Then if successive projections occur together, they are merged.

2. Doing Sort-Merge Join - Pushing conditions down for a second time also ensures that the join conditions will lie right above the corresponding cross product operator. Further the attributes on both sides of the join condition must belong to different children of the cross product operator. Since we don't have database statistics, we make use of simple heuristics to determine if a relation is worth sorting. A recursive procedure has been implemented for the same with the heuristic - if there is any equality based condition lying on the downward path from the node whose output is to be sorted till a binary operator is seen or the end of the path, the node is not worth sorting. If both the children of the cross product operator are worth sorting then a sort operator is inserted between each child and the cross product operator and the cross product operator is converted into the merge operator.

3. Pushing down Sort - In the current implementation, when a sort operator has a RM scan operator as its child, the sorted file is kept around till an insert or delete is done. Any subsequent query which requires sorting based on the same attribute, reads from a sorted file if it exists, otherwise creates a new one. In this case, pushing Sort down is highly desirable so as to make use of the sorted file if it exists or to make one so that it can be used by future queries. Further, range scans on the sorted files are much more efficient than IX scans and more efficient than RM scan if a sorted file already exists. Thus, pushing Sort down makes way for the next step in which RM range scans can be replaced by Sort based range scans.

#### Testing ####
I tested my implementation on the tests used by the QL part. I also tested my implementation on the ebay dataset provided in CS 145 and matching the counts of output tuples on sqlite. To test the robustness of sort operator, I tested the sorting operator on yelp academic dataset, which has a relation consisting of about 1.5 million tuples. The merge join has also been tested on the yelp dataset involving join of large relations have about 1.5 million output tuples. Memory checks have also been done using valgrind. 

#### Acknowledgements ####
I would like to thank Jaeho for introducing me to this design in the QL part. The operator tree based implementation helped a lot in coding and debugging this part. I also thank him for valuable inputs during our discussion on design.  



# ql_DOC #

#### Overview ####
This part provides the core database functionality to redbase. Internally, the Query Language (QL) module takes in the inputs from the command line parser and interacts with the other modules - PF, RM, IX and SM to execute the tasks demanded by the user. Planning the execution of a general database query in the most efficient way is quite challenging. In this implementation, many simplifications have been made to restrict the search space of the possible query execution plans.

#### Code structure ####
An iterator approach has been used in this implementation. Various atomic operators have been defined corresponding to the various simple database operations. These operators belong to a class hierarchy. The base operator class is QL_Op, which is an abstract class which provides a common interface to all the operators. Each operator has 4 functions- (i) Open, (ii) Next, (iii) Reset and (iv) Close. Each operator also stores the schema of its output, which includes the relation name, attribute name, attribute offsets, attribute lengths and index number of each attribute. These operators are then chained together in a tree like fashion as described below.

The operators are broadly divided into two categories - (a) Unary operators and (b) Binary operators. The unary operators take the input from a single operator or none of the operators (file scan operators) whereas the binary operators take input from two other operators and combine them to produce their output. There are two separate abstract classes- QL_UnaryOp and QL_BinaryOp, extending the QL_Op base class to make this distinction. This abstraction is very convenient because the functionality can be easily extended by defining more operators which implement one of these classes.

The unary operators store a pointer to its child, which can be null in case of leaf nodes and the binary operators store a pointer for each of its two children. Each operator also stores a pointer to its parent node (null in case of root). The parent pointer makes the tree transformations very convenient. In the current implementation, five unary operators corresponding to (i) Condition checking, (ii) Projection, (iii) File Scan (iv) Index Scan (v) Permutation/Duplication of attributes have been implemented. One binary operator for Cross product has also been implemented.

#### Initial Query Plan ####
If we have N relations in our query, there are N! possible orderings of them. Besides the presence of selection and projection conditions make the search space really large. In this implementation, no attempt has been made to reorder the relations. Consider a general select query-
~~~~
select A1, A2 ... Am from R1, R2, ... Rn where C1, C2, ... Ck;
~~~~
where A1, A2 etc are the various attributes, R1, R2 etc are different relations and C1, C2 etc are the conditions involving attributes or constants in any of the given relations. After checking the validity of the query which includes validating the names of each input, checking for ambiguous attribute names, type consistency etc, a simple query tree is designed. This tree consists of a left leaning binary tree having file scan operators, one for each relation at its leaf and cross product operator at each non-leaf node. Then one operator corresponding to each condition is added above the root. After that, if the query doesn't have "*" in the select clause then a projection operator is placed above the new root, which projects the given attributes from the giant cross product of the relations. Further, if the attributes in the select clause differ from the natural ordering imposed by the relations in the from clause and the attributes within these relations, then a permutation operator is defined above the root. This operator also identifies the duplicates in the select clause and hence duplicates the attributes appropriately.

Let us take a simple example to visualize this. Let R(a,b,c), S(b,c,d) and T(c,d,e) be three relations. The attribute e of T is indexed. Consider the query - 
~~~~
select T.e, S.b, S.d from R, S, T where R.b = S.b and R.c = 1 and T.e = 3;
~~~~
The initial query plan is as follows-
~~~~
PERMUTE/DUPLICATE ATTRIBUTES
PROJECT S.b, S.d, T.e
FILTER BY T.e =AttrType: INT *(int *)data=3
FILTER BY R.c =AttrType: INT *(int *)data=1
FILTER BY R.b =S.b
CROSS PROD { 
  CROSS PROD { 
    FILE SCAN R
    ,
    FILE SCAN S
   } 
  ,
  FILE SCAN T
 }
~~~~
#### Query optimization ####
In the restricted optimization done in this implementation, the structure formed by the leaf nodes and the binary operators is never changed. The unary condition and projection nodes are pushed down the tree as much as possible. Pushing down condition operators gives us a large efficiency gain by rejecting invalid tuples early. Pushing down projection operator reduces the amount of data flowing between nodes and improves performance. Two separate recursive functions have been implemented for pushing down conditions and projection respectively. These functions are called with root of the binary tree as input and they recursively modify the whole tree. 

At first we push down conditions because some conditions can merge with the file scans to produce index scans. If we push down projections first then they might project out the indexed attributes and thus prevent us from doing index scans. 

##### Pushing Down Conditions #####
This function is called on the root once. When called on a node, this function calls itself on the child(ren) of the node. Then it operates on the current node only if the node is a condition node. Since conditions are pushed down first, the child of the current node can only be a File Scan node, Index Scan node, Cross Product Node or another Condition node. In this implementation, we are doing index scans only based on equality condition because we don't have the distribution of values in each relation. So, if the child is an Index Scan node then nothing is done. Other three cases are dealt as follows-

(i) Child is File Scan - If the condition is an equality condition having a value as its RHS then the condition is merged with the file scan to produce an Index Scan.
(ii) Child is another Condition node - Since conditions are commutative, child is swapped with the current node and the function is called again on the current node. This allows conditions to 'pass through' other conditions and hence guarantees each condition will reach as low as possible.
(iii) Child is a Cross product node - If the RHS of the condition is a value then the condition node is pushed towards the appropriate child containing the LHS attribute. If the RHS is another attribute then the condition is pushed only if both the attributes belong to the same child. If it doesn't then the condition is not moved. The function is again called on the condition after it got pushed down.

The result of this optimization on the above mentioned query is as follows - 
~~~~
PERMUTE/DUPLICATE ATTRIBUTES
PROJECT S.b, S.d, T.e
CROSS PROD { 
  FILTER BY R.b =S.b
  CROSS PROD { 
    FILTER BY R.c = 1
    FILE SCAN R
    ,
    FILE SCAN S
   } 
  ,
  INDEX SCAN T ON e
 }
~~~~
##### Pushing Down Projections #####
This function has similar calling architecture as condition pushing function. It is also recursive and is called on the root node. The projection node is more dynamic than the condition node in nature because it results in creation of new nodes in the operator tree. Lets examine the different cases. 

If the function is called on a node which is not a Projection node then it calls it on its child(ren) and returns. If the node is a projection node then different actions are taken based on the type of its child. The child of a Projection Node on which the call has been made can't be another Projection node. It also can't be a Permute/Duplicate node because pushes are only done downwards. If the child is File Scan or Index Scan then nothing is done because it gives no gain in efficiency. The other two cases are-

(i) Child is Condition node - If the current node is a condition node then two cases exist - (a) The projection passes the attributes involved in the condition. In this case the two operators are commutative and hence the Projection node is swapped with the Condition node. (b) If the condition involves at least one attribute not passed by the projection then the condition is no longer pushed down. Instead a new projection operator is defined as the new child of the condition attribute which passes the condition attributes along with the attributes passed by the Projection node on which the function was called. The function is then called again on the new projection operator.

(ii) Child is Cross Product node - In this case the projection operator splits up. The attributes are partitioned into two parts - one belonging to the right child and other belonging to the left child. Two cases arise (a) If both these partitions are non-empty then the two new projection operators are defined and they are pushed down the two children of the Cross Product node and the original Projection operator is deleted. The function is called again on the newly created Projection operators. (b) If one of the partitions is empty then a new Projection node is created and pushed down the child corresponding to the non-empty partition. The original Projection operator is not created. Ideally, this operation should have created an empty projection operator on the other child, which just throws empty tuples. But the current implementation doesn't support empty tuples and hence it has been avoided. It might not require much work to incorporate it but it has been left for now to deal with more interesting issues.

The result of this optimization on the above mentioned query is as follows - 
~~~~
PERMUTE/DUPLICATE ATTRIBUTES
CROSS PROD { 
  PROJECT S.b, S.d
  FILTER BY R.b = S.b
  CROSS PROD { 
    PROJECT R.b
    FILTER BY R.c = 1
    PROJECT R.b, R.c
    FILE SCAN R
    ,
    PROJECT S.b, S.d
    FILE SCAN S
   } 
  ,
  PROJECT T.e
  INDEX SCAN T ON e
 }
~~~~

#### Query Execution ####
After all this hard work, query execution is just a matter of calling Open, Next and Close on the root node. The resulting tuples are printed using the Printer class.

#### Update and Delete commands ####
The update and delete command are fairly simple because they involve only one relation and hence the query tree doesn't have any binary node. However these commands were implemented before implementing the select command and hence the query tree formalism has not been used for them. The effect of the same can be seen in the reduced elegance of code. 

#### Using Scratch Pages for holding records ####
I have not used scratch pages to hold intermediate records but since my implementation pushes down projections as much as possible, the extra memory used by program would be very small except in pathological cases. Also I thought of a more efficient methods which doesn't require defining a new scratch page for each binary operator but it needs some redesigning of the API. We can change the Open function to accept two pointers. After Query optimization, we traverse the operator tree and for each binary operator we allocate a disjoint portion of the scratch page depending on the size of its the record it needs to hold. A new scratch page is allocated if the current scratch page becomes full. A common portion is allocated for the unary operators. Then Open is called on all the operators with the allocated pointers which are saved by each operator. This implementation would not require number of pages equal to the number of binary operators and hence I request the TA to give a reasonable penalty, which would be 1-2 pages in most practical cases.


#### Extra functionality ####
Some micro optimizations have been done for the two commands - (i) If the update command is called with a trivial condition e.g, for a relation R(a,b) if the update command is "update R set a = a;" then the update is recognized as a trivial update and nothing is done. (ii) If the delete command has no condition then instead of deleting all the tuples, we can delete and recreate the entire relation. This has been implemented by me but has been commented out because we need to display the deleted tuples as a feedback to the user. I have also implemented type coercion, which changes the type of RHS to match the LHS for conditions in the command.

#### Debugging ####
I tested my implementation on many hand designed queries on small tables for debugging and also on CS145 ebay-dataset for tests. I also used valgrind to check for memory errors.

#### Acknowledgements ####
As always, I would like to thank Jaeho for discussion and ideas on many implementation aspects. I also consulted this paper on query optimization for some ideas -http://infolab.stanford.edu/~hyunjung/cs346/ioannidis.pdf 



# sm_DOC #

#### Overview ####
This part provides command line utilities for the user to interact with the database. Internally, the System Management (SM) module acts as a client to RM, IX and PF components and provides functionality such as creating and deleting a database, adding and dropping relations, adding and dropping indices for different attributes of a relation, bulk-loading from a csv file and displaying the contents of a relation on the command line.

#### File Hierarchy ####
Each database has a separate directory in the redbase home directory. All the files corresponding to the relations and indices on these relations belonging to a database are located within the database's directory. The database directory has two special relations called relcat and attrcat, which maintain the catalog of relations present in the database and the schema related information of each relation in the database respectively. As these catalogs are located within the database directory, it is illegal to create relations with these names. 

The index for a relation are stored within the database directory and they are named as relName.indexNum where relName is the name of the relation and indexNum is the index number, which is stored in attrcat. Again, due to the naming convention of the index files, it is illegal to create a relation with a name of the format xxxx.n where n is a number. If we allow creation of relations with these names, they may conflict with the index files of another relation xxxx. However, after ny implementation, I found out that the parser prohibits relation names containing a dot. With my implementation, such names could be allowed without creating conflicts with the index files.

#### Catalog Management ####
The relation catalog (relcat) of each database, stores the (i) relation name, (ii) tuple size, (iii) number of attributes and (iv) maximum allotted index number for each relation. The index number is used to keep track of the number to be allotted to a newly created index. This number increases by 1 each time a new index is created. Thus if we repeatedly create and drop the index on a particular attribute of a relation, the index number would keep increasing. 

The attribute catalog stores the (i) relation name, (ii) attribute name, (iii) attribute type, (iv) attribute offset, (v) attribute length and (vi) index number for each attribute of each relation in the database. The index number is set to -1 if the attribute is not indexed, otherwise it contains the n where relName.n is the index file for that attribute. The catalog files are always accessed from disk and are flushed to the disk each time they are modified. They are not stored in memory as they can get quite large for some databases. 

#### Operation ####
The user can interact with redbase using three commands which are provided by sm module. The are - (i) dbcreate dbName - It creates a new directory dbName and creates the catalog files in this directory if dbName is a valid unix directory name and another database with the same name doesn't exist. (ii) dbdestroy dbname - It deletes the directory dbName if it exists, thus deleting the entire database. (iii) redbase dbName - It starts the redbase parser which takes DDL commands from the user and calls the appropriate methods of SM_Manager class to serve those commands.The implementation of SM_Manager class is pretty straightforward as it mostly involves sanity check of input parameters, catalog management and calling the appropriate PF, RM or IX method.

#### Debugging ####

I wrote a C++ script to test and debug SM_Manager under many different scenarios. These tests work like the tests for RM and IX part and get compiled after slightly modifying the ql_stub.cc (removing all couts, didn't spend time on thinking why). To operate these tests, one must create a db using the createdb command and then input the db name in the test file and compile. This process can be automated but I didn't spend time on it. This helped me in debugging my code using ddd. After debugging my implementation, I ran tests using the tester script provided to us. I modified some of the given data files so that they contain null attributes in many different positions as well as the cases when all attributes in a line are null. I also used valgrind to test for memory leaks.

#### Acknowledgements ####
I would like to thank Jaeho for discussion on some implementation aspects.


# ix_DOC #

#### Index layout ####
The index has been implemented using a data structure similar to B+ tree and the variations are described below. The first page of the index file contains the file header which contains useful information about the index which is common to all the pages. The page header stores (i) key length, (ii) page number of root (iii) maximum number of keys that internal pages can hold, (iv) maximum number of keys that leaf pages can hold (v) maximum number of RIDs that overflow pages can hold (vi) page number of header (vii) indexed attribute type.

Three different types of pages can be found in the index - INTERNAL pages, LEAF pages and OVERFLOW pages. The RIDs are stored in LEAF and OVERFLOW pages only. The INTERNAL pages direct the search so that we can reach the appropriate LEAF page and then an OVERFLOW page if applicable. All pages keep the count of number of keys/pointers contained in them in their page header.

The INTERNAL pages are all the non-leaf tree pages. They contain n keys and (n+1) page numbers of their child pages which may be another INTERNAL page or a LEAF page. The LEAF pages contain m (key, RID) pairs and page numbers of left and right leaf pages. If any of these pages don't exist (for left-most and right-most leaf), the corresponding page number is set to IX_SENTINEL. An OVERFLOW page corresponds to a single key and hence it only stores the RIDs. As a result of this design, the three types of pages can accommodate different number of keys/RIDs. This helps us to do a very good utilization of pages, achieving a higher fan-out.

#### Inserting into index ####
When an index file is created, no root is allocated to it. When the first insertion is done, a root page gets allocated and it is considered to be of the type LEAF and hence it stores RIDs too. As more records are inserted, this LEAF page reaches it maximum capacity and then it needs to be split. Another LEAF page is allocated and half the keys of the original page are moved to it. To direct the searches to the two pages, an INTERNAL page is allocated. The INTERNAL page contains the minimum key of the newly allocated LEAF page. Subsequent inserts lead to creation of more LEAF pages and for each newly generated LEAF page, a key and page number is inserted in its parent. When the parent reaches its maximum capacity, it splits resulting in creation of an INTERNAL page and the key, page number insertion in the parent is recursively carried out. Due to the elegant recursive definition of the algorithm, I have implemented it recursively as well. I have written different functions which carry out individual steps such as splitting a page, creating a new page, inserting an entry into a page etc.

#### Handling duplicates ####

Duplicates keys may exist in the same LEAF page or in OVERFLOW pages. If a duplicate key is inserted in a LEAF page which has space, the (key, RID) pair is inserted in the LEAF page and no OVERFLOW page is allocated. When a page becomes full and we want to insert a new (key, RID) pair in it, we might need to split it to create space. But prior to considering a split, it is checked whether the LEAF page contains any duplicate keys and if it does, an OVERFLOW page is created for the key with the highest frequency in the LEAF. All the RIDs for this key are then moved to the overflow page. A single copy of the key is kept in the LEAF page having a dummy RID whose page number denotes the first page OVERFLOW page and the slot number is set to a negative value to indicate the presence of an OVERFLOW page. Thus splits can be avoided if duplicate keys exist in the same LEAF page. Also, as a result of this design, the same key can't exist in two different LEAF pages, avoiding the complication of keeping null pointer in parents to indicate such cases. Keeping duplicates in the LEAF helps us to avoid an extra IO if there is space in the LEAF.

#### Deleting from index ####
The tree structure is not changed during deletion and a page is kept even if it becomes empty. This doesn't affect performance much if we have infrequent deletions in our use case or if the number of insertions is much higher than the number of deletions which ends up filling the empty pages created during deletion. If an OVERFLOW page becomes empty during deletion, it is disposed if it is the last page in the linked list of OVERFLOW pages. If it is not the last page, all the contents of the next page are copied into in and the next page is deleted, effectively getting rid of the OVERFLOW page which became empty. A simple optimization that can be implemented while deletion in OVERFLOW pages is to get rid of the OVERFLOW page if there is sufficient space in the LEAF page. I haven't implemented it yet but plan to do so. 

#### Scanning ####
I have disallowed inequality scan operator as the scan using RM file scan would be more efficient in such cases. The six allowed scan operators are - (i) Null (always true) (ii) LT(<) (iii) LE(<=) (iv) EQ(==) (v) GT(>) and (vi) GE(>=). For the first three operators, we navigate to the left-most LEAF page in the tree and then scan through the linked list of LEAF pages from left to right till a violation of the scan operator is seen. For the last three operators, we navigate to the appropriate LEAF page which is likely to contain the smallest key which could match the scan condition. After reaching this LEAF, we start navigating towards right using the linked list of LEAF pages and stop when we encounter a key which doesn't match the scan operator or after we have exhausted all keys. The scan takes care of the presence of OVERFLOW pages and emits all the RIDs in an overflow page upon successive calls.  


#### Debugging ####
Setting the PF Page size to 60 helped me reduce the capacity of each page and thus helped me examine deep trees with just a few number of records. This greatly helped my debugging process. In addition to this, I used DDD to keep track of changes in the state of the tree during insertion and deletion. I also wrote a few simple tests myself which made debugging easier. I also ran valgrind on tests to check for memory leaks.

#### Acknowledgements ####
I would like to thank Jaeho for discussion on ways of handling duplicates and project management using git.

# rm_DOC #

#### File and Page Layout ####
Each file has its first page as the file header, on which the attributes which are specific to the file or common to all pages are stored. Similarly, each page has a page header storing some attributes along with a bitmap which tells which records are valid. Thus the organization of a file is- [File Header] [Page p1][Page p2][Page p3] ... [Page pn] Each page is organized as - [Page Header][Bitmap][Record R1][Record R2]...[Record Rm] where some of the record slots may have invalid data. Bitmap helps us to identify them.

These attributes stored in file header are - (i)Size of record in bytes, (ii) Number of records that can fit on each page, (iii) Size of bitmap in bytes, (iv) Offset of bitmap in bytes from beginning of page (v) Offset of location of first record from the beginning of page (vi) Count of empty pages in file (vii) Page number of header page (viii) First page belonging to the linked list of free pages. Since the file header is accessed many times, it is stored in memory as a private member of the RM_FileHandle object. If the header is changed since the file was opened, the header page is loaded from disk and updated before the file is closed. 

The page header stores the number of valid records in the page and the page number of the next page in the free page linked list. Storing the number of valid records in the page header helps us prematurely terminate the scan of the complete page in some cases. For example if the page has only one valid record, we wont need to search for another valid record in the bitmap after we have the first valid record. 

#### Keeping Track of Free Pages ####
We want to keep the pages of database as full as possible because if we have more pages, we will need more IOs to scan through them. Since there can be arbitrary insertions and deletions in the database, it is necessary to keep track of pages which have some free space so that while inserting a record we can find them quickly. If we don't keep track of these pages, we will either need to do a linear scan of the file or assign new pages while insertion, both of which are expensive.

As suggested in the specification doc, I used a linked list to keep track of free pages. The file header contains the page number of the head of the linked list. Each subsequent page in the linked list contains the page number of the next member in the list in its page header. The last page in the linked list has a special number RM_SENTINEL indicating the end of the list. While creation of file, there is no page other than the header and in this case the linked list is empty. Two kinds of pages get added to a free page linked list - (i) Newly allocated pages which are empty (ii) Pages which were full and just had a deletion. An addition to the list is made on the head and doesn't need any IOs to make updates to page header of other pages. 

However, keeping a linked list of free pages doesn't make it convenient for us to get rid of empty pages. A free page located in the middle of the linked list may become empty and if we choose to delete it, the page header of its predecessor needs to be updated. This would need an extra i/o. In my current implementation I have not deleted the empty pages. Some alternatives which can help us to delete some or all empty pages are- (i) Delete a free page only if it comes to the head of the free page linked list and the free page linked list has other pages in it. (ii) Keep a list of empty pages in the file header so that they can be avoided during file scan. If this list becomes larger than the page size, then incur the extra IO to delete these pages. (iii) Reconstruct the free page linked list during a file scan, getting rid of free pages in the process.

#### Keeping Track of Free Records ####
Each record page of a file has a bitmap which indicates whether a record slot is occupied or not. So, if there are n records in a page, then a bitmap of ceiling(n/8) bytes suffices. The indexing for the bits has been done using bitwise operators. There operations are declared as private methods of RM_FileHandle class. 

#### Optimizing Comparisons during File Scan ####
During the file scan, we need to scan through all the records contained in all the pages of the file and compare them against the given attribute using the given comparator. We need to define separate functions implementing each of the comparators and call one of them based on the input comparator. One possible strategy is to condition on the given comparator each time we make a comparison. So if there are n records in total and 8 comparators, we will be making 4n comparisons on an average to decide the comparator. I have used function pointers to speed up this process. The pointer is set to point to the correct comparator when the scan is initialized and thus only 8 comparisons are needed in the worst case for determining the comparator, independent of n.

#### Pinning Strategy during File Scan ####
I could think of two (un)pinning strategies- (i) Unpin the page after outputting a record. (ii) Keep the page pinned till all the records of the page have been examined. Consider the case of doing a block nested loop join on two relations R and S with comparable sizes. We read in a page of R as part of a scan and run a scan on S for each group of pages of R read. In this case the scan on S is fairly quick but the scan on R is slow. So, strategy (i) is more useful while scanning relation R and (ii) while scanning S. In my current implementation, I have implemented strategy (i). I preferred it over (ii) because if the scan is fast, it is highly likely that the unpinned page will still remain in the buffer pool. I intend to implement the second strategy as part of my personal extension. 

#### Debugging and Tests ####
I debugged the code using GDB and DDD. These tools were really helpful in isolating bugs. I ran the provided standard tests as well as rm_testrecsizes.cc and rm_testyh.cc given in the shared test repository. I was unable to compile some of the tests in the shared folder. I also wrote my own tests which test insertion of a large number of tuples and then check the output of the file scanner for some comparators. My code passed these tests. I also checked the integrity of the file and page headers using DDD and examined the changes as insertions were being made. I am not aware of any known bugs now but I would admit that I have not tested all the functions rigorously.

#### Acknowledgements ####
I would like to thank Jaeho for answering my questions, addressing my concerns regarding design and making many suggestions regarding coding efficiency and style, which includes the idea of using function pointers as described above. I would like to thank Prof. Hector for a discussion about keeping track of empty pages. I would also like to thank Aditya, with whom I discussed some implementation details.