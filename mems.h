/*
All the main functions with respect to the MeMS are implemented here
read the function description for more details

NOTE: DO NOT CHANGE THE NAME OR SIGNATURE OF FUNCTIONS ALREADY PROVIDED
you are only allowed to implement the functions 
you can also make additional helper functions a you wish

REFER DOCUMENTATION FOR MORE DETAILS ON FUNCTIONS AND THEIR FUNCTIONALITY
*/
// add other headers as required
#include<stdio.h>
#include<stdlib.h>
#include<sys/mman.h>

/*
Use this macro wherever you need PAGE_SIZE.
As PAGESIZE can differ system to system we should have flexibility to modify this 
macro to make the output of all system same and conduct a fair evaluation. 
*/
#define PAGE_SIZE 16384
// 'getconf PAGESIZE' on Mac m1 terminal returns 16384
// void *first = mmap(NULL, PAGE_SIZE*2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
// munmap(first, PAGE_SIZE*2);

struct Address {
	int virtual_address;
	int *physical_address;
	int size;
	struct Sub_Chain_Node *associated_node;
	struct Address *prev_address_node;
	struct Address *next_address_node;
};

struct Sub_Chain_Node {
	int type; // 1: Process; 0: Hole
	int size;
	int virtual_address;
	int *physical_address;
	struct Sub_Chain_Node *prev_sub_node;
    struct Sub_Chain_Node *next_sub_node;
};

struct Main_Chain_Node {
	int no_of_pages;
	struct Sub_Chain_Node *head_sub_list;
	struct Main_Chain_Node *prev_main_node;
	struct Main_Chain_Node *next_main_node;
};

struct Address *head_address_node;
struct Address *tail_address_node;

struct Main_Chain_Node *head_main_node;
struct Main_Chain_Node *tail_main_node;

int cur_virtual_address;
void *other_main_memory_ptr;
void *other_main_memory_end;

void *other_sub_memory_ptr;
void *other_sub_memory_end;

void *other_address_memory_ptr;
void *other_address_memory_end;

