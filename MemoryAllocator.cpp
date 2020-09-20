#include <stdlib.h>
#include <cstdint>
#include <cassert>
#include <string>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <iostream>
using namespace std;

#define CLASSES_AMOUNT 4
#define PAGE_SIZE 256
#define HEADER_SIZE 1
#define MEMORY_PART_SIZE 2048
#define PAGES_AMOUNT MEMORY_PART_SIZE / PAGE_SIZE

class MemoryAllocator
{
private:

	enum class pageState {
		free,
		blockFilled,
		multipageBlockFilled
	};

	struct Describer {
		pageState state;
		int amount;
		int classSize;
		int* firstFreeBlock;
	};

	uint16_t memPart[MEMORY_PART_SIZE]; // The taken part of memory.
	vector<void*> freePages; // The array of free pages pointers.
	map<void*, Describer> pageDescrPtrs; // The map of page descriptors pointers.
	unordered_map<int, vector<void*>> freeClassBlocks; // The map containing a vector of free block pointers for each block class.

	// Fills the vector of free pages pointers.
	void FillFreePages();

	// Fills the vector of free pages pointers.
	void FillPagesDescrPtrs();

	// Fills the chosen page with the blocks of the chosen classSize.
	void FillPageWithBlocks(void* page, size_t classSize);

	// Fills the map of the class free blocks pointers.
	void FillFreeClassBlocks();

	// Gets the closest satisfying criterias class size.
	int GetClosestClass(size_t size, const bool& inpageBlock);

	// Gets the class size free block
	void* GetFreeClassBlock(int classSize);

	// Gets the lowest amount of pages able to contain the 'size'.
	void* GetPages(size_t size);

	// Gets the pointer of the first page in the continuous sequence of free pages with 'pagesAmount' pages.
	void* GetConsecutiveFreePages(size_t pagesAmount);

	// Does all actions to mark the block as taken: changing the page describer, the free blocks and pages pointers lists.
	void FlagBlockAsTaken(void* block);

	// Does all actions to mark the pages as taken: changing the pages describer, the free pages pointers lists.
	void FlagPagesAsTaken(void* page, int size, int pagesTaken);

	// Does all actions to mark the page as free: changes the pages describer, the free pages pointers list.
	void FlagPageAsFree(void* page);

	// Creates a new page with the blocks of the chosen classSize.
	bool CreateClassBlock(size_t classSize);

	// Copies data from oldMem to NewMem fully/partly (newMem bigger/lesser than oldMem).
	void CopyMemory(void* oldMem, void* newMem);

	// Resets the flags of the block to pageState::free.
	void ResetFlags(void* addr);

public:
	MemoryAllocator();

	// Returns the pointer to the start of the allocated memory. If the memory is unable to allocate, returns nullptr.
	void* mem_alloc(size_t size);
	void* mem_realloc(void* addr, size_t size);
	void mem_free(void* addr);
	void mem_dump();
};

