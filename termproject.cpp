#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <sqlite3.h>
#include <ctime>
#include <cmath>
#include <unistd.h>
#include <sys/time.h>


#define BLOCK_SIZE 80    // Block size in bytes
#define BUFFER_POOL_SIZE 10 // Number of blocks in buffer pool

using namespace std;


// Structure for buffer block
typedef struct buffer_block
{
    string table_name;          // page file to which this block belongs to
    int page_number;           //page  number in page file, assuming that 

    //char *data;                // Data stored in the block
    int pin_count;             // Number of times the block is pinned
    int clock_referenceBit;     //used in Clock replacement strategy
    //int is_dirty;              // Flag indicating whether the block has been modified
    //int is_valid;              // Flag indicating whether the block is valid (i.e., has been loaded into the buffer pool)
    struct timeval timestamp;
    struct buffer_block *next; // Pointer to the next block in the linked list
    struct buffer_block *prev; // Pointer to the previous block in the linked list
} buffer_block_t;

// Structure for buffer pool
typedef struct buffer_pool
{
    buffer_block_t *head; // Pointer to the head of the linked list
    buffer_block_t *tail; // Pointer to the tail of the linked list
    int size;             // Number of blocks in the buffer pool
    int num_valid_blocks; // Number of blocks currently loaded in the buffer pool
    int num_disk_reads;   // Number of disk reads performed
    //int num_disk_writes;  // Number of disk writes performed
} buffer_pool_t;
buffer_pool_t *pool;

// Function to initialize buffer pool
buffer_pool_t *init_buffer_pool(int size)
{
    //buffer_pool_t *pool = (buffer_pool_t *)malloc(sizeof(buffer_pool_t));
    pool->head = NULL;
    pool->tail = NULL;
    pool->size = size;
    pool->num_valid_blocks = 0;
    pool->num_disk_reads = 0;
    //pool->num_disk_writes = 0;
    return pool;
}

// Function to allocate memory for a new buffer block
buffer_block_t *allocate_block()
{
    buffer_block_t *block = (buffer_block_t *)malloc(sizeof(buffer_block_t));

    //block->data = (char *)malloc(BLOCK_SIZE);
    block->pin_count = 0;
    block->clock_referenceBit=1;
    
    //block->is_dirty = 0;

    if(pool->head==NULL){
        pool->head=block;
        block->prev=NULL;
        block->next=NULL;
        pool->tail=block;
    }
    else {
    
        block->prev=pool->tail;
        block->prev->next=block;
        block->next=NULL;
        pool->tail = block;
    }
    
    return block;
}

// Function to load a block from disk into the buffer pool
void load_block(buffer_pool_t *pool, buffer_block_t *block,string table_name,int page_number)
{
    // TODO: Implement disk I/O to load block from disk
    pool->num_disk_reads++;
    block->table_name=table_name;
    block->page_number=page_number;
    gettimeofday(&block->timestamp,NULL);
    pool->num_valid_blocks++;
}

// Function to evict a block from the buffer pool
void evict_block(buffer_block_t *block)
{
    if (block->pin_count == 0)
    {
        if (pool->head == block)
        {
            //printf("evict block evicting head\n");
            pool->head = pool->head->next;
            if(pool->head!=NULL){
                //printf("pool->head != NULL;");
                pool->head->prev = NULL;
            }
            else{
                //printf("pool->head = NULL;");
            }
            
        }
        else if (block == pool->tail)
        {
            //printf("evict block evicting tail\n");
            block->prev->next = NULL;
            pool->tail = block->prev;
        }
        else
        {
            //printf("evict block evicting some middle block\n");
            block->prev->next = block->next;
            block->next->prev = block->prev;
        }
        // free(block->data);
        free(block);
        pool->num_valid_blocks--;
    }
    else
    {
        //printf("Block is pinned, cant be evicted\n");
    }
}

// Function to pin a block in the buffer pool
void pin_block(buffer_pool_t *pool, buffer_block_t *block)
{
    block->pin_count++;
}

