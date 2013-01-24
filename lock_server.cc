// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0), 
  qlock(PTHREAD_MUTEX_INITIALIZER),
  qready(PTHREAD_COND_INITIALIZER)
{

}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
	pthread_mutex_lock(&qlock);
	if(lockstate.find(lid) == lockstate.end())
	{
		//create the lock
		lockstate[lid] = frees;
	}
	
	LockState& state = lockstate[lid];
	//grant the lock
	while(state == locked)
		pthread_cond_wait(&qready, &qlock);

	state = locked;

	pthread_mutex_unlock(&qlock);
	return lock_protocol::OK;
}

lock_protocol::status lock_server::release(int clt, lock_protocol::lockid_t lid, int &)
{
	lockstate[lid] = frees;

	pthread_cond_signal(&qready);
	return lock_protocol::OK;
}

