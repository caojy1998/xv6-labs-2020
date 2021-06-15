// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUCKETS][NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[NBUCKETS];
  struct spinlock bucketslock[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for (int j = 0; j < NBUCKETS; j++){
    bcache.head[j].prev = &bcache.head[j];
    bcache.head[j].next = &bcache.head[j];
    for(b = bcache.buf[j]; b < bcache.buf[j]+NBUF; b++){
      b->next = bcache.head[j].next;
      b->prev = &bcache.head[j];
      initsleeplock(&b->lock, "buffer");
      bcache.head[j].next->prev = b;
      bcache.head[j].next = b;
    }
  }
  for (int j = 0; j < NBUCKETS; j++){
    initlock(&bcache.bucketslock[j], "bcachebucketslock");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  
  uint hashcode = blockno % NBUCKETS;
  
  //acquire(&bcache.lock);
  acquire(&bcache.bucketslock[hashcode]);

  // Is the block already cached?
  for(b = bcache.head[hashcode].next; b != &bcache.head[hashcode]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucketslock[hashcode]);
      //release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head[hashcode].prev; b != &bcache.head[hashcode]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.bucketslock[hashcode]);
      //release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  
  uint hashcode = b->blockno % NBUCKETS;
  
  releasesleep(&b->lock);

  //acquire(&bcache.lock);
  acquire(&bcache.bucketslock[hashcode]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[hashcode].next;
    b->prev = &bcache.head[hashcode];
    bcache.head[hashcode].next->prev = b;
    bcache.head[hashcode].next = b;
  }
  
  //release(&bcache.lock);
  release(&bcache.bucketslock[hashcode]);
}

void
bpin(struct buf *b) {
  uint hashcode = b->blockno % NBUCKETS;
  
  //acquire(&bcache.lock);
  acquire(&bcache.bucketslock[hashcode]);
  b->refcnt++;
  //release(&bcache.lock);
  release(&bcache.bucketslock[hashcode]);
}

void
bunpin(struct buf *b) {
  uint hashcode = b->blockno % NBUCKETS;

  //acquire(&bcache.lock);
  acquire(&bcache.bucketslock[hashcode]);
  b->refcnt--;
  //release(&bcache.lock);
  release(&bcache.bucketslock[hashcode]);
}