MemoryAllocator::MemoryAllocator()
{
	FillFreePages();
	FillPagesDescrPtrs();
	FillFreeClassBlocks();
}
void MemoryAllocator::FillFreePages()
{
	int* tempMemPtr = (int*)memPart;

	for (size_t i = 0; i < PAGES_AMOUNT; i++)
	{
		freePages.push_back(tempMemPtr);

		if (i == PAGES_AMOUNT - 1) break;
		tempMemPtr += (int)PAGE_SIZE;
	}
}
void MemoryAllocator::FillFreeClassBlocks()
{
	int tempSize = 16;

	for (int i = 0; i < CLASSES_AMOUNT; i++)
	{
		freeClassBlocks.insert({ tempSize, {} });
		tempSize *= 2;
	}
}
void MemoryAllocator::FillPagesDescrPtrs()
{
	for (const auto& page : freePages)
	{
		pageDescrPtrs.insert({ page, {pageState::free, 0, 0, nullptr} });
	}
}
void MemoryAllocator::FillPageWithBlocks(void* page, size_t classSize)
{
	pageDescrPtrs[page].state = pageState::blockFilled;
	pageDescrPtrs[page].amount = PAGE_SIZE / classSize;
	pageDescrPtrs[page].classSize = classSize;
	pageDescrPtrs[page].firstFreeBlock = (int*)((int*)page + (int)HEADER_SIZE);

	freeClassBlocks[classSize].push_back(page);

	int* tempPage = (int*)page;
	int amount = PAGE_SIZE / classSize;

	// setting the links to the next free block in the page
	for (int i = 0; i < amount; i++)
	{
		if (i != amount - 1)
		{
			int* nextHeaderAddress = new int((int)(&(*((int*)tempPage + (int)classSize))));
			int* nextBlockAddress = new int((int)(&(*((int*)tempPage + (int)classSize + (int)HEADER_SIZE))));

			int* b = (int*)(int)*nextBlockAddress;
			int* h = (int*)(int)*nextHeaderAddress;

			memcpy(tempPage, nextBlockAddress, sizeof(nextBlockAddress));
			int* c = (int*)*tempPage;
			tempPage += (int)classSize;
		}
		else
		{
			int* b = (int*)(&(*pageDescrPtrs[page].firstFreeBlock));
			*tempPage = (int)(&(*pageDescrPtrs[page].firstFreeBlock));

			//int* a = (&(*((int*)page + (int)HEADER_SIZE)));
			//cout << a << endl;
			//int b = (int)a;
			//int* c = (int*)b;
		}
	}

	freePages.erase(find(freePages.begin(), freePages.end(), page));
}
int MemoryAllocator::GetClosestClass(size_t size, const bool& inpageBlock)
{
	assert(size > 0);

	if (inpageBlock)
	{
		for (int i = 16; i <= PAGE_SIZE / 2; i *= 2)
		{
			if (size <= i) return i;
			if (i == PAGE_SIZE / 2) break;
		}
	}
	else return size - 1;

	return size;
}
void* MemoryAllocator::GetFreeClassBlock(int classSize)
{
	// finds the vector of pointers to the free blocks of the chosen class
	auto search = freeClassBlocks.find(classSize);
	int* result = nullptr;

	if (search != freeClassBlocks.end())
	{
		if (!search->second.empty())
		{
			result = pageDescrPtrs[search->second.front()].firstFreeBlock;
			FlagBlockAsTaken(result);
		}
		else
		{
			if (CreateClassBlock(classSize))
			{
				result = pageDescrPtrs[(freeClassBlocks.find(classSize)->second.front())].firstFreeBlock;
				FlagBlockAsTaken(result);
			}
		}

		return result;
	}

	throw(exception("Unable to find vector of pointers to the free blocks of the chosen class with size == " + classSize));
}
void* MemoryAllocator::GetConsecutiveFreePages(size_t pagesAmount)
{
	if (pagesAmount == 1) return *freePages.begin();

	int consecutiveFor = 1;
	void* result = nullptr;

	if (freePages.size() >= pagesAmount)
	{
		for (auto i = freePages.begin(); i != freePages.end(); i++)
		{
			if (i != freePages.begin())
			{
				int consecutiveForTemp = consecutiveFor;
				if ((int*)*prev(i) + PAGE_SIZE == (int*)*i) consecutiveFor++;

				if (consecutiveFor == pagesAmount)
				{
					result = (int*)*i - (int)PAGE_SIZE * ((int)pagesAmount - 1);
				}

				consecutiveFor = consecutiveForTemp == consecutiveFor ? 1 : consecutiveFor;
			}
		}
	}

	return result;
}
void MemoryAllocator::FlagBlockAsTaken(void* block)
{
	int pageNumber = (int)((uint16_t*)block - (uint16_t*)memPart) / PAGE_SIZE;
	uint16_t* page = (uint16_t*)memPart + pageNumber * PAGE_SIZE;

	--(pageDescrPtrs[page].amount);
	if (pageDescrPtrs[page].amount > 0)
	{
		int* tempPost = (int*)(int)*(pageDescrPtrs[page].firstFreeBlock - (int)HEADER_SIZE);

		// changing the block pointer of the page in the list of free class blocks pointers.
		pageDescrPtrs[page].firstFreeBlock = tempPost;
	}
	// deleting the page from the list of free class blocks pointers.
	else freeClassBlocks[pageDescrPtrs[page].classSize].erase(find(freeClassBlocks[pageDescrPtrs[page].classSize].begin(), freeClassBlocks[pageDescrPtrs[page].classSize].end(), page));
}
void MemoryAllocator::FlagPagesAsTaken(void* page, int size, int pagesTaken)
{
	int* tempPage = (int*)page;

	for (int i = 0; i < pagesTaken; i++)
	{
		freePages.erase(find(freePages.begin(), freePages.end(), tempPage));

		pageDescrPtrs[tempPage].state = pageState::multipageBlockFilled;
		pageDescrPtrs[tempPage].amount = pagesTaken - i;
		pageDescrPtrs[tempPage].classSize = pagesTaken;

		tempPage += PAGE_SIZE;
	}
}
void MemoryAllocator::FlagPageAsFree(void* page)
{
	pageDescrPtrs[page].state = pageState::free;
	pageDescrPtrs[page].amount = 0;
	pageDescrPtrs[page].firstFreeBlock = nullptr;
	
	if (pageDescrPtrs[page].classSize <= PAGE_SIZE / 2)
	{
		auto search = find(freeClassBlocks[pageDescrPtrs[page].classSize].begin(), freeClassBlocks[pageDescrPtrs[page].classSize].end(), page);
		if (search != freeClassBlocks[pageDescrPtrs[page].classSize].end())
		{
			freeClassBlocks[pageDescrPtrs[page].classSize].erase(search);
		}
	}
	pageDescrPtrs[page].classSize = 0;

	freePages.emplace(upper_bound(freePages.begin(), freePages.end(), page), page);
}
bool MemoryAllocator::CreateClassBlock(size_t classSize)
{
	if (freePages.empty()) return false;

	void* freePage = freePages.front();

	FillPageWithBlocks(freePage, classSize);

	return true;
}
void MemoryAllocator::CopyMemory(void* oldMem, void* newMem)
{
	int pageNumberOld = ((uint16_t*)oldMem - (uint16_t*)memPart) / PAGE_SIZE;
	uint16_t* pageOld = (uint16_t*)memPart + pageNumberOld * PAGE_SIZE;
	int oldSize = pageDescrPtrs[pageOld].classSize + (pageDescrPtrs[pageOld].state == pageState::blockFilled ? -1 : 0);

	int pageNumberNew = ((uint16_t*)newMem - (uint16_t*)memPart) / PAGE_SIZE;
	uint16_t* pageNew = (uint16_t*)memPart + pageNumberNew * PAGE_SIZE;
	int newSize = pageDescrPtrs[pageNew].classSize + (pageDescrPtrs[pageOld].state == pageState::blockFilled ? -1 : 0);

	char* j = (char*)newMem;
	int counter = 0;
	for (char* i = (char*)oldMem; counter < (newSize < oldSize ? newSize : oldSize); counter++, i++, j++)
	{
		*j = *i;
	}
}
void MemoryAllocator::ResetFlags(void* addr)
{
	int pageNumber = ((int*)addr - (int*)memPart) / (int)PAGE_SIZE;
	int* page = (int*)memPart + (int)pageNumber * (int)PAGE_SIZE;

	cout << "\nFreeing the " << pageDescrPtrs[page].classSize * (pageDescrPtrs[page].state == pageState::multipageBlockFilled ? PAGE_SIZE : 1) << " bytes sized block from ";
	if (pageDescrPtrs[page].state == pageState::blockFilled) cout << "page #" << pageNumber << ". The block number: #" << ((int*)addr - (int*)page) / pageDescrPtrs[page].classSize + 1 << endl << endl;
	else cout << "pages #" << pageNumber << " to #" << pageNumber + pageDescrPtrs[page].amount << endl << endl;

	if (pageDescrPtrs[page].state == pageState::multipageBlockFilled)
	{
		int size = pageDescrPtrs[page].amount;
		int* tempPage = (int*)page;
		for (int i = 0; i < size; i++)
		{
			FlagPageAsFree(tempPage);

			tempPage += (int)PAGE_SIZE;
		}
	}
	else
	{
		if (pageDescrPtrs[page].amount > 0)
		{
			// get a first free block
			int* tempAddr = pageDescrPtrs[page].firstFreeBlock;

			// iterate the free blocks
			for (int blockCount = 0; blockCount < pageDescrPtrs[page].amount; blockCount++)
			{
				// check the last free block
				if (blockCount == pageDescrPtrs[page].amount - 1)
				{
					*(tempAddr - (int)HEADER_SIZE) = (int)addr;
					*((int*)addr - (int)HEADER_SIZE) = (int)pageDescrPtrs[page].firstFreeBlock;

					if ((int*)addr < pageDescrPtrs[page].firstFreeBlock) pageDescrPtrs[page].firstFreeBlock = (int*)addr;
				}

				tempAddr = (int*)(int)*(tempAddr - (int)HEADER_SIZE);
			}
		}
		else
		{
			pageDescrPtrs[page].firstFreeBlock = (int*)addr;

			freeClassBlocks[pageDescrPtrs[page].classSize].push_back(page);
		}

		pageDescrPtrs[page].amount++;

		if (pageDescrPtrs[page].amount == PAGE_SIZE / pageDescrPtrs[page].classSize)
		{
			FlagPageAsFree(page);
		}
	}
}
void* MemoryAllocator::GetPages(size_t size)
{
	if (freePages.empty()) return nullptr;

	int pagesAmount = ceil((float)size / (float)PAGE_SIZE);
	void* resultPage = GetConsecutiveFreePages(pagesAmount);
	if (resultPage != nullptr) FlagPagesAsTaken(resultPage, size, pagesAmount);

	return resultPage;
}