/*
Initializes all the required parameters for the MeMS system. The main parameters to be initialized are:
1. the head of the free list i.e. the pointer that points to the head of the free list
2. the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
3. any other global variable that you want for the MeMS implementation can be initialized here.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_init(){
	head_address_node = NULL;
	tail_address_node = NULL;
	head_main_node = NULL;
	tail_main_node = NULL;
	head_sub_node = NULL;
	other_main_memory_ptr = NULL;
	other_main_memory_end = NULL;
	other_sub_memory_ptr = NULL;
	other_sub_memory_end = NULL;
	other_address_memory_ptr = NULL;
	other_address_memory_end = NULL;
	cur_virtual_address = 0;
}


/*
This function will be called at the end of the MeMS system and its main job is to unmap the 
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish()
{
	struct Main_Chain_Node *node = head_main_node;
	while (node != NULL)
	{
		struct Sub_Chain_Node *sub_node = node->head_sub_list;
		while (sub_node != NULL)
		{
			if (munmap(sub_node->physical_address, sub_node->size) == -1)
			{
				perror("munmap error");
			}
			sub_node = sub_node->next_sub_node;
		}
		node = node->next_main_node;
	}
}


/*
Allocates memory of the specified size by reusing a segment from the free list if 
a sufficiently large segment is available. 

Else, uses the mmap system call to allocate more memory on the heap and updates 
the free list accordingly.

Note that while mapping using mmap do not forget to reuse the unused space from mapping
by adding it to the free list.
Parameter: The size of the memory the user program wants
Returns: MeMS Virtual address (that is created by MeMS)
*/ 
void* mems_malloc(size_t size){
	//Checking if Space is Left in Already Allocated Memory
	struct Main_Chain_Node *cur_main_node = head_main_node;
	struct Sub_Chain_Node *cur_sub_node;

	while (cur_main_node != NULL) {
		cur_sub_node = cur_main_node->head_sub_list;

		while (cur_sub_node != NULL) {
			if (cur_sub_node->type == 0 && cur_sub_node->size > (int)size) {
				void *new_other_sub_memory_node = mmap(NULL, 2 * PAGE_SIZE * (((int)sizeof(struct Sub_Chain_Node *)) / PAGE_SIZE + 1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				other_sub_memory_ptr = (int*)new_other_sub_memory_node;
				other_sub_memory_end = other_sub_memory_ptr + 2 * PAGE_SIZE * (((int)sizeof(struct Sub_Chain_Node *)) / PAGE_SIZE + 1);

				struct Sub_Chain_Node *new_hole_node = (struct Sub_Chain_Node *) other_sub_memory_ptr;
				other_sub_memory_ptr += (int)sizeof(struct Sub_Chain_Node *);

				cur_sub_node->type = 1;
				new_hole_node->type = 0;
				new_hole_node->size = cur_sub_node->size - (int) size;
				int hole_size = cur_sub_node->size - (int) size;
				cur_sub_node->size = (int) size;
				int sub_size = (int) size;
				new_hole_node->virtual_address = cur_sub_node->virtual_address + (int)size;
				int hole_va = cur_sub_node->virtual_address + (int)size;
				new_hole_node->physical_address = cur_sub_node->physical_address + (int)size;
				new_hole_node->next_sub_node = cur_sub_node->next_sub_node;
				cur_sub_node->next_sub_node = new_hole_node;
				new_hole_node->prev_sub_node = cur_sub_node;
				new_hole_node->type = 0;

				if (new_hole_node->next_sub_node) {
					new_hole_node->next_sub_node->prev_sub_node = new_hole_node;
				}

				void *new_other_address_memory_node = mmap(NULL, PAGE_SIZE * (((int)sizeof(struct Address *)) / PAGE_SIZE + 1) * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
				other_address_memory_ptr = (int*)new_other_address_memory_node;
				other_address_memory_end = other_address_memory_ptr + 2 * PAGE_SIZE * (((int)sizeof(struct Address *)) / PAGE_SIZE + 1);

				struct Address *new_address_node = (struct Address *) other_address_memory_ptr;
				other_address_memory_ptr += sizeof(struct Address *);

				tail_address_node->next_address_node = new_address_node;
				new_address_node->prev_address_node = tail_address_node;
				tail_address_node = new_address_node;
				new_address_node->next_address_node = NULL;

				tail_address_node->virtual_address = new_hole_node->virtual_address;
				tail_address_node->physical_address = new_hole_node->physical_address;
				tail_address_node->size = new_hole_node->size;
				tail_address_node->associated_node = new_hole_node;

				return (void *) cur_sub_node->virtual_address;

			}
			cur_sub_node = cur_sub_node->next_sub_node;
		}

		cur_main_node = cur_main_node->next_main_node;
	}


	//No Space is left in Already Allocated Memory
	int pages_required = ((int)size) / PAGE_SIZE + 1;
	int start_other_memory_flag = 0;

	if (!other_main_memory_ptr) {
		start_other_memory_flag = 1;
	}


	void *new_other_main_memory_node = mmap(NULL, PAGE_SIZE * (((int)sizeof(struct Main_Chain_Node *)) / PAGE_SIZE + 1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	other_main_memory_ptr = (int*)new_other_main_memory_node;
	other_main_memory_end = other_main_memory_ptr + PAGE_SIZE * (((int)sizeof(struct Main_Chain_Node *)) / PAGE_SIZE + 1);

	struct Main_Chain_Node *new_main_node = (struct Main_Chain_Node *) other_main_memory_ptr;
	other_main_memory_ptr += (int)sizeof(struct Main_Chain_Node *);


	if (start_other_memory_flag) {
		head_main_node = new_main_node;
		tail_main_node = new_main_node;
		new_main_node->prev_main_node = NULL;
		new_main_node->next_main_node = NULL;
	}
	else {
		tail_main_node->next_main_node = new_main_node;
		new_main_node->prev_main_node = tail_main_node;
		tail_main_node = new_main_node;
		new_main_node->next_main_node = NULL;
	}

	new_main_node->no_of_pages = pages_required;

	int *main_node_memory = (int *) mmap(NULL, PAGE_SIZE * pages_required, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);


	void *new_other_sub_memory_node = mmap(NULL, PAGE_SIZE * (((int)sizeof(struct Sub_Chain_Node *)) / PAGE_SIZE + 1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	other_sub_memory_ptr = (int*)new_other_sub_memory_node;
	other_sub_memory_end = other_sub_memory_ptr + PAGE_SIZE * (((int)sizeof(struct Sub_Chain_Node *)) / PAGE_SIZE + 1);

	struct Sub_Chain_Node *process_node = (struct Sub_Chain_Node *) other_sub_memory_ptr;
	other_sub_memory_ptr += (int)sizeof(struct Sub_Chain_Node *);

	process_node->type = 1;
	process_node->size = (int) size;
	process_node->virtual_address = cur_virtual_address;
	process_node->physical_address = main_node_memory;
	tail_main_node->head_sub_list = process_node;
	process_node->prev_sub_node = NULL;


	void *new_other_address_memory_node = mmap(NULL, PAGE_SIZE * (((int)sizeof(struct Address *)) / PAGE_SIZE + 1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	other_address_memory_ptr = (int*)new_other_address_memory_node;
	other_address_memory_end = other_address_memory_ptr + PAGE_SIZE * (((int)sizeof(struct Address *)) / PAGE_SIZE + 1);
	

	struct Address *new_address_node = (struct Address *) other_address_memory_ptr;
	other_address_memory_ptr += (int)sizeof(struct Address *);

	if (start_other_memory_flag) {
		head_address_node = new_address_node;
		tail_address_node = new_address_node;
		new_address_node->prev_address_node = NULL;
		new_address_node->next_address_node = NULL;
	}
	else {
		tail_address_node->next_address_node = new_address_node;
		new_address_node->prev_address_node = tail_address_node;
		tail_address_node = new_address_node;
		new_address_node->next_address_node = NULL;
	}

	tail_address_node->virtual_address = cur_virtual_address;
	tail_address_node->physical_address = main_node_memory;
	tail_address_node->size = process_node->size;
	tail_address_node->associated_node = process_node;

	cur_virtual_address += process_node->size;
	main_node_memory += process_node->size;


	new_other_sub_memory_node = mmap(NULL, PAGE_SIZE * (((int)sizeof(struct Sub_Chain_Node *)) / PAGE_SIZE + 1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	other_sub_memory_ptr = (int*)new_other_sub_memory_node;
	other_sub_memory_end = other_sub_memory_ptr + PAGE_SIZE * (((int)sizeof(struct Sub_Chain_Node *)) / PAGE_SIZE + 1);

	struct Sub_Chain_Node *hole_node = (struct Sub_Chain_Node *) other_sub_memory_ptr;
	other_sub_memory_ptr += (int)sizeof(struct Sub_Chain_Node *);

	hole_node->type = 0;
	hole_node->size = PAGE_SIZE * pages_required - (int) size;
	hole_node->virtual_address = cur_virtual_address;
	hole_node->physical_address = main_node_memory;
	hole_node->prev_sub_node = process_node;
	process_node->next_sub_node = hole_node;
	hole_node->type = 0;
	hole_node->size = PAGE_SIZE * pages_required - (int) size;
	hole_node->virtual_address = cur_virtual_address;
	hole_node->physical_address = main_node_memory;
	hole_node->prev_sub_node = process_node;
	hole_node->next_sub_node = NULL;

	new_other_address_memory_node = mmap(NULL, PAGE_SIZE * (((int)sizeof(struct Address *)) / PAGE_SIZE + 1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	other_address_memory_ptr = (int*)new_other_address_memory_node;
	other_address_memory_end = other_address_memory_ptr + PAGE_SIZE * (((int)sizeof(struct Address *)) / PAGE_SIZE + 1);


	new_address_node = (struct Address *) other_address_memory_ptr;
	other_address_memory_ptr += sizeof(struct Address *);

	tail_address_node->next_address_node = new_address_node;
	new_address_node->prev_address_node = tail_address_node;
	tail_address_node = new_address_node;
	new_address_node->next_address_node = NULL;

	tail_address_node->virtual_address = cur_virtual_address;
	tail_address_node->physical_address = main_node_memory;
	tail_address_node->size = hole_node->size;
	tail_address_node->associated_node = hole_node;

	cur_virtual_address += hole_node->size;
	start_other_memory_flag = 0;

	return (void *) process_node->virtual_address;

}


/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/

void mems_print_stats()
{
	struct Main_Chain_Node *node = head_main_node;
	int first_sub_virtual_address = 0;
	int last_sub_virtual_address = 0;
	while (node != NULL)
	{
		struct Sub_Chain_Node *sub_node = node->head_sub_list;
		first_sub_virtual_address = sub_node->virtual_address;
		while (sub_node != NULL)
		{
			last_sub_virtual_address = sub_node->virtual_address + sub_node->size - 1;
			sub_node = sub_node->next_sub_node;
		}
		node = node->next_main_node;
	}
	printf("MAIN[%d:%d]-> ", first_sub_virtual_address, last_sub_virtual_address);
	node = head_main_node;
	while (node != NULL)
	{
		struct Sub_Chain_Node *sub_node = node->head_sub_list;
		while (sub_node != NULL)
		{
			(sub_node->type == 1) ? printf(" -> P") : printf(" -> H");
			// last_sub_virtual_address = sub_node->virtual_address;
			printf("[%d:%d] <-> ", sub_node->virtual_address, sub_node->virtual_address + sub_node->size - 1);
			sub_node = sub_node->next_sub_node;
		}
		printf("NULL\n");
		node = node->next_main_node;
	}
	node = head_main_node;
	int page_count = 0;
	int space_unused = 0;
	int main_chain_length = 0;
	while (node != NULL)
	{
		page_count += node->no_of_pages;
		main_chain_length++;
		struct Sub_Chain_Node *sub_node = node->head_sub_list;
		while (sub_node != NULL)
		{
			if(sub_node->type == 0)
			{
				space_unused += sub_node->size - 1;
			}
			sub_node = sub_node->next_sub_node;
		}
		node = node->next_main_node;
	}
	printf("Pages used : %d\n", page_count);
	printf("Space unused : %d\n", space_unused);
	printf("Main Chain Length : %d\n", main_chain_length);

	node = head_main_node;
	int sub_count = 0;
	printf("Sub-chain length array : [");
	while (node != NULL)
	{
		page_count += node->no_of_pages;
		main_chain_length++;
		struct Sub_Chain_Node *sub_node = node->head_sub_list;
		while (sub_node != NULL)
		{
			sub_node = sub_node->next_sub_node;
			sub_count++;
		}
		printf("%d, ", sub_count);
		node = node->next_main_node;
		sub_count = 0;
	}
	printf("]\n");
}


/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void *mems_get(void *v_ptr)
{
	struct Address *address = head_address_node;
	for (int i = (int)address->virtual_address; i < (int)address->virtual_address + (int)address->size; i++)
	{
		if (v_ptr >= (void *)address->virtual_address &&
			v_ptr < (void *)(address->virtual_address + address->size))
		{
			size_t offset = (size_t)v_ptr - (size_t)address->virtual_address;
			return (void *)(address->physical_address + offset);
		}
	}
	return NULL;
}

/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS) 
Returns: nothing
*/
void mems_free(void *v_ptr){
    struct Main_Chain_Node *cur_main_node = head_main_node;
    int check_virtual_address = (int) v_ptr;
    int node_found_flag = 0;

    while (cur_main_node != NULL) {
    	struct Sub_Chain_Node *cur_sub_node = cur_main_node->head_sub_list;

    	while (cur_sub_node != NULL) {
    		if (check_virtual_address >= cur_sub_node->virtual_address && check_virtual_address < cur_sub_node->virtual_address + cur_sub_node->size) {
    			node_found_flag = 1;
    			if (cur_sub_node->next_sub_node && cur_sub_node->next_sub_node->type == 0) {
    				cur_sub_node->next_sub_node->size += cur_sub_node->virtual_address + cur_sub_node->size - check_virtual_address;
    				cur_sub_node->size = check_virtual_address - cur_sub_node->virtual_address;
    			}

    			else {
    				if ((int) (other_sub_memory_end - other_sub_memory_ptr) < (int)sizeof(struct Sub_Chain_Node *)) {
						void *new_other_sub_memory_node = mmap(NULL, PAGE_SIZE * (((int)sizeof(struct Sub_Chain_Node *)) / PAGE_SIZE + 1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
						other_sub_memory_ptr = (int*)new_other_sub_memory_node;
						other_sub_memory_end = other_sub_memory_ptr + PAGE_SIZE * (((int)sizeof(struct Sub_Chain_Node *)) / PAGE_SIZE + 1);
					}

					struct Sub_Chain_Node *new_hole_node = (struct Sub_Chain_Node *) other_sub_memory_ptr;
					other_sub_memory_ptr += (int)sizeof(struct Sub_Chain_Node *);

					new_hole_node->type = 0;
					new_hole_node->size = cur_sub_node - check_virtual_address;
					cur_sub_node->size = check_virtual_address - cur_sub_node->virtual_address;
					new_hole_node->virtual_address = check_virtual_address;
					new_hole_node->physical_address = cur_sub_node->physical_address + cur_sub_node->size;
					new_hole_node->next_sub_node = cur_sub_node->next_sub_node;
					cur_sub_node->next_sub_node = new_hole_node;
					new_hole_node->prev_sub_node = cur_sub_node;
					new_hole_node->next_sub_node->prev_sub_node = new_hole_node;

    			}
    		}

    		cur_sub_node = cur_sub_node->next_sub_node;
    	}

    	cur_main_node = cur_main_node->next_main_node;
    }

    if (!node_found_flag) {
    	perror("Physical Address not appointed or already free.");
    }
}
