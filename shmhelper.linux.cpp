#ifdef __linux__ 

#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include "shmhelper.h"

ShMHandle shm_int(const char *shmname, int messageSize, int messageCount)
{
  ShMHandle ret = { };
  
  int shmlen = sizeof(RingBuffer) + (messageCount * sizeof(Message)) + (messageSize * messageCount);
  ret.shm_handle = (unsigned long long) shm_open(shmname, O_RDWR | O_CREAT, 0600);
  if(!ret.shm_handle)
    return ret;

  ftruncate(ret.shm_handle, shmlen + 1);
  ret.rb = (RingBuffer *) mmap( 0, shmlen, PROT_READ | PROT_WRITE, MAP_SHARED, ret.shm_handle, 0);
  if(!ret.rb)
  {
    close(ret.shm_handle);
	  ret.shm_handle = 0;
    return ret;
  }
  close(ret.shm_handle);

  ret.rb->_size = messageSize;
  ret.rb->_count = messageCount;
  ret.rb->_wseq = 0;
  for(int i = 0; i < MAX_NUM_READERS; ret.rb->_rseq[i++] = -1);

  /* data offsets */
  unsigned long p = sizeof(RingBuffer) + (messageCount * sizeof(Message));
  for(unsigned int i = 0; i < ret.rb->_count; i++)
  {
	  ret.rb->_buffer[i]._offset = p;
	  p += ret.rb->_size;
  }

  return ret;
}

ShMHandle shm_connect(const char *shmname, int *rIndex)
{
  ShMHandle ret = { };

  ret.shm_handle = (unsigned long long) shm_open(shmname, O_RDWR, 0600);
  if(ret.shm_handle == -1)
    return ret;

  ret.rb = (RingBuffer *) mmap( 0, sizeof(RingBuffer), PROT_READ | PROT_WRITE, MAP_SHARED, ret.shm_handle, 0 );
  if(!ret.rb)
  {
    close(ret.shm_handle);
	  ret.shm_handle = -1;
	  return ret;
  }

  int shmlen = sizeof(RingBuffer) + (ret.rb->_count * sizeof(Message)) + (ret.rb->_size * ret.rb->_count);
  munmap(ret.rb, sizeof(RingBuffer));
  ret.rb = (RingBuffer *) mmap( 0, shmlen, PROT_READ | PROT_WRITE, MAP_SHARED, ret.shm_handle, 0 );
  if(!ret.rb)
  {
	  close(ret.shm_handle);
	  ret.shm_handle = -1;
	  return ret;
  }

  // reader index
  if(rIndex)
  {
	  *rIndex = -1;
    for(int i = 0; i < MAX_NUM_READERS; i++)
    {
      if(ret.rb->_rseq[i] < 0)
	    {
	      *rIndex = i;
	      break;
	    }
    }
	  if(*rIndex < 0)
	  {
	    for(int i = 0; i < MAX_NUM_READERS; i++)
	    {
        if(ret.rb->_rseq[i] + MAX_READER_DELAY < ret.rb->_wseq)
		    {
		      *rIndex = i;
		      break;
		    }
	    }
	  }
  }

  return ret;
}

void shm_sleep(int ms)
{
  timespec tss;
  tss.tv_sec = 0;
  tss.tv_nsec = ms * 1000000;
  nanosleep(&tss, 0);
}

bool shm_write_increment(ShMHandle *h)
{
  __sync_fetch_and_add(&h->rb->_wseq, 1);
  return true;
}

bool shm_read_set(ShMHandle *h, int rIndex, long long value)
{
  __sync_lock_test_and_set(&h->rb->_rseq[rIndex], value);
  return true;
}

bool shm_close(ShMHandle *h)
{
  if(!h) return false;

  if(h->rb)
  {
	  int shmlen = sizeof(RingBuffer) + (h->rb->_count * sizeof(Message)) + (h->rb->_size * h->rb->_count);
    munmap(h->rb, shmlen);
  }

  if(h->shm_handle)
    close(h->shm_handle);
  h->shm_handle = -1;
  
  return true;
}

char * shm_getmessagedata(RingBuffer *rb, Message *message)
{
  if(!rb || !rb) return NULL;

  unsigned long long addr = (unsigned long long) rb;
  return (char *) (addr + message->_offset);
}

#endif // __linux__
