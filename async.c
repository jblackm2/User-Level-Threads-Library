#include <aio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include "scheduler.h"

ssize_t read_wrap(int fd, void * buf, size_t count) {

  struct aiocb *aiocbBlock;
 
  aiocbBlock = malloc(sizeof(struct aiocb));
  memset(aiocbBlock, 0, sizeof(struct aiocb)); //Set memory to zeros

  off_t current_position = lseek(fd, 0, SEEK_CUR);

  if(current_position != -1){ //Check if the file is seekable
    aiocbBlock->aio_offset = current_position;
  }
  else{ //Otherwise offset is nothing
    aiocbBlock->aio_offset = 0;
  }

  //Initialize other aioblock variables
  aiocbBlock->aio_nbytes = count;
  aiocbBlock->aio_fildes = fd;
  aiocbBlock->aio_buf = buf;
  aiocbBlock->aio_sigevent.sigev_notify = SIGEV_NONE;
  aiocbBlock->aio_reqprio = 0;
 
  aio_read(aiocbBlock); //Start reading file
 
  while(aio_error(aiocbBlock) == EINPROGRESS){ //Yield if you are still reading
    yield();
  }
  
  int ret = aio_return(aiocbBlock); //Return value is # bytes read, or error
  lseek(fd, ret, SEEK_CUR); //Set the new offset with # bytes read
  
  return ret;
}
