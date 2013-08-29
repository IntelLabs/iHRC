//===-- HeteroCPUTransform.cpp - Generate CPU/IA code  -------------------===//
//
// Copyright (c) 2013 Intel Corporation. All rights reserved.
//                     The iHRC Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE-iHRC.TXT for details.
//
//===----------------------------------------------------------------------===
#include <new>
#include <iostream>
#include <stdlib.h>
#include <assert.h>


#define NIL (void *)1
#define TRUE 1
#define FALSE 0

// Concord APIs
#include "..\headers\hetero.h"

/* Linked List structue */
typedef struct linked_list_s {
	int key;
	struct linked_list_s *next;
}linked_list_t;

/* Generate random number */
static int random() {
    int n;
    n = rand();
    return n;
}


class LinkedListSearch {

public:
	
	int num_search_keys; // Number of keys to search in parallel
	int *search_keys; // Searching keys
	linked_list_t *list; // Linked list of keys
	int *found_keys; // Array to indicate if search_keys[i] is in linked-list or not

	LinkedListSearch(int num_search_keys_, int *search_keys_, linked_list_t *list_, int *found_keys_) : num_search_keys(num_search_keys_), search_keys(search_keys_), list(list_), found_keys (found_keys_) {  }

#ifdef _GNUC_
__attribute__((noinline))
#endif
	void operator()(int tid) const{	// Search in parallel for each tid
		int found = FALSE;
		int search_key = search_keys[tid]; // Key to search
		for(  linked_list_t *element = list;element != NIL; element = element->next) {
			if (element->key == search_key) {
				found = TRUE; // found the key
				break;
			}
		}
		found_keys[tid] = found;
		return;
	}
};


int main(int argc, char *argv[]){
	
	/* Initialize the Concord runtime */
	hetero_init();

	int num_keys = 10000;
	if (argc > 1) {
		num_keys = atoi(argv[1]);
		assert(num_keys > 0);
	}

	std::cout << "\n\nNumber of keys to search = " << num_keys << std::endl;

	int *keys = (int *)malloc(num_keys * sizeof(int));
	
	/* Allocate search_keys in shared region; Allocate search results in shared region */
    int *search_keys = (int *)malloc_shared(num_keys * sizeof(int));
	int *found_keys = (int *)malloc_shared(num_keys * sizeof(int));

	std::cout << "initialize keys...\n";
	/* Initialize linked list keys and search keys*/
    for (int i = 0;  i < num_keys;  i++) {
        keys[i]             = 1 + (int)(1000.0 * (rand()/(RAND_MAX + 1.0)));
        search_keys[i]      = 1 + (int)(1000.0 * (rand()/(RAND_MAX + 1.0)));
		found_keys[i] = 0;
    }

	std::cout << "create list...\n";
	/* Initialize linked list with keys */
	linked_list_t *list = (linked_list_t *)malloc_shared(num_keys * sizeof(linked_list_t));
	for (int i = 0;  i < num_keys;  i++) {
		list[i].key = keys[i];
		list[i].next = (i<num_keys-1) ? &list[i+1] : NULL;
	}
	
	std::cout << "start searching in gpu...\n";
	/* Create the Functor and invoke search in parallel on the GPU */
	LinkedListSearch *list_object = new (malloc_shared(sizeof(LinkedListSearch))) LinkedListSearch(num_keys, search_keys, list, found_keys);
	parallel_for_hetero(num_keys, *list_object, GPU);
	

	/* Perform search in parallel on CPU for verification */
	std::cout << "start searching in cpu...\n";
	int *cpu_found_keys = (int *)malloc(num_keys * sizeof(int));
	for (int i = 0;  i < num_keys;  i++) {
		cpu_found_keys[i] = 0;
	}
	list_object->found_keys = cpu_found_keys;
    parallel_for_hetero(num_keys, *list_object, MULTI_CORE_CPU_TBB);
	//parallel_for_hetero(num_keys, *list_object, MULTI_CORE_CPU_VEC);
	
	
	/* Verify results */
	std::cout << "Verify results:\n";
	for(int i=0;i<num_keys;i++){
		if(found_keys[i] != cpu_found_keys[i]){
			std::cout << "Error: Key=" << keys[i] << " CPU=" << cpu_found_keys[i] << " GPU=" << found_keys[i] << std::endl;
			break;
		}	
	}
	
	/* Deallocate the data structures */
	free_shared(search_keys);
	free_shared(found_keys);
	free_shared(list);
	free(keys);
	free(cpu_found_keys);

	std::cout << "Test Completed\n";
	return 0;
}
