#include<dlfcn.h>
#include<stdio.h>
#include<stdlib.h>
#include<ctype.h>
#include<pthread.h>
#include<string.h>
#include<assert.h>

typedef struct allocation_list_s {
	struct allocation_list_s* next;
	size_t size;
	void* pointer;
} allocation_t;

static struct {
	bool isInitialized;
	void* (*std_malloc)(size_t);
	void* (*std_realloc)(void*, size_t);
	void (*std_free)(void*);
	char* failures;
	allocation_t* allocations;
	pthread_mutex_t mutex;
} interceptor;

int qInterceptorAllocationCount = 0;
size_t qInterceptorAllocatedSize = 0;

static void report() {
	printf("-- qInterceptor --\n[%zu bytes in %d allocations]\n", qInterceptorAllocatedSize, qInterceptorAllocationCount);
	allocation_t* allocation = interceptor.allocations;
	while(allocation != NULL) {
		char string[51];
		memset(string, 0, 51);
		size_t size = allocation -> size;
		if(size > 50) size = 50;
		memcpy(string, allocation -> pointer, size);
		int index = 0;
		while(index < size) {
			if(!isprint(string[index]))
				string[index] = '.';
			index++;
		}
		printf("%zu: %s\n", allocation -> size, string);
		allocation_t* temp = allocation;
		allocation = allocation -> next;
		interceptor.std_free(temp);
	}
	pthread_mutex_destroy(&interceptor.mutex);
}

static void initialize() {
	interceptor.isInitialized = true;
	interceptor.std_malloc = dlsym(RTLD_NEXT, "malloc");
	interceptor.std_realloc = dlsym(RTLD_NEXT, "realloc");
	interceptor.std_free = dlsym(RTLD_NEXT, "free");
	interceptor.failures = getenv("ALLOCATION_FAILURES");
	interceptor.allocations = NULL;
	pthread_mutex_init(&interceptor.mutex, NULL);
	if(interceptor.failures == NULL)
		interceptor.failures = "";
	atexit(report);
}

void* malloc(size_t size) {
	pthread_mutex_lock(&interceptor.mutex);
	if(!interceptor.isInitialized)
		initialize();
	if(*interceptor.failures != '\0') {
		if(*interceptor.failures == '1') {
			interceptor.failures++;
			pthread_mutex_unlock(&interceptor.mutex);
			return NULL;
		}
		interceptor.failures++;
	}
	allocation_t* allocation = interceptor.std_malloc(sizeof(allocation_t));
	allocation -> next = interceptor.allocations;
	allocation -> size = size;
	allocation -> pointer = interceptor.std_malloc(size);
	interceptor.allocations = allocation;
	qInterceptorAllocationCount++;
	qInterceptorAllocatedSize += size;
	pthread_mutex_unlock(&interceptor.mutex);
	return allocation -> pointer;
}

void free(void* pointer) {
	if(pointer == NULL)
		return;
	pthread_mutex_lock(&interceptor.mutex);
	if(!interceptor.isInitialized)
		initialize();
	allocation_t* allocation = interceptor.allocations;
	if(allocation != NULL) {
		if(allocation -> pointer == pointer) {
			qInterceptorAllocatedSize -= allocation -> size;
			interceptor.allocations = allocation -> next;
			interceptor.std_free(allocation);
		} else {
			allocation_t* temp = allocation -> next;
			while(temp != NULL) {
				if(temp -> pointer == pointer) {
					qInterceptorAllocatedSize -= temp -> size;
					allocation -> next = temp -> next;
					interceptor.std_free(temp);
					break;
				}
				allocation = temp;
				temp = allocation -> next;
			}
		}
	}
	qInterceptorAllocationCount--;
	interceptor.std_free(pointer);
	pthread_mutex_unlock(&interceptor.mutex);
}

void* realloc(void* pointer, size_t size) {
	if(pointer == NULL)
		return malloc(size);
	if(size == 0) {
		free(pointer);
		return NULL;
	}
	pthread_mutex_lock(&interceptor.mutex);
	if(!interceptor.isInitialized)
		initialize();
	if(*interceptor.failures != '\0') {
		if(*interceptor.failures == '1') {
			interceptor.failures++;
			pthread_mutex_unlock(&interceptor.mutex);
			return NULL;
		}
		interceptor.failures++;
	}
	allocation_t* allocation = interceptor.allocations;
	while(allocation != NULL) {
		if(allocation -> pointer == pointer) {
			qInterceptorAllocatedSize -= allocation -> size;
			qInterceptorAllocatedSize += size;
			allocation -> size = size;
			allocation -> pointer = interceptor.std_realloc(pointer, size);
			pthread_mutex_unlock(&interceptor.mutex);
			return allocation -> pointer;
		}
		allocation = allocation -> next;
	}
	pthread_mutex_unlock(&interceptor.mutex);
	return NULL;
}

void* calloc(size_t count, size_t size) {
	void* pointer = malloc(count * size);
	if(pointer == NULL)
		return NULL;
	memset(pointer, 0, count * size);
	return pointer;
}