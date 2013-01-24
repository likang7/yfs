// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() 
{
  pthread_mutex_init(&lock, NULL);
}

extent_server::~extent_server()
{
  files.clear();
  pthread_mutex_destroy(&lock);
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
    pthread_mutex_lock(&lock);
    
    //set the ctime and mtime to the current time
    unsigned tcur = (unsigned)time(NULL);
    if(files.find(id) != files.end())//new file
        files[id].attr.atime = tcur;
    extent_protocol::attr& attr = files[id].attr; 
    attr.ctime = tcur;
    attr.mtime = tcur;
    attr.size = buf.size();
    files[id].content = buf;

    pthread_mutex_unlock(&lock);
    return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
    pthread_mutex_lock(&lock);
    if(files.find(id) == files.end()){
        buf = "";
        pthread_mutex_unlock(&lock);
        return extent_protocol::NOENT;
    }
    extent_File& file = files[id];
    //set the atime to the current time
    file.attr.atime = (unsigned)time(NULL);
    buf = file.content;

    pthread_mutex_unlock(&lock);
    return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
    pthread_mutex_lock(&lock);

    if(files.find(id) == files.end()){
        pthread_mutex_unlock(&lock);
        return extent_protocol::NOENT;
    }
    a = files[id].attr;
    a.atime = (unsigned)time(NULL);

    pthread_mutex_unlock(&lock);
    return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
    // You fill this in for Lab 2.
    pthread_mutex_lock(&lock);

    if(files.find(id) == files.end()){
        pthread_mutex_unlock(&lock);
        return extent_protocol::NOENT;
    }
    files.erase(id);

    pthread_mutex_unlock(&lock);
    return extent_protocol::OK;
}

