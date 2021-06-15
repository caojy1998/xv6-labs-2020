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
  struct run *freelist;   //用一个指针数组来代替一个指针
} kmem[NCPU];

void
kinit()
{
  initlock(&kmem[NCPU].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);  //貌似这边已经做到了give all free memory to the CPU running freerange  
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  
  push_off();            //*
  int numcpu = cpuid();  //* 

  acquire(&kmem[numcpu].lock);
  
  r->next = kmem[numcpu].freelist;   //*
  kmem[numcpu].freelist = r;
  release(&kmem[numcpu].lock);
  pop_off();             //*
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();            //*
  int numcpu = cpuid();  //*
 

  acquire(&kmem[numcpu].lock);
  
  r = kmem[numcpu].freelist;  //*
  if(r)
    kmem[numcpu].freelist = r->next;  //*
  release(&kmem[numcpu].lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  else{                          //分配其他CPU的空间给这个CPU
    for (int j=0;j<NCPU;j++){
      if (j!=numcpu){
        acquire(&kmem[j].lock);
  
        r = kmem[j].freelist;  //*
        if(r){
          kmem[j].freelist = r->next;  //*
          }
        release(&kmem[j].lock);

      if(r){
        memset((char*)r, 5, PGSIZE); // fill with junk
        break;
        }
      }
    }
  }
  pop_off();             //*
  return (void*)r;
}
