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
                   
int reference_count[PHYSTOP >> 12];
struct spinlock ref_cnt_lock;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref_cnt_lock, "ref_cnt");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    reference_count[(uint64)p >> 12] = 1;
    kfree(p);
  }
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
  

  r = (struct run*)pa;
  
  reference_count[(uint64)pa>>12]--;   //目前将改变数组的操作放在这边 
  if (reference_count[(uint64)pa>>12] == 0){
    memset(pa, 1, PGSIZE);
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
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
  if(r){
    kmem.freelist = r->next;
    reference_count[(uint64)r>>12] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


/*reference_count[(uint64)pa>>12]--;   //目前将改变数组的操作放在这边 
1、load内存地址(mem)的数到寄存器(reg)
2、将reg包含的数减1
3、store，将reg中的数写回mem
if (reference_count[(uint64)pa>>12] == 0){
4、load mem的数到寄存器reg2
5、比较reg2是否为0
步骤4被优化
偏序关系：1<2  2<3 2<5    1235 or 1253

假设是1235顺序
假设两个process A B分别由CPU A B同时执行，假设mem存储的数是2
假设顺序A1 B1 A2 A3 A5 B2 B3 B5
需要保证B3<A1 or A3<B1，实际中用lock来达成更强的约束B5<A1 or A5<B1

line 65 之前 acquire lock，最安全的做法是在72行之后release lock

__sync_synchronize();保证编译器在翻译指令的时候，不将后面的指令翻译到前面*/
