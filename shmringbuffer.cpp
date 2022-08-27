#include <stdio.h>
#include <string.h>
#include <ctime>
#include "shmhelper.h"

#define SHM_ID "SHM_TEST"
#define SHM_COUNT 12
#define SHM_SIZE 1024 * 1024

void producerLoop()
{
  ShMHandle h = shm_int(SHM_ID, SHM_SIZE, SHM_COUNT);
  unsigned long long i = 0;
  clock_t begin = clock();

  while(1)
  {
    // write the next entry and atomically update the write sequence number
	  Message *msg = &(h.rb->_buffer[h.rb->_wseq%SHM_COUNT]);
	  char *msgData = shm_getmessagedata(h.rb, msg);
	  msg->_id = i++;
    shm_write_increment(&h);

	  clock_t end = clock();
	  double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
	  printf("w: %llu %f\r", h.rb->_wseq, elapsed_secs>0? h.rb->_wseq / elapsed_secs : 0);

	  // give consumer some time to catch up
	  shm_sleep(10);
  }

  shm_close(&h);
}

void consumerLoop()
{
  int rIndex = -1;
  ShMHandle h = {};

  // connect
  while(1)
  {
    h = shm_connect(SHM_ID, &rIndex);
	  if(rIndex >= 0)
	    break;
	  shm_sleep(10);
  }

  // initialize our sequence numbers in the ring buffer
  long long seq = h.rb->_wseq;
  long long pid = -1;
  long long w2rdelay = SHM_COUNT >> 3;
  clock_t begin = clock();
  long long ref = seq;

  while(1)
  {
    // while there is data to consume
    if(seq < h.rb->_wseq)
	  {
      Message *msg = &(h.rb->_buffer[seq%SHM_COUNT]);
	    char *msgData = shm_getmessagedata(h.rb, msg);
      if( (pid != -1) && (msg->_id != pid + 1) )
	      printf("warning: %lld %lld\n", msg->_id, pid);
	    pid = msg->_id;
      ++seq;

	    // reader slow, resync
	    if((seq + w2rdelay) < h.rb->_wseq)
	    {
	      seq = h.rb->_wseq;
		    ref = seq;
	      printf("warning: %lld %lld\n", seq, h.rb->_wseq);
	    }
	    // atomically update the read sequence in the ring buffer
      // making it visible to the producer
	    shm_read_set(&h, rIndex, seq);

	    clock_t end = clock();
	    double elapsed_secs = double(end - begin) / CLOCKS_PER_SEC;
	    printf("r: %llu %f\r", h.rb->_rseq[rIndex], elapsed_secs > 0 ? (h.rb->_rseq[rIndex] - ref) / elapsed_secs : 0);
	  }

	  // master reconnection?
	  if(h.rb->_wseq < seq)
	  {
      seq = h.rb->_wseq;
	    ref = seq;
	    printf("warning: %lld %lld\n", seq, h.rb->_wseq);
	  }

	  // wait for more data
	  shm_sleep(1);
  }

  shm_close(&h);
}

int main(int argc, char** argv)
{
  if(argc != 2)
  {
    printf("please supply args (producer/consumer)\n");
	  return -1;
  }
  
  if(strcmp(argv[1], "consumer") == 0)
  {
	  consumerLoop();
	  return -1;
  }
  if(strcmp(argv[1], "producer") == 0)
  {
    producerLoop();
	  return -1;
  }

  printf("invalid arg: %s\n", argv[1]);
  return -1;
}
