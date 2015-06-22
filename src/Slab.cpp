#include "Slab.h"
#include "Options.h"
#include "Logger.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SLAB_PAGE_MASK  3
#define SLAB_PAGE        0
#define SLAB_BIG         1
#define SLAB_EXACT       2
#define SLAB_SMALL       3
//#define PTR_SIZE         sizeof(unsigned long)

#ifdef __i386__

#define SLAB_PAGE_FREE   0
#define SLAB_PAGE_BUSY   0xffffffff
#define SLAB_PAGE_START  0x80000000

#define SLAB_SHIFT_MASK  0x0000000f
#define SLAB_MAP_MASK    0xffff0000
#define SLAB_MAP_SHIFT   16

#define SLAB_BUSY        0xffffffff

#else /* (PTR_SIZE == 8) */

#define SLAB_PAGE_FREE   0
#define SLAB_PAGE_BUSY   0xffffffffffffffff
#define SLAB_PAGE_START  0x8000000000000000

#define SLAB_SHIFT_MASK  0x000000000000000f
#define SLAB_MAP_MASK    0xffffffff00000000
#define SLAB_MAP_SHIFT   32

#define SLAB_BUSY        0xffffffffffffffff

#endif

#define alignPtr(p, a) \
            (char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

using namespace bt;

Slab::Slab()
    : minSize_(0),
      minShift_(3),
      pages_(0),
      last_(0),
      free_(),
      start_(0),
      end_(0),
      addr_(0),
      maxSize_(0),
      exactSize_(0),
      exactShift_(0),
      pageSize_(PAGE_SIZE),
      pageShift_(0),
      realPages_(0)
{

}

Slab::~Slab()
{
    free(addr_);
}

bool Slab::init(uint32_t slabSize)
{
    uint32_t n;

    // pageShift_ 12
    for(n = pageSize_, pageShift_ = 0; n >>= 1; pageShift_++)
        ;

    //maxSize = pageSize_ / 2; // 2K
    exactSize_ = pageSize_ / (8 * sizeof(uintptr_t)); // 2^7=128
    for(n = exactSize_; n >>= 1; exactShift_++)
        ;

    minSize_ = 1 << minShift_; // 8 byte
    char* p = (char*)malloc(slabSize);

    addr_ = p;
    end_ = p + slabSize;

    Page* slots = (Page*)p;
    n = pageShift_ - minShift_; // 9
    for(uint32_t i = 0; i < n; ++i) {
        slots[i].slab = 0;
        slots[i].next = &slots[i];
        slots[i].prev = 0;
    }

    p += n * sizeof(Page);

    uint32_t size = end_ - p;

    uint32_t pages = size / (pageSize_ + sizeof(Page));
    memset(p, 0, pages * sizeof(Page));

    pages_ = (Page*)p;

    free_.prev = 0;
    free_.next = (Page* )p;

    pages_->slab = pages;
    pages_->next = &free_;
    pages_->prev = (uintptr_t)&free_;

    start_ = (char*)alignPtr((uintptr_t)p + pages * sizeof(Page), pageSize_);
    pages_->slab = (end_ - start_) / pageSize_;

	return true;
}

void* Slab::alloc(uint32_t size)
{
    void* p = allocLocked(size);
    return p;
}

void* Slab::allocLocked(uint32_t size)
{
    size_t s;
    uintptr_t p, n, m, mask, *bitmap;
    uint32_t i, slot, shift, map;
    Page *page, *prev, *slots;

    if(size >= pageSize_ / 2) { // >= 2K
        page = allocPages((size >> pageShift_) + ((size % pageSize_) ? 1 : 0));
        if(page) {
            p = (page - pages_) << pageShift_;
            p += (uintptr_t)start_;
        } else {
            p = 0;
        }

        goto done;
    }
    if(size > minSize_) {
        shift = 1;
        for(s = size - 1; s >>= 1; shift++)
            ;
        slot = shift - minShift_;
    } else {
        size = minSize_;
        shift = minShift_;
        slot = 0;
    }

    slots = (Page*)addr_;
    page = slots[slot].next;

    if(page->next != page) {
        if(shift < exactShift_) {
            do {
                p = (page - pages_) << pageShift_;
                bitmap = (uintptr_t*) (start_ + p);

                map = (1 << (pageShift_ -  shift)) / (sizeof(uintptr_t) * 8);
                for(n = 0; n < map; n++) {
                    if(bitmap[n] != SLAB_BUSY) {
                        for(m = 1, i = 0; m; m <<= 1, i++) {
                            if(bitmap[n] & m)
                                continue;
							// set bit
                            bitmap[n] |= m;
                            i = ((n * sizeof(uintptr_t) * 8) << shift) + (i << shift);
                            if(bitmap[n] == SLAB_BUSY) {
                                for(n = n + 1; n < map; n++) {
									// find next bitmap
                                    if(bitmap[n] != SLAB_BUSY) {
                                        p = (uintptr_t)bitmap + i;
                                        goto done;
                                    }
                                }
								// all the bitmap is used, then delete current page
                                prev = (Page*)(page->prev & ~SLAB_PAGE_MASK);
                                prev->next = page->next;
                                page->next->prev = page->prev;

                                page->next = NULL;
                                page->prev = SLAB_SMALL;
                            }
                            p = (uintptr_t)bitmap + i;
                            goto done;
                        }
                    }
                }
                page = page->next;
            } while(page);
        } else if(shift == exactShift_) {
            do {
                if(page->slab != SLAB_BUSY) {
                    for(m = 1, i = 0; m; m <<= 1, i++) {
                        if((page->slab & m)) 
                            continue;
                        page->slab |= m;

                        if(page->slab == SLAB_BUSY) {
                            prev = (Page*)(page->prev & ~SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = SLAB_EXACT;
                        }

                        p = (page - pages_) << pageShift_;
                        p += i << shift;
                        p += (uintptr_t)start_;
                        goto done;
                    }
                }
                page = page->next;
            } while(page);
        } else {
            n = pageShift_ - (page->slab & SLAB_SHIFT_MASK);
            n = 1 << n;
            n = ((uintptr_t)1 << n) - 1;
            mask = n << SLAB_MAP_SHIFT;

            do {
                if((page->slab & SLAB_MAP_MASK) != mask) {
                    for(m = (uintptr_t)1 << SLAB_MAP_SHIFT, i = 0; m & mask; m <<= 1, i++) {
                        if(page->slab & m)
                            continue;

                        page->slab |= m;
                        if((page->slab & SLAB_MAP_MASK) == mask) {
                            prev = (Page*)(page->prev & ~SLAB_PAGE_MASK);
                            prev->next = page->next;
                            page->next->prev = page->prev;

                            page->next = NULL;
                            page->prev = SLAB_BIG;
                        }

                        p = (page - pages_) << pageShift_;
                        p += i << shift;
                        p += (uintptr_t)start_;

                        goto done;
                    }
                }
                page = page->next;
            } while(page);
        }
    }



    // 没有可用的page
    page = allocPages(1);
    if(page) {
        if(shift < exactShift_) {
            p = (page - pages_) << pageShift_;
            bitmap = (uintptr_t* )(start_ + p);

            s = 1 << shift;
            n = (1 << (pageShift_ - shift)) / 8 / s;

            if(n == 0)
                n = 1;

            bitmap[0] = (2 << n) - 1;

            map = (1 << (pageShift_ - shift)) / (sizeof(uintptr_t) * 8);
            for(i = 1; i < map; i++)
                bitmap[i] = 0;

            page->slab = shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t)&slots[slot] | SLAB_SMALL;

            slots[slot].next = page;

            p = ((page - pages_) << pageShift_) + s * n;
            p += (uintptr_t)start_;
            goto done;
        } else if(shift == exactShift_) {
            page->slab = 1;
            page->next = &slots[slot];
            page->prev = (uintptr_t)&slots[slot] | SLAB_EXACT;

            slots[slot].next = page;
            
            p = (page - pages_) << pageShift_;
            p += (uintptr_t)start_;
            goto done;
        } else {
            page->slab = ((uintptr_t)1 << SLAB_MAP_SHIFT) | shift;
            page->next = &slots[slot];
            page->prev = (uintptr_t)&slots[slot] | SLAB_BIG;

            slots[slot].next = page;

            p = (page - pages_) << pageShift_;
            p += (uintptr_t)start_;

            goto done;
        }
    }

    p = 0;

done:
    return (void* )p;
}

Page* Slab::allocPages(uint32_t pages)
{
    Page* page, *p;
    for(page = free_.next; page != &free_; page = page->next) {
        if(page->slab >= pages) {
            if(page->slab > pages) {
                page[pages].slab = page->slab - pages;
                page[pages].next = page->next;
                page[pages].prev = page->prev;

                p = (Page*)page->prev;
                p->next = &page[pages];
                page->next->prev = (uintptr_t)&page[pages];
            } else {
                p = (Page*)page->prev;
                p->next = page->next;
                page->next->prev = page->prev;
            }

            page->slab = pages | SLAB_PAGE_START;
            page->next = NULL;
            page->prev = SLAB_PAGE;

            if(--pages == 0) 
                return page;
        
            for(p = page + 1; pages; pages--) {
                p->slab = SLAB_PAGE_BUSY;
                p->next = NULL;
                p->prev = SLAB_PAGE;
                p++;
            }
            return page;
        }
    }

    return NULL;
}

void Slab::free(void* p)
{
    freeLocked(p);
}

void Slab::freeLocked(void* p)
{
    size_t size;
    uintptr_t slab, m, *bitmap;
    uint32_t n, type, slot, shift, map;
    Page  *slots, *page;

    if((char*)p < start_ || (char*)p > end_) {
        goto fail;
    }

    n = ((char*)p - start_) >> pageShift_;
    page = &pages_[n];
    slab = page->slab;
    type = page->prev & SLAB_PAGE_MASK;

    switch(type) {
        case SLAB_SMALL:
            shift = slab & SLAB_SHIFT_MASK;
            size = 1 << shift;

            if((uintptr_t)p & (size - 1))
                goto wrongChunk;

            n = ((uintptr_t)p & (pageSize_ - 1)) >> shift;
            m = (uintptr_t)1 << (n & (sizeof(uintptr_t) * 8 - 1));
            n /= (sizeof(uintptr_t) * 8);
            bitmap = (uintptr_t*)((uintptr_t)p & ~((uintptr_t)pageSize_ - 1));

            if(bitmap[n] & m) {
                if(page->next == NULL) {
                    slots = (Page*)addr_;
                    slot = shift - minShift_;

                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    page->prev = (uintptr_t)&slots[slot] | SLAB_SMALL;
                    page->next->prev = (uintptr_t)page | SLAB_SMALL;
                }

                bitmap[n] &= ~m;

                n = (1 << (pageSize_ - shift)) / 8 / (1 << shift);
                if(n == 0)
                    n = 1;

                if(bitmap[0] & ~(((uintptr_t)1 << n) - 1))
                    goto done;
                map = (1 << (pageSize_ - shift)) / (sizeof(uintptr_t) * 8);

                for(n = 1; n < map; n++) {
                    if(bitmap[n])
                        goto done;
                }

                freePages(page, 1);
                goto done;
            }
            goto chunkAlreadyFree;
        case SLAB_EXACT:
            m = (uintptr_t)1 << (((uintptr_t)p & (pageSize_ - 1)) >> exactShift_);
            size = exactSize_;
            if((uintptr_t)p & (size - 1))
                goto wrongChunk;
            if(slab & m) {
                if(slab == SLAB_BUSY) {
                    slots = (Page*)addr_;
                    slot = exactShift_ - minShift_;
                    page->next = slots[slot].next;
                    slots[slot].next = page;

                    page->prev = (uintptr_t)&slots[slot] | SLAB_EXACT;
                    page->next->prev = (uintptr_t)page | SLAB_EXACT;
                }
                page->slab &= ~m;
                if(page->slab)
                    goto done;
                freePages(page, 1);
                goto done;
            }
            goto chunkAlreadyFree;
        case SLAB_BIG:
            shift = slab & SLAB_SHIFT_MASK;
            size = 1 << shift;

            if((uintptr_t)p & (size - 1))
                goto wrongChunk;

            m = (uintptr_t)1 << ((((uintptr_t)p & (pageSize_ - 1)) >> shift) + SLAB_MAP_SHIFT);
            if(slab & m) {
                if(page->next == NULL) {
                    slots = (Page*)addr_;
                    slot = shift - minShift_;
                    page->next = slots[slot].next;
                    slots[slot].next = page;
                    page->prev = (uintptr_t)&slots[slot] | SLAB_BIG;
                    page->next->prev = (uintptr_t)page | SLAB_BIG;
                }
                page->slab &= ~m;
                if(page->slab & SLAB_MAP_MASK)
                    goto done;
                freePages(page, 1);
                goto done;
            }
            goto chunkAlreadyFree;
        case SLAB_PAGE:
            if((uintptr_t)p & (pageSize_ - 1))
                goto wrongChunk;
            if(slab == SLAB_PAGE_FREE)
                goto fail;
            if(slab == SLAB_PAGE_BUSY)
                goto fail;

            n = ((char*)p - start_) >> pageShift_;
            size = slab & ~SLAB_PAGE_START;
            freePages(&pages_[n], size);
            return;
    }
    // not reached
    return;
done:
    return;
wrongChunk:
    goto fail;
chunkAlreadyFree:
    goto fail;
fail:
    return;
}

void Slab::freePages(Page* page, uint32_t pages)
{
    Page *prev;
    if(pages > 1)
        memset(&page[1], 0, (pages - 1) * sizeof(Page*));

    if(page->next) {
        prev = (Page*)(page->prev & ~SLAB_PAGE_MASK);
        prev->next = page->next;
        page->next->prev = page->prev;
    }
    page->slab = pages;

    page->prev = (uintptr_t)&free_;
    page->next = free_.next;
    page->next->prev = (uintptr_t)page;
    free_.next = page;
}

void Slab::slabStat()
{
	uintptr_t m, n, mask, slab;
	uintptr_t *bitmap;
	uint32_t i, j, map, type, objSize;
	Page *page;

	size_t statTotalPages = 0, statUsedSize = 0, statSmall = 0, statSmalls = 0;
	size_t statExact = 0, statExacts = 0, statBig = 0, statBigs = 0, statPage = 0, statPages = 0;
	size_t statSlabSize = 0, statUsedPct = 0, statFreePage = 0, statMaxFreePages = 0;

	page = pages_;
 	statTotalPages = (end_ - start_) / pageSize_;

	for (i = 0; i < statTotalPages; i++)
	{
		slab = page->slab;
		type = page->prev & SLAB_PAGE_MASK;

		switch (type) {

			case SLAB_SMALL:
	
				n = (page - pages_) << pageShift_;
                bitmap = (uintptr_t *)(start_ + n);

				objSize = 1 << slab;
                map = (1 << (pageShift_ - slab)) / (sizeof(uintptr_t) * 8);

				for (j = 0; j < map; j++) {
					for (m = 1 ; m; m <<= 1) {
						if ((bitmap[j] & m)) {
							statUsedSize += objSize;
							statSmalls += objSize;
						}
					}		
				}
	
				statSmall++;

				break;

			case SLAB_EXACT:

				if (slab == SLAB_BUSY) {
					statUsedSize += sizeof(uintptr_t) * 8 * exactSize_;
					statExacts += sizeof(uintptr_t) * 8 * exactSize_;
				}
				else {
					for (m = 1; m; m <<= 1) {
						if (slab & m) {
							statUsedSize += exactSize_;
							statExacts += exactSize_;
						}
					}
				}

				statExact++;

				break;

			case SLAB_BIG:

				j = pageShift_ - (slab & SLAB_SHIFT_MASK);
				j = 1 << j;
				j = ((uintptr_t) 1 << j) - 1;
				mask = (uintptr_t)j << SLAB_MAP_SHIFT;
				objSize = 1 << (slab & SLAB_SHIFT_MASK);

				for (m = (uintptr_t) 1 << SLAB_MAP_SHIFT; m & mask; m <<= 1)
				{
					if ((page->slab & m)) {
						statUsedSize += objSize;
						statBigs += objSize;
					}
				}

				statBig++;

				break;

			case SLAB_PAGE:

				if (page->prev == SLAB_PAGE) {		
					slab 			=  slab & ~SLAB_PAGE_START;
					statUsedSize += slab * pageSize_;
					statPages += slab * pageSize_;
					statPage += slab;

					i += (slab - 1);

					break;
				}

			default:
				if (slab  > statMaxFreePages) {
					statMaxFreePages = page->slab;
				}

				statFreePage += slab;

				i += (slab - 1);

				break;
		}

		page = pages_ + i + 1;
	}

	statSlabSize = end_ - start_;
	statUsedPct = statUsedSize * 100 / statSlabSize;

	LOGFMTD("pool_size : %zu bytes",	statSlabSize);
	LOGFMTD("used_size : %zu bytes",	statUsedSize);
	LOGFMTD("used_pct  : %zu%%",		statUsedPct);

	LOGFMTD("total page count : %zu",	statTotalPages);
	LOGFMTD("free page count  : %zu",	statFreePage);
		
	LOGFMTD("small slab use page : %zu,\tbytes : %zu",	statSmall, statSmalls);	
	LOGFMTD("exact slab use page : %zu,\tbytes : %zu",	statExact, statExacts);
	LOGFMTD("big   slab use page : %zu,\tbytes : %zu",	statBig,   statBigs);	
	LOGFMTD("page slab use page  : %zu,\tbytes : %zu",	statPage,  statPages);				

	LOGFMTD("max free pages : %zu",		statMaxFreePages);
}

