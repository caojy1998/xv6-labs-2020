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
#include "proc.h"

#define NBUC  13 

struct {
  struct spinlock lock[NBUC];
  struct buf buf[NBUF];
  struct buf hashbucket[NBUC];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //struct buf head;
} bcache;  //作业


void replaceBuffer(struct buf *lrubuf, uint dev, uint blockno, uint ticks){
  lrubuf->dev = dev;
  lrubuf->blockno = blockno;
  lrubuf->valid = 0;
  lrubuf->refcnt = 1;
  lrubuf->tick = ticks;
  
}


void
binit(void)
{
  struct buf *b;

  for(int i=0;i<NBUCKETS;i++){
    initlock(&bcache.lock[i], "bcache.bucket");
    // 仍然将每个bucket的头节点都指向自己
    b=&bcache.hashbucket[i];
    b->prev = b;
    b->next = b;
  }

  // Create linked list of buffers
 
  //作业，在这边加一个for循环
  
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.hashbucket[0].next;
    b->prev = &bcache.hashbucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[0].next->prev = b;
    bcache.hashbucket[0].next = b;
  }
  
 
  

}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)                     //连接工作是一个双向链表的连接
{
  struct buf *b;
  uint64 h = blockno % NBUC;
  acquire(&bcache.lock[h]);
  
  for(b = bcache.hashbucket[h].next; b != &bcache.hashbucket[h]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[h]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  int nh=(h+1)%NBUCKETS; // nh表示下一个要探索的bucket，当它重新变成h，说明所有的buffer都busy（refcnt不为0），此时panic
  while(nh!=h){          //lock lab bio.c 100行：应当从h本身开始遍历寻找空的block 101行，考虑do while循环
    acquire(&bcache.lock[nh]);// 获取当前bucket的锁
    for(b = bcache.hashbucket[nh].prev; b != &bcache.hashbucket[nh]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        // 从原来bucket的链表中断开
        b->next->prev=b->prev;
        b->prev->next=b->next;
        release(&bcache.lock[nh]);
        // 插入到blockno对应的bucket中去
        //头插法
        b->next=bcache.hashbucket[h].next;
        b->prev=&bcache.hashbucket[h];
        bcache.hashbucket[h].next->prev=b;
        bcache.hashbucket[h].next=b;
        release(&bcache.lock[h]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    // 如果当前bucket里没有找到，在转到下一个bucket之前，释放当前bucket的锁
    release(&bcache.lock[nh]);
    nh=(nh+1)%NBUCKETS;
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
brelse(struct buf *b)                          //要改的
{
  
  
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  
  uint64 num = b->blockno % NBUC;

  acquire(&bcache.lock[num]);         
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[num].next;
    b->prev = &bcache.hashbucket[num];
    bcache.hashbucket[num].next->prev = b;
    bcache.hashbucket[num].next = b;
  }
    
  release(&bcache.lock[num]);        
}

void
bpin(struct buf *b) {
  uint64 num = b->blockno % NBUC;
  acquire(&bcache.lock[num]);         
  b->refcnt++;
  release(&bcache.lock[num]);       
}

void
bunpin(struct buf *b) {
  uint64 num = b->blockno % NBUC;
  acquire(&bcache.lock[num]);       
  b->refcnt--;
  release(&bcache.lock[num]);      
}



