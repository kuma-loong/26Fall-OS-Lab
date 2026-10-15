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

// Metadata belongs to the bucket selected by blockno. Only misses take
// evict; lock order is evict -> one bucket, never the reverse. No sleeping
// lock is acquired while a metadata spinlock is held.
#define NBUCKET 13
struct {
  struct spinlock lock;
  struct buf *head;
} bucket[NBUCKET];

struct {
  struct spinlock evict;
  struct buf buf[NBUF];
  uint hand;
} bcache;

void
binit(void)
{
  initlock(&bcache.evict, "bcache.evict");
  for(int i = 0; i < NBUCKET; i++)
    initlock(&bucket[i].lock, "bcache.bucket");
  for(int i = 0; i < NBUF; i++){
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    // Unused buffers have distinct identities on the unused device 0.
    b->blockno = i;
    b->next = bucket[i % NBUCKET].head;
    bucket[i % NBUCKET].head = b;
  }
}

// Caller holds the bucket lock. A reference also pins the identity while
// waiting for the buffer's sleeping lock.
static struct buf*
blookup(uint dev, uint blockno)
{
  for(struct buf *b = bucket[blockno % NBUCKET].head; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      return b;
    }
  }
  return 0;
}

static struct buf*
bget(uint dev, uint blockno)
{
  uint h = blockno % NBUCKET;
  struct buf *b;

  acquire(&bucket[h].lock);
  b = blookup(dev, blockno);
  release(&bucket[h].lock);
  if(b){
    acquiresleep(&b->lock);
    return b;
  }

  acquire(&bcache.evict);
  // Another miss may have installed this block while we waited.
  acquire(&bucket[h].lock);
  b = blookup(dev, blockno);
  release(&bucket[h].lock);
  if(b){
    release(&bcache.evict);
    acquiresleep(&b->lock);
    return b;
  }

  // Round-robin replacement: only buffers with no users or log pins.
  // evict keeps identities stable during this scan; hits use bucket locks.
  for(int n = 0; n < NBUF; n++){
    b = &bcache.buf[bcache.hand];
    bcache.hand = (bcache.hand + 1) % NBUF;
    uint old = b->blockno % NBUCKET;
    acquire(&bucket[old].lock);
    if(b->refcnt == 0){
      struct buf **link = &bucket[old].head;
      while(*link != b)
        link = &(*link)->next;
      *link = b->next;
      // Detached and private until insertion; no bucket locks overlap.
      release(&bucket[old].lock);
      acquire(&bucket[h].lock);
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->next = bucket[h].head;
      bucket[h].head = b;
      release(&bucket[h].lock);
      release(&bcache.evict);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bucket[old].lock);
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

// A live reference keeps blockno stable through the final decrement.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);
  uint h = b->blockno % NBUCKET;
  acquire(&bucket[h].lock);
  b->refcnt--;
  release(&bucket[h].lock);
}

void
bpin(struct buf *b)
{
  uint h = b->blockno % NBUCKET;
  acquire(&bucket[h].lock);
  b->refcnt++;
  release(&bucket[h].lock);
}

void
bunpin(struct buf *b)
{
  uint h = b->blockno % NBUCKET;
  acquire(&bucket[h].lock);
  b->refcnt--;
  release(&bucket[h].lock);
}
