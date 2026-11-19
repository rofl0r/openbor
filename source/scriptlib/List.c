/*
 * List library.
 * 90% (C) 2011 anallyst
 * 10% (C) unknown author
 *
 * license to use and redistribute is granted under the same terms as OpenBOR itself.
 * as long as anallyst is granted to use and redistribute the OpenBOR source code,
 * and maintain his own fork.
 * 
 * the whole smart stuff going on here was written by anallyst.
 * at the time anallyst touched this code, this was a very simple single linked list.
 * double linked list functionality is (C) anallyst.
 * 
 */

#include "List.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG
static void chklist(const List *list) {
	assert(list);
	assert(list->initdone == 1);
}
#endif

#define HASH_BUCKET_COUNT 256

static unsigned char strhash(const char *s) {
	ptrdiff_t hash = 0;
	while(*s)
		hash += *s++ - 'A';
	return (size_t) hash % HASH_BUCKET_COUNT;
}

#define BUCKET_INIT_NODES_ALLOC ((size_t) 2)
static void List_AddHash(List *list, Node *node) {
#ifdef DEBUG
	chklist(list);
#endif
	assert(node);
	if(!node->name)
		return;

	if(!list->buckets) {
		list->buckets = calloc(HASH_BUCKET_COUNT, sizeof *list->buckets);
		assert(list->buckets);
	}

	const unsigned char bucket_index = strhash(node->name);
	Bucket *bucket = list->buckets[bucket_index];
	if(!bucket) {
		bucket = calloc(1, sizeof *bucket);
		assert(bucket);
		list->buckets[bucket_index] = bucket;
		bucket->nodes = calloc(BUCKET_INIT_NODES_ALLOC, sizeof *bucket->nodes);
		assert(bucket->nodes);
		bucket->size = BUCKET_INIT_NODES_ALLOC;
	}

	const size_t capacity = bucket->size;
	assert(capacity > 0);
	assert(bucket->used <= capacity);
	if(bucket->used == capacity) {
		Node **nodes = realloc(bucket->nodes, sizeof *bucket->nodes * (capacity * 2));
		assert(nodes);
		bucket->nodes = nodes;
		bucket->size = capacity * 2;
	}

	assert(bucket->used < bucket->size);
	bucket->nodes[bucket->used++] = node;
}

static void List_RemoveHash(List *list, const Node *node) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!node->name)
		return;
	assert(list->buckets);
	const unsigned char bucket_index = strhash(node->name);
	Bucket *bucket = list->buckets[bucket_index];
	assert(bucket);
	assert(bucket->used > 0);
	for(size_t node_index = 0; node_index < bucket->used; node_index++) {
		if(node == bucket->nodes[node_index]) {
			bucket->nodes[node_index] = NULL;
			break;
		}
	}
}

static void List_FreeHashes(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!list->buckets)
		return;

	for(size_t bucket_index = 0; bucket_index < HASH_BUCKET_COUNT; bucket_index++) {
		Bucket *bucket = list->buckets[bucket_index];
		if(bucket) {
			free(bucket->nodes);
			free(bucket);
		}
	}
	free(list->buckets);
	list->buckets = NULL;
}

int List_GotoFirst(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!list->size)
		return 0;
	assert(list->first);
	list->current = list->first;
	return 1;
}

Node *List_GetCurrentNode(const List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	return list ? list->current : NULL;
}

void List_Init(List *list) {
	list->first = list->current = list->last = NULL;
	list->size = 0;
	list->buckets = NULL;
#ifdef DEBUG
	list->initdone = 1;
#endif
}


void List_Clear(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *current_node = list->first;
	while(current_node) {
		Node *next_node = current_node->next;
		free(current_node->name);
		free(current_node);
		current_node = next_node;
	}
	List_FreeHashes(list);
	List_Init(list);
}

void List_InsertAfter(List *list, void *value, char *name) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *node = malloc(sizeof *node);
	Node *current = list->current;

	assert(node);
	if(list->size)
		assert(current);
	node->value = value;
	node->name = name ? strdup(name) : NULL;

	List_AddHash(list, node);

	if(!list->size) {
		node->prev = NULL;
		node->next = NULL;
		list->current = list->first = list->last = node;
	} else {
		node->next = current->next;
		node->prev = current;
		if(current->next)
			current->next->prev = node;
		current->next = node;
		if(current == list->last)
			list->last = node;
		list->current = node;
	}
	list->size++;
}

void List_Remove(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *current_node = list->current;
	if(!list->size)
		return;
	assert(current_node);
	assert(list->first);
	assert(list->last);
	if(list->size == 1) {
		List_RemoveHash(list, current_node);
		free(current_node->name);
		free(current_node);
		list->first = list->current = list->last = NULL;
	} else {
		const int was_last = current_node == list->last;
		const int was_first = current_node == list->first;
		if(current_node->prev)
			current_node->prev->next = current_node->next;
		if(current_node->next)
			current_node->next->prev = current_node->prev;
		Node *next_current = was_last ? current_node->prev : current_node->next;

		List_RemoveHash(list, current_node);

		free(current_node->name);
		free(current_node);

		if(was_last)
			list->last = next_current;
		if(was_first)
			list->first = next_current;
		list->current = next_current;
	}
	list->size--;
}

int List_GotoNext(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	if(list->current == list->last)
		return 0;
	assert(list->current);
	assert(list->current->next);
	list->current = list->current->next;
	return 1;
}

Node *List_GetNodeByName(const List *list, const char *name) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!name || !list->buckets)
		return NULL;

	const unsigned char bucket_index = strhash(name);
	const Bucket *bucket = list->buckets[bucket_index];
	if(!bucket)
		return NULL;
	assert(bucket->used <= bucket->size);
	assert(bucket->nodes);
	for(size_t node_index = 0; node_index < bucket->used; node_index++) {
		Node *node = bucket->nodes[node_index];
		if(node) {
			assert(node->name);
			if(strcmp(name, node->name) == 0)
				return node;
		}
	}
	return NULL;
}

int List_FindByName(List *list, const char *name) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *node = List_GetNodeByName(list, name);
	if(!node)
		return 0;
	list->current = node;
	return 1;
}
