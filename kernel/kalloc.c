// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint refcount[PHYSTOP / PGSIZE];
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kmem.refcount[(uint64)p >> 12] = 0;
    incr_refcount(p);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  decr_refcount(pa);
  if (get_refcount(pa) >= 1)
    return;

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);


  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    kmem.refcount[(uint64)r >> 12] = 0;
  }
  release(&kmem.lock);

  if(r) {
    incr_refcount(r);
    memset((char*)r, 5, PGSIZE); // fill with junk
  }

  return (void*)r;
}

uint 
get_refcount(void *pa)
{
  int index = (uint64)pa >> 12;

  acquire(&kmem.lock);
  uint result = kmem.refcount[index];
  release(&kmem.lock);

  return result;
}

void
incr_refcount(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("incr_refcount");

  int index = (uint64)pa >> 12; // pa / 4096

  acquire(&kmem.lock);
  kmem.refcount[index]++;
  release(&kmem.lock);
}

void
decr_refcount(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("decr_refcount");

  int index = (uint64)pa >> 12;

  acquire(&kmem.lock);
  if (kmem.refcount[index] == 0)
    panic("decr_refcount: no reference on pa");

  kmem.refcount[index]--;
  release(&kmem.lock);
}
