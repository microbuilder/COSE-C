#include <stdlib.h>
#ifdef _MSC_VER
#endif
#include <stdio.h>
#include <memory.h>
#include <assert.h>

#include <cn-cbor/cn-cbor.h>
#include <cose/cose.h>

#ifdef USE_CBOR_CONTEXT
#include "context.h"

typedef struct {
	cn_cbor_context context;
	byte *pFirst;
	int iFailLeft;
	int allocCount;
} MyContext;

typedef struct _MyItem {
	int allocNumber;
	struct _MyItem *pNext;
	size_t size;
	byte pad[4];
	byte data[4];
} MyItem;

bool CheckMemory(MyContext *pContext)
{
	MyItem *p = nullptr;
	//  Walk memory and check every block

	for (p = (MyItem *)pContext->pFirst; p != nullptr; p = p->pNext) {
		if (p->pad[0] == (byte)0xab) {
			//  Block has been freed
			for (unsigned i = 0; i < p->size + 8; i++) {
				if (p->pad[i] != (byte)0xab) {
					fprintf(stderr, "Freed block is modified");
					assert(false);
				}
			}
		}
		else if (p->pad[0] == (byte)0xef) {
			for (unsigned i = 0; i < 4; i++) {
				if ((p->pad[i] != (byte)0xef) ||
					(p->pad[i + 4 + p->size] != (byte)0xef)) {
					fprintf(stderr, "Current block was overrun");
					assert(false);
				}
			}
		}
		else {
			fprintf(stderr, "Incorrect pad value");
			assert(false);
		}
	}

	return true;
}

void *MyCalloc(size_t count, size_t size, void *context)
{
	MyItem *pb = nullptr;
	MyContext *myContext = (MyContext *)context;

	CheckMemory(myContext);

	if (myContext->iFailLeft != -1) {
		if (myContext->iFailLeft == 0) {
			return nullptr;
		}
		myContext->iFailLeft--;
	}

	pb = (MyItem *)malloc(sizeof(MyItem) + count * size);

	memset(pb, 0xef, sizeof(MyItem) + count * size);
	memset(&pb->data, 0, count * size);

	pb->pNext = (struct _MyItem *)myContext->pFirst;
	myContext->pFirst = (byte *)pb;
	pb->size = count * size;
	pb->allocNumber = myContext->allocCount++;

	return &pb->data;
}

void MyFree(void *ptr, void *context)
{
	MyItem *pb = (MyItem *)((byte *)ptr - sizeof(MyItem) + 4);
	MyContext *myContext = (MyContext *)context;
	MyItem *pItem = nullptr;

	CheckMemory(myContext);
	if (ptr == nullptr) {
		return;
	}

	for (pItem = (MyItem *)myContext->pFirst; pItem != nullptr;
		 pItem = pItem->pNext) {
		if (pItem == pb) {
			break;
		}
	}

	if (pItem == nullptr) {
		//  Not an item we allocated
		assert(false);
		return;
	}

	if (pItem->pad[0] == (byte)0xab) {
		//  already freed.
		assert(false);
	}

	memset(&pb->pad, 0xab, pb->size + 8);
}

cn_cbor_context *CreateContext(int iFailPoint)
{
	MyContext *p = (MyContext *)malloc(sizeof(MyContext));

	p->context.calloc_func = MyCalloc;
	p->context.free_func = MyFree;
	p->context.context = p;
	p->pFirst = nullptr;
	p->iFailLeft = iFailPoint;
	p->allocCount = 0;

	return &p->context;
}

void FreeContext(cn_cbor_context *pContext)
{
	MyContext *myContext = (MyContext *)pContext;
	MyItem *pItem;
	MyItem *pItem2;

	CheckMemory(myContext);

	for (pItem = (MyItem *)myContext->pFirst; pItem != nullptr;
		 pItem = pItem2) {
		pItem2 = pItem->pNext;
		free(pItem);
	}

	free(myContext);
}

int IsContextEmpty(cn_cbor_context *pContext)
{
	MyContext *myContext = (MyContext *)pContext;
	int i = 0;

	//  Walk memory and check every block

	for (MyItem *p = (MyItem *)myContext->pFirst; p != nullptr; p = p->pNext) {
		if (p->pad[0] == (byte)0xab) {
			//  Block has been freed
		}
		else {
			//  This block has not been freed
			i += 1;
		}
	}

	return i;
}

#endif	// USE_CBOR_CONTEXT
