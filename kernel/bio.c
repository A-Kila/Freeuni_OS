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
  struct spinlock locks[NBUCKETS];
  struct buf buf[NBUF];

  struct buf buf_table[NBUCKETS];
} bcache;

void
binit(void)
{
  struct buf *b;

  for (int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.locks[i], "bcache");
    bcache.buf_table[i].next = &bcache.buf_table[i];
    bcache.buf_table[i].prev = &bcache.buf_table[i];
  }

  for (b = bcache.buf; b < bcache.buf+NBUF; b++) {
    b->next = bcache.buf_table[0].next;
    b->prev = &bcache.buf_table[0];
    initsleeplock(&b->lock, "buffer");
    bcache.buf_table[0].next->prev = b;
    bcache.buf_table[0].next = b;
  }
}

void lock_two_buckets(int a, int b)
{
  if (a < b) {
    acquire(&bcache.locks[a]);
    acquire(&bcache.locks[b]);
  } else if (a == b) {
    acquire(&bcache.locks[a]);
  } else {
    acquire(&bcache.locks[b]);
    acquire(&bcache.locks[a]);
  }
}

void release_two_buckets(int a, int b)
{
  if (a < b) {
    release(&bcache.locks[b]);
    release(&bcache.locks[a]);
  } else if (a == b) {
    release(&bcache.locks[a]);
  } else {
    release(&bcache.locks[a]);
    release(&bcache.locks[b]);
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bucket_num = get_bucket(blockno);
  struct spinlock *lock = &bcache.locks[bucket_num];

  acquire(lock);

  // Is the block already cached?
  struct buf *bucket = &bcache.buf_table[bucket_num];
  for(b = bucket->next; b != bucket; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(lock);

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf *head;
  for(int i = 0; i < NBUCKETS; ++i) {
    lock_two_buckets(bucket_num, i);
    head = &bcache.buf_table[i];

    for(b = head->prev; b != head; b = b->prev) {
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // take out of LRU list
        b->next->prev = b->prev;
        b->prev->next = b->next;

        // put at front of LRU list
        b->next = bucket->next;
        b->prev = bucket;
        bucket->next->prev = b;
        bucket->next = b;
        release_two_buckets(bucket_num, i);
        acquiresleep(&b->lock);
        return b;
      }
    }
    
    release_two_buckets(bucket_num, i);
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

  releasesleep(&b->lock);

  struct spinlock *lock = &bcache.locks[get_bucket(b->blockno)];
  acquire(lock);

  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    struct buf *bucket = &bcache.buf_table[get_bucket(b->blockno)];

    // take out of LRU list
    b->next->prev = b->prev;
    b->prev->next = b->next;

    // put at back of LRU list
    b->next = bucket;
    b->prev = bucket->prev;
    bucket->prev->next = b;
    bucket->prev = b;
  }
  
  release(lock);
}

void
bpin(struct buf *b) {
  struct spinlock *lock = &bcache.locks[get_bucket(b->blockno)];

  acquire(lock);
  b->refcnt++;
  release(lock);
}

void
bunpin(struct buf *b) {
  struct spinlock *lock = &bcache.locks[get_bucket(b->blockno)];

  acquire(lock);
  b->refcnt--;
  release(lock);
}