// Function to unpin a block in the buffer pool
void unpin_block(buffer_pool_t *pool, buffer_block_t *block)
{
    block->pin_count--;
    /*if (block->pin_count == 0)
    {
        if (pool->num_valid_blocks >= pool->size)
        {
            evict_block(pool);
        }

        if (pool->head == NULL)
        {
            pool->head = block;
            pool->tail = block;
        }
        else
        {
            block->next = pool->head;
            pool->head->prev = block;
            pool->head = block;
        }
        pool->num_valid_blocks++;
    }*/
}

vector<string> extractTableNames(string query) {
    vector<string> tableNames;

    size_t fromIndex = query.find("FROM");
    size_t joinIndex = query.find("JOIN");

    while (fromIndex != string::npos || joinIndex != string::npos) {
        size_t index = min(fromIndex, joinIndex);
        string tableName;

        // Extract the table name
        size_t start = query.find_first_not_of(" \t", index + 4);
        size_t end = query.find_first_of(" \t\n\r\f\v", start);
        tableName = query.substr(start, end - start);

        tableNames.push_back(tableName);

        // Search for the next table name
        fromIndex = query.find("FROM", end);
        joinIndex = query.find("JOIN", end);
    }

    return tableNames;
}

int get_table_info(const char* db_file, string table_name, int* numb_rows, int* record_size){
    sqlite3* db;
    char* err_msg = 0;

    int rc = sqlite3_open("mydatabase.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    sqlite3_stmt* stmt;
    char sql_query[100];
    sprintf(sql_query,"SELECT * FROM %s",table_name.c_str());
    rc = sqlite3_prepare_v2(db, sql_query, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    int num_cols = sqlite3_column_count(stmt);
    int num_rows = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        num_rows++;
        int total_size = 0;
        for (int i = 0; i < num_cols; i++) {
            int size = sqlite3_column_bytes(stmt, i);
            total_size += size;
            //printf("Column %d size: %d bytes\n", i, size);
        }
        //printf("size of record  in table : %s : %d bytes\n",table_name.c_str(), total_size);
        *record_size=max(*record_size,total_size);
    }
    printf("Total number of records: %d\n", num_rows);
    printf("Final size of record= %d\n",*record_size);
    *numb_rows=num_rows;

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return 0;
}
buffer_block_t * search_in_pool(string table_name,int page_number){
    buffer_block_t * curr_block=pool->head;
    while(curr_block!=NULL){
        if(curr_block->table_name==table_name && curr_block->page_number==page_number){
            gettimeofday(&curr_block->timestamp,NULL);
            return curr_block;
        }
        curr_block=curr_block->next;

    }
    return NULL;

}
buffer_block_t *mru(buffer_pool_t *pool,string table_name,int page_number)
{
    //let's take the mru block to be the pools head
    buffer_block_t *block = pool->head;
    buffer_block_t *mru_block = block;

    //iterating over all the blocks
    while (block != NULL)
    {
        //printf("timestamp of %s %d : %ld %d\n",block->table_name.c_str(),block->page_number,block->timestamp.tv_sec,block->timestamp.tv_usec);
        if( (block->timestamp.tv_sec > mru_block->timestamp.tv_sec )|| (block->timestamp.tv_sec == mru_block->timestamp.tv_sec && (block->timestamp.tv_usec > mru_block->timestamp.tv_usec )) )
        {
            mru_block = block;
        }
        //moving to next block
        block = block->next;
    }
    //printf("evicting %s %d\n",mru_block->table_name.c_str(),mru_block->page_number);
    evict_block(mru_block);
    buffer_block_t *allocated_block=allocate_block();
    load_block(pool,allocated_block,table_name,page_number);
    
       //block is valid and pin count==0 then we need to evict the block
    //printf("mru is evicted\n");
    return allocated_block;
 
}

buffer_block_t *lru(buffer_pool_t *pool,string table_name,int page_number)
{
    //let's take the mru block to be the pools head
    buffer_block_t *block = pool->head;
    buffer_block_t *mru_block = block;

    //iterating over all the blocks
    while (block != NULL)
    {
        //printf("timestamp of %s %d : %ld %d\n",block->table_name.c_str(),block->page_number,block->timestamp.tv_sec,block->timestamp.tv_usec);
        if( (block->timestamp.tv_sec < mru_block->timestamp.tv_sec )|| (block->timestamp.tv_sec == mru_block->timestamp.tv_sec && (block->timestamp.tv_usec < mru_block->timestamp.tv_usec )) )
        {
            mru_block = block;
        }
        //moving to next block
        block = block->next;
    }
    //printf("evicting %s %d\n",mru_block->table_name.c_str(),mru_block->page_number);
    evict_block(mru_block);
    buffer_block_t *allocated_block=allocate_block();
    load_block(pool,allocated_block,table_name,page_number);
    
       //block is valid and pin count==0 then we need to evict the block
    //printf("mru is evicted\n"); 
    return allocated_block;
 
}

buffer_block_t *clock_hand;
buffer_block_t *clock_replacement(buffer_pool_t *pool,string table_name,int page_number)
{
    if(clock_hand==NULL){
        clock_hand=pool->head;
    }

    while(1)
    {
        // If block is not pinned and reference bit is 0, evict it
        if(clock_hand->pin_count == 0 && clock_hand->clock_referenceBit == 0)
        {
            //printf("Eviction in process\n");
            //evict_block(clock_hand);
            buffer_block_t *allocated_block= (buffer_block_t *)malloc(sizeof(buffer_block_t));

    //block->data = (char *)malloc(BLOCK_SIZE);
            allocated_block->pin_count = 0;
            allocated_block->clock_referenceBit=1;

            load_block(pool,allocated_block,table_name,page_number);
            allocated_block->next=clock_hand->next;
            allocated_block->prev=clock_hand->prev;
            if(clock_hand->next!=NULL){
                clock_hand->next->prev=allocated_block;
            }
            else{
                pool->tail=allocated_block;
            }
            if(clock_hand->prev!=NULL){
                clock_hand->prev->next=allocated_block;
            }
            else{
                pool->head=allocated_block;
            }
            //printf("Evicted and loaded a new block\n");
            //printf("evicted %s %d\n",clock_hand->table_name.c_str(),clock_hand->page_number);

            if(clock_hand->next == NULL)
            {
                clock_hand = pool->head;
            }
            else
            {
                clock_hand = clock_hand->next;
            }

            return allocated_block;
        }
        // If block is not pinned and reference bit is 1, set it to 0 and move the clock hand forward
        else if(clock_hand->pin_count == 0 && clock_hand->clock_referenceBit == 1)
        {
            clock_hand->clock_referenceBit = 0;
            if(clock_hand->next == NULL)
            {
                clock_hand = pool->head;
            }
            else
            {
                clock_hand = clock_hand->next;
            }
        }
        // If block is pinned, move the clock hand forward
        else
        {
            if(clock_hand->next == NULL)
            {
                clock_hand = pool->head;
            }
            else
            {
                clock_hand = clock_hand->next;
            }
        }
    }
}

int using_mru(buffer_pool_t *pool,vector<string> tables,int number_of_blocks[2]){
    if (tables.size() == 1)
    {
        int no_of_blocks_left = number_of_blocks[0];
        while (no_of_blocks_left != 0)
        {
            // search in pool if it exists
            buffer_block_t* search_ans=search_in_pool(tables[0],number_of_blocks[0] - no_of_blocks_left);
            if(search_ans!=NULL){
                //printf("found in buffer %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left);
                no_of_blocks_left--;
                //sleep(1);
                continue;
            }
            if (BUFFER_POOL_SIZE > pool->num_valid_blocks)
            {
                //printf("allocating in buffer %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left);
                buffer_block_t *new_block = allocate_block();
                load_block(pool, new_block, tables[0], number_of_blocks[0] - no_of_blocks_left);
                no_of_blocks_left--;
                continue;
            }
            // find block using replacement strategy (in this only eviction and allot_block)
            //clock(tables[0],number_of_blocks[0] - no_of_blocks_left)
            //printf("Calling MRU %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left);
            mru(pool,tables[0],number_of_blocks[0] - no_of_blocks_left);
        }
    }
    else{
        int no_of_blocks_left_1 = number_of_blocks[0];
        while (no_of_blocks_left_1 != 0)
        {
            // search in pool if it exists
            buffer_block_t* search_ans=search_in_pool(tables[0],number_of_blocks[0] - no_of_blocks_left_1);
            if(search_ans!=NULL){
                //printf("found in buffer%s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left_1);
                no_of_blocks_left_1--;
                pin_block(pool,search_ans);
                //sleep(1);
                continue;
            }
            if (BUFFER_POOL_SIZE > pool->num_valid_blocks)
            {
                //printf("allocating in buffer %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left_1);
                search_ans = allocate_block();
                load_block(pool, search_ans, tables[0], number_of_blocks[0] - no_of_blocks_left_1);
                pin_block(pool,search_ans);
                no_of_blocks_left_1--;
                
            }
            else
            {
                // find block using replacement strategy (in this only eviction and allot_block)
                 //printf("Calling MRU %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left_1);
                search_ans =mru(pool,tables[0],number_of_blocks[0] - no_of_blocks_left_1);
                pin_block(pool,search_ans);
                no_of_blocks_left_1--;
            }
            //sleep(1);
            int no_of_blocks_left_2 = number_of_blocks[1];
            while (no_of_blocks_left_2 != 0)
            {
                // search in pool if it exists
                buffer_block_t* search_ans=search_in_pool(tables[1],number_of_blocks[1] - no_of_blocks_left_2);
                if(search_ans!=NULL){

                    //printf("found in buffer %s %d\n",tables[1].c_str(),number_of_blocks[1] - no_of_blocks_left_2);
                    no_of_blocks_left_2--;
                    //sleep(1);
                    continue;
                }

                if (BUFFER_POOL_SIZE > pool->num_valid_blocks)
                {
                    buffer_block_t *new_block = allocate_block();
                    load_block(pool, new_block, tables[1], number_of_blocks[1] - no_of_blocks_left_2);
                    //printf("allocating in buffer %s %d\n",tables[1].c_str(),number_of_blocks[1] - no_of_blocks_left_2);
                    no_of_blocks_left_2--;
                    
                }
                else
                {
                    // find block using replacement strategy (in this only eviction and allot_block)
                    //printf("Calling MRU %s %d\n",tables[1].c_str(),number_of_blocks[1] - no_of_blocks_left_2);
                    mru(pool,tables[1],number_of_blocks[1] - no_of_blocks_left_2);
                    no_of_blocks_left_2--;
                }
                //sleep(1);
            }
            unpin_block(pool,search_ans);

        }
    }
    printf("number of mru disk reads=%d\n",pool->num_disk_reads);
    return pool->num_disk_reads;
}

int using_lru(buffer_pool_t *pool,vector<string> tables,int number_of_blocks[2]){
    if (tables.size() == 1)
    {
        int no_of_blocks_left = number_of_blocks[0];
        while (no_of_blocks_left != 0)
        {
            // search in pool if it exists
            buffer_block_t* search_ans=search_in_pool(tables[0],number_of_blocks[0] - no_of_blocks_left);
            if(search_ans!=NULL){
                //printf("found in buffer %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left);
                no_of_blocks_left--;
                //sleep(1);
                continue;
            }
            if (BUFFER_POOL_SIZE > pool->num_valid_blocks)
            {
                //printf("allocating in buffer %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left);
                buffer_block_t *new_block = allocate_block();
                load_block(pool, new_block, tables[0], number_of_blocks[0] - no_of_blocks_left);
                no_of_blocks_left--;
                continue;
            }
            // find block using replacement strategy (in this only eviction and allot_block)
            //clock(tables[0],number_of_blocks[0] - no_of_blocks_left)
            //printf("Calling LRU %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left);
            lru(pool,tables[0],number_of_blocks[0] - no_of_blocks_left);
        }
    }
    else{
        int no_of_blocks_left_1 = number_of_blocks[0];
        while (no_of_blocks_left_1 != 0)
        {
            // search in pool if it exists
            buffer_block_t* search_ans=search_in_pool(tables[0],number_of_blocks[0] - no_of_blocks_left_1);
            if(search_ans!=NULL){
                //printf("found in buffer%s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left_1);
                no_of_blocks_left_1--;
                pin_block(pool,search_ans);
                //sleep(1);
                continue;
            }
            if (BUFFER_POOL_SIZE > pool->num_valid_blocks)
            {
                //printf("allocating in buffer %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left_1);
                search_ans = allocate_block();
                load_block(pool, search_ans, tables[0], number_of_blocks[0] - no_of_blocks_left_1);
                pin_block(pool,search_ans);
                no_of_blocks_left_1--;
                
            }
            else
            {
                // find block using replacement strategy (in this only eviction and allot_block)
                // printf("Calling LRU %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left_1);
                search_ans =lru(pool,tables[0],number_of_blocks[0] - no_of_blocks_left_1);
                pin_block(pool,search_ans);
                no_of_blocks_left_1--;
            }
            //sleep(1);
            int no_of_blocks_left_2 = number_of_blocks[1];
            while (no_of_blocks_left_2 != 0)
            {
                // search in pool if it exists
                buffer_block_t* search_ans=search_in_pool(tables[1],number_of_blocks[1] - no_of_blocks_left_2);
                if(search_ans!=NULL){

                    //printf("found in buffer %s %d\n",tables[1].c_str(),number_of_blocks[1] - no_of_blocks_left_2);
                    no_of_blocks_left_2--;
                    //sleep(1);
                    continue;
                }

                if (BUFFER_POOL_SIZE > pool->num_valid_blocks)
                {
                    buffer_block_t *new_block = allocate_block();
                    load_block(pool, new_block, tables[1], number_of_blocks[1] - no_of_blocks_left_2);
                    //printf("allocating in buffer %s %d\n",tables[1].c_str(),number_of_blocks[1] - no_of_blocks_left_2);
                    no_of_blocks_left_2--;
                    
                }
                else
                {
                    // find block using replacement strategy (in this only eviction and allot_block)
                    //printf("Calling LRU %s %d\n",tables[1].c_str(),number_of_blocks[1] - no_of_blocks_left_2);
                    lru(pool,tables[1],number_of_blocks[1] - no_of_blocks_left_2);
                    no_of_blocks_left_2--;
                }
                //sleep(1);
            }
            unpin_block(pool,search_ans);

        }
    }
    printf("number of lru disk reads=%d\n",pool->num_disk_reads);
    return pool->num_disk_reads;
}

int using_clock(buffer_pool_t *pool,vector<string> tables,int number_of_blocks[2]){
    if (tables.size() == 1)
    {
        int no_of_blocks_left = number_of_blocks[0];
        while (no_of_blocks_left != 0)
        {
            // search in pool if it exists
            buffer_block_t* search_ans=search_in_pool(tables[0],number_of_blocks[0] - no_of_blocks_left);
            if(search_ans!=NULL){
                //printf("found in buffer %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left);
                no_of_blocks_left--;
                //sleep(1);
                continue;
            }
            if (BUFFER_POOL_SIZE > pool->num_valid_blocks)
            {
                //printf("allocating in buffer %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left);
                buffer_block_t *new_block = allocate_block();
                load_block(pool, new_block, tables[0], number_of_blocks[0] - no_of_blocks_left);
                no_of_blocks_left--;
                continue;
            }
            // find block using replacement strategy (in this only eviction and allot_block)
            //clock(tables[0],number_of_blocks[0] - no_of_blocks_left)
            //printf("Calling clock %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left);
            clock_replacement(pool,tables[0],number_of_blocks[0] - no_of_blocks_left);
        }
    }
    else{
        int no_of_blocks_left_1 = number_of_blocks[0];
        while (no_of_blocks_left_1 != 0)
        {
            // search in pool if it exists
            buffer_block_t* search_ans=search_in_pool(tables[0],number_of_blocks[0] - no_of_blocks_left_1);
            if(search_ans!=NULL){
                //printf("found in buffer%s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left_1);
                no_of_blocks_left_1--;
                pin_block(pool,search_ans);
                //sleep(1);
                continue;
            }
            if (BUFFER_POOL_SIZE > pool->num_valid_blocks)
            {
                //printf("allocating in buffer %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left_1);
                search_ans = allocate_block();
                load_block(pool, search_ans, tables[0], number_of_blocks[0] - no_of_blocks_left_1);
                pin_block(pool,search_ans);
                no_of_blocks_left_1--;
                
            }
            else
            {
                // find block using replacement strategy (in this only eviction and allot_block)
                 //printf("Calling clock %s %d\n",tables[0].c_str(),number_of_blocks[0] - no_of_blocks_left_1);
                search_ans =clock_replacement(pool,tables[0],number_of_blocks[0] - no_of_blocks_left_1);
                pin_block(pool,search_ans);
                no_of_blocks_left_1--;
            }
            //sleep(1);
            int no_of_blocks_left_2 = number_of_blocks[1];
            while (no_of_blocks_left_2 != 0)
            {
                // search in pool if it exists
                buffer_block_t* search_ans=search_in_pool(tables[1],number_of_blocks[1] - no_of_blocks_left_2);
                if(search_ans!=NULL){

                    //printf("found in buffer %s %d\n",tables[1].c_str(),number_of_blocks[1] - no_of_blocks_left_2);
                    no_of_blocks_left_2--;
                    //sleep(1);
                    continue;
                }

                if (BUFFER_POOL_SIZE > pool->num_valid_blocks)
                {
                    buffer_block_t *new_block = allocate_block();
                    load_block(pool, new_block, tables[1], number_of_blocks[1] - no_of_blocks_left_2);
                    //printf("allocating in buffer %s %d\n",tables[1].c_str(),number_of_blocks[1] - no_of_blocks_left_2);
                    no_of_blocks_left_2--;
                    
                }
                else
                {
                    // find block using replacement strategy (in this only eviction and allot_block)
                    //printf("Calling clock %s %d\n",tables[1].c_str(),number_of_blocks[1] - no_of_blocks_left_2);
                    clock_replacement(pool,tables[1],number_of_blocks[1] - no_of_blocks_left_2);
                    no_of_blocks_left_2--;
                }
                //sleep(1);
            }
            unpin_block(pool,search_ans);

        }
    }
    printf("number of clock disk reads=%d\n",pool->num_disk_reads);
    return pool->num_disk_reads;
}


int main(){
    pool = (buffer_pool_t *)malloc(sizeof(buffer_pool_t));
    init_buffer_pool(BUFFER_POOL_SIZE);
    clock_hand=NULL;
    printf("num of valid blocks=%d\n",pool->num_valid_blocks);
    string query;
    printf("Enter SQL query: ");
    getline(cin,query);
    cout<<"query="<<query<<endl;
    vector<string> tables=extractTableNames(query);
    cout<<"size of tables="<<tables.size()<<endl;
    int num_rows, record_size=0;
    int number_of_blocks[2];
    for(int i=0;i<tables.size();i++){
        cout<<tables[i]<<endl;
        get_table_info("mydatabase.db",tables[i],&num_rows,&record_size);
        number_of_blocks[i] = ceil(num_rows/ (BLOCK_SIZE/record_size));
        printf("number_of_blocks in table %s = %d\n",tables[i].c_str(),number_of_blocks[i]);
    }
    
    


    /*buffer_block_t* block1=allocate_block(1);
    buffer_block_t* block2=allocate_block(2);
    buffer_block_t* block3=allocate_block(3);
    evict_block(block2);
    printf("num of valid blocks=%d\n",pool->num_valid_blocks);
    evict_block(block1);
    printf("num of valid blocks=%d\n",pool->num_valid_blocks);*/

    //simple selection query
    using_clock(pool,tables,number_of_blocks);
    init_buffer_pool(BUFFER_POOL_SIZE);
    using_lru(pool,tables,number_of_blocks);
    init_buffer_pool(BUFFER_POOL_SIZE);
    using_mru(pool,tables,number_of_blocks);

    
    

}

//insert into persons values (14,"sukee", "rajeevi" ,"something","kadapa");