void* MemoryAllocator::mem_alloc(size_t size)
{
	bool inpageBlock = size < PAGE_SIZE / 2;
	int classSize = GetClosestClass(size + 1, inpageBlock);

	cout << "\nTrying to allocate " << (inpageBlock ? size + 1 : size) << " bytes. Data: " << size << " bytes" << (inpageBlock ? " + header: 1 byte" : ".") << endl;
	cout << "The closest classSize is: " << classSize << endl;

	void* memPointer = inpageBlock ? GetFreeClassBlock(classSize) : GetPages(classSize);

	if (memPointer != nullptr) cout << "\nDone!\n" << endl;
	else cout << "\nFailed! Not enough memory\n" << endl;

	return memPointer;
}
void* MemoryAllocator::mem_realloc(void* addr, size_t size)
{
	void* newMem = mem_alloc(size);
	if (newMem == nullptr) return nullptr;

	CopyMemory(addr, newMem);
	ResetFlags(addr);

	addr = nullptr;
	return newMem;
}
void MemoryAllocator::mem_free(void* addr)
{
	ResetFlags(addr);

	addr = nullptr;
}
void MemoryAllocator::mem_dump()
{
	cout << "\n========================================" << endl;

	int counter = 0;
	cout << "The amount of memory pages: " << PAGES_AMOUNT << endl;
	cout << "--------------------------------------" << endl;

	// iterating the page describers
	for (const pair<void*, Describer> page : pageDescrPtrs)
	{
		++counter;
		cout << "Page #" << counter << endl;
		cout << "Address: " << page.first << "; State: " <<
			(page.second.state == pageState::free ? "FREE" : page.second.state == pageState::blockFilled ? "FILLED WITH BLOCKS" : "PART OF A MULTIPAGE BLOCK") << endl;

		if (page.second.state == pageState::blockFilled)
		{
			cout << "The classSize of the blocks: " << pageDescrPtrs[page.first].classSize << endl;
			cout << "The total amount of blocks: " << PAGE_SIZE / pageDescrPtrs[page.first].classSize << endl;
			cout << "From those: TAKEN: " << PAGE_SIZE / pageDescrPtrs[page.first].classSize - pageDescrPtrs[page.first].amount;
			cout << "; FREE: " << pageDescrPtrs[page.first].amount << endl;

			cout << endl;

			int* tempAddr = pageDescrPtrs[page.first].firstFreeBlock;
			unordered_set<int*> freeBlocks;

			// create a vector of addresses of the free blocks
			for (int blockCounter = 0; blockCounter < pageDescrPtrs[page.first].amount; blockCounter++)
			{
				freeBlocks.insert(tempAddr);

				tempAddr = (int*)(int)*(tempAddr - (int)HEADER_SIZE);
			}

			int blockCounter = 0;
			for (int* i = (int*)page.first + (int)HEADER_SIZE; i < (int*)page.first + (int)PAGE_SIZE; i += (int)pageDescrPtrs[page.first].classSize)
			{
				++blockCounter;

				bool taken = freeBlocks.find(i) == freeBlocks.end();
				cout << "Block #" << setw(2) << blockCounter << ". Address: " << i << ". Data: " << *i << ". " << (taken ? "TAKEN" : "FREE") << endl;
			}
		}
		else if (page.second.state == pageState::multipageBlockFilled)
		{
			int pagesAmount = pageDescrPtrs[page.first].classSize;

			if (pagesAmount == pageDescrPtrs[page.first].amount)
			{
				cout << "This block is " << pagesAmount << " pages long" << endl;
			}

			if (pagesAmount == 1) cout << "This block is a fullpage block" << endl;
			else cout << "This page is a part #" << pagesAmount - pageDescrPtrs[page.first].amount + 1 << " of a multipage block" << endl;
		}

		cout << "--------------------------------------" << endl;
	}

	cout << "========================================\n" << endl;
}

void Test()
{
	MemoryAllocator allocator;

	allocator.mem_dump();
	void* a = allocator.mem_alloc(15);
	*(int*)a = 6;
	void* b = allocator.mem_alloc(14);
	*(int*)b = 7;
	allocator.mem_dump();
	void* c = allocator.mem_alloc(30);
	allocator.mem_dump();
	void* d = allocator.mem_alloc(60);
	allocator.mem_dump();
	void* e = allocator.mem_alloc(162);
	allocator.mem_dump();
	void* f = allocator.mem_alloc(800);
	allocator.mem_dump();
	allocator.mem_free(a);
	allocator.mem_dump();
	a = allocator.mem_alloc(8);
	allocator.mem_dump();
	allocator.mem_free(d);
	allocator.mem_dump();
	allocator.mem_free(f);
	allocator.mem_dump();
	e = allocator.mem_realloc(e, 14);
	*(int*)e = 9;
	b = allocator.mem_realloc(b, 14);
	allocator.mem_dump();
}

int main()
{
	Test();

	//TODO page data in multipage blocks

	return 0;
}