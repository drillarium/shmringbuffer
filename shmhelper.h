#ifndef SM_HELPER_INCLUDE
#define SM_HELPER_INCLUDE

/*
 * Based on https://stackoverflow.com/questions/16283517/single-producer-consumer-ring-buffer-in-shared-memory
 * gcc -I./ -o shmringbuffer shmringbuffer.cpp shmhelper.linux.cpp -lrt
 */

#define MAX_NUM_READERS 256
#define MAX_READER_DELAY 250            // in frames reader not present timeout

struct Message
{
  unsigned long long _id;
  unsigned long _offset;	            // offset from RingBuffer start
};

struct RingBuffer
{
  unsigned int _size;
  unsigned int _count;

  long long _rseq[MAX_NUM_READERS];		// 128 readers
  long long _wseq;						// 1 writer

  /* always last member */
  Message _buffer[];
};

struct ShMHandle
{
  unsigned long long shm_handle;
  RingBuffer *rb;
};

ShMHandle shm_int(const char *shmname, int messageSize, int messageCount);
ShMHandle shm_connect(const char *shmname, int *rIndex);
void shm_sleep(int ms);
bool shm_write_increment(ShMHandle *h);
bool shm_read_set(ShMHandle *h, int rIndex, long long value);
bool shm_close(ShMHandle *h);
char * shm_getmessagedata(RingBuffer *rb, Message *message);

#endif // SM_HELPER_INCLUDE
