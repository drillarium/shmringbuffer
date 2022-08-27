#ifdef _WIN32

#include <windows.h>
#include "shmhelper.h"

ShMHandle shm_int(const char *shmname, int messageSize, int messageCount)
{
  ShMHandle ret = { };
  
  int shmlen = sizeof(RingBuffer) + (messageCount * sizeof(Message)) + (messageSize * messageCount);
  ret.shm_handle = (unsigned long long) CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, shmlen, shmname);
  if(!ret.shm_handle)
    return ret;

  ret.rb = (RingBuffer *) MapViewOfFile((HANDLE) ret.shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, shmlen);
  if(!ret.rb)
  {
    CloseHandle((HANDLE) ret.shm_handle);
	  ret.shm_handle = 0;
    return ret;
  }

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

  ret.shm_handle = (unsigned long long) OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shmname);
  if(ret.shm_handle == NULL)
    return ret;

  ret.rb = (RingBuffer *) MapViewOfFile((HANDLE) ret.shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(RingBuffer));
  if(!ret.rb)
  {
    CloseHandle((HANDLE) ret.shm_handle);
	  ret.shm_handle = NULL;
	  return ret;
  }

  int shmlen = sizeof(RingBuffer) + (ret.rb->_count * sizeof(Message)) + (ret.rb->_size * ret.rb->_count);
  UnmapViewOfFile(ret.rb);
  ret.rb = (RingBuffer *) MapViewOfFile((HANDLE) ret.shm_handle, FILE_MAP_ALL_ACCESS, 0, 0, shmlen);
  if(!ret.rb)
  {
	  CloseHandle((HANDLE)ret.shm_handle);
	  ret.shm_handle = NULL;
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
  Sleep(ms);
}

bool shm_write_increment(ShMHandle *h)
{
  if(InterlockedIncrement64(&h->rb->_wseq))
    return true;
  
  return false;
}

bool shm_read_set(ShMHandle *h, int rIndex, long long value)
{
  if(InterlockedExchange64(&(h->rb->_rseq[rIndex]), value))
    return true;
	
  return false;
}

bool shm_close(ShMHandle *h)
{
  if(!h)
    return false;

  if(h->rb)
    UnmapViewOfFile(h->rb);
  h->rb = NULL;

  if(h->shm_handle)
    CloseHandle((HANDLE) h->shm_handle);
  h->shm_handle = NULL;
 
  return true;
}

char * shm_getmessagedata(RingBuffer *rb, Message *message)
{
  if (!rb || !rb) return NULL;

  unsigned long long addr = (unsigned long long) rb;
  return (char *) (addr + message->_offset);
}

#endif // _WIN32
