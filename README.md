# rm_DOC #

Your overall design
The key data structures used in your component
Your testing process
Any known bugs
Any assistance you received (from the instructor, TA, other students in the course, or anyone else) in program design or debugging, as per the Honor Code statement provided in the course homepage

### File and Page Layout ###
Each file has its first page as the file header, on which the attributes which are specific to the file or common to all pages are stored. Similarly, each page has a page header storing some attributes along with a bitmap which tells which records are valid. Thus the organization of a file is- [File Header] [Page p1][Page p2][Page p3] ... [Page pn] Each page is organized as - [Page Header][Bitmap][Record R1][Record R2]...[Record Rm] where some of the record slots may have invalid data. Bitmap helps us to identify them.

#### File Header####
These attributes stored in file header are - (i)Size of record in bytes, (ii) Number of records that can fit on each page, (iii) Size of bitmap in bytes, (iv) Offset of bitmap in bytes from beginning of page (v) Offset of location of first record from the beginning of page (vi) Count of empty pages in file (vii) Page number of header page (viii) First page belonging to the linked list of free pages. Since the file header is accessed many times, it is stored in memory as a private member of the RM_FileHandle object. If the header is changed since the file was opened, the header page is loaded from disk and updated before the file is closed. 

#### Page Header####
The page header stores the number of valid records in the page and the page number of the next page in the free page linked list. Storing the number of valid records in the page header helps us prematurely terminate the scan of the complete page in some cases. For example if the page has only one valid record, we wont need to search for another valid record in the bitmap after we have the first valid record. 

#### Maintaining Free Page List ####
We want to keep the pages of database as full as possible because if we have more pages, we will need more IOs to scan through them. Since there can be arbitrary insertions and deletions in the database, it is necessary to keep track of pages which have some free space so that while inserting a record we can find them quickly. If we don't keep track of these pages, we will either need to do a linear scan of the file or assign new pages while insertion, both of which are expensive.

As suggested in the specification doc, I used a linked list to keep track of free pages. The file header contains the page number of the head of the linked list. Each subsequent page in the linked list contains the page number of the next member in the list in its page header. The last page in the linked list has a special number RM_SENTINEL indicating the end of the list. While creation of file, there is no page other than the header and in this case the linked list is empty. Two kinds of pages get added to a free page linked list - (i) Newly allocated pages which are empty (ii) Pages which were full and just had a deletion. An addition to the list is made on the head and doesn't need any IOs to make updates to page header of other pages. 



#### 