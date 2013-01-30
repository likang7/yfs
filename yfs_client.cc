// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
    : lockclient(lock_dst)
{
  ec = new extent_client(extent_dst);
  const inum root = 0x00000001;
  std::string buf('/'+filename(root)+'/'+"root");
  ec->put(root, buf);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

yfs_client::inum yfs_client::generate_inum(bool isdir){
    int r = rand() * rand();
    if(isdir == false){
        r |=  0x80000000;
    }
    else
        r &= 0x7FFFFFFF;
    return r;
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;
  // You modify this function for Lab 3
  // - hold and release the file lock
    lockclient.acquire(inum);
    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;

    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:
    lockclient.release(inum);
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;
  // You modify this function for Lab 3
  // - hold and release the directory lock
    lockclient.acquire(inum);

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    lockclient.release(inum);
    return r;
}

//suppose /inum/name, return value is the index of the first '/'
//if name is not found, return npos
size_t yfs_client::lookup(const std::string& pbuf, const std::string& name, 
    yfs_client::dirent& d){
        bool hasFind = false;
    size_t st = 0;
    size_t mid = pbuf.find_first_of('/', st + 1);
    size_t ed = pbuf.find_first_of('/', mid + 1);
    while(!hasFind){
        st = ed;
        if(st == std::string::npos || st == pbuf.size())
          break;

        mid = pbuf.find_first_of('/', st + 1);
        d.inum = n2i(pbuf.substr(st + 1, mid - st - 1));

        ed = pbuf.find_first_of('/', mid + 1);
        if(ed == std::string::npos)
          ed = pbuf.size();
        d.name = pbuf.substr(mid + 1, ed - mid  - 1);
        if(d.name == name){
            hasFind = true;
        }
    }

    if(hasFind == false){
        return std::string::npos;
    }
    else
        return st;
}


int yfs_client::lookupinode(inum parent, const std::string& name,
           inum &child){
    lockclient.acquire(parent);
    int r = OK;
    std::string pbuf;
    dirent ans;
    size_t pos;

    if(ec->get(parent, pbuf) != OK){
        r = NOENT;
        goto release;
    }

    
    pos = lookup(pbuf, name, ans);

    if(pos == std::string::npos){
        r = NOENT;
        goto release;
    }
    else
        child = ans.inum;
release:
    lockclient.release(parent);
    return r;
}

int yfs_client::create(inum parent, const std::string name, 
          mode_t mode, int isdir, inum &child){
    lockclient.acquire(parent);

    int r = OK;
    std::string pbuf;
    std::string filebuf;
    dirent d;
    if((r = ec->get(parent, pbuf)) != OK){
        goto release;
    }
    if(lookup(pbuf, name, d) != std::string::npos){
        r = EXIST;
        goto release;
    }
    child = generate_inum(isdir);
    lockclient.acquire(child);
    if(isdir)
        filebuf = ("/" + filename(child) + "/" + name);
    if((r = ec->put(child, filebuf)) != OK){
        lockclient.release(child);
        goto release;
    }
    lockclient.release(child);
    pbuf.append("/" + filename(child) + "/" + name);
    if((r = ec->put(parent, pbuf)) != OK){
        goto release;
    }

release:
    lockclient.release(parent);
    return r;
}

// Remove the file named @name from directory @parent.
// Free the file's extent.
// If the file doesn't exist, indicate error ENOENT.
//
// Do *not* allow unlinking of a directory.
int yfs_client::remove(inum iparent, const std::string name){
    lockclient.acquire(iparent);

    int r = OK;

    std::string pbuf;
    size_t pos;
    size_t len;
    dirent d;

    if(ec->get(iparent, pbuf) != OK){
        r = NOENT;
        goto release;
    }

    pos = lookup(pbuf, name, d);
    if(pos == std::string::npos){
        r = NOENT;
        goto release;
    }

    lockclient.acquire(d.inum);
    //firstly, erase the file from the content of its parent
    len = filename(d.inum).size() + name.size() + 2;
    pbuf.erase(pos, len);
    if(ec->put(iparent, pbuf) != OK){
        r = IOERR;
        lockclient.release(d.inum);
        goto release;
    }
    //secondly, erase the file
    if(ec->remove(d.inum) != OK){
        r = IOERR;
        lockclient.release(d.inum);
        goto release;
    }
release:
    lockclient.release(iparent);
    return r;
}

int yfs_client::readdir(inum inum, std::vector<dirent> &ans){
    lockclient.acquire(inum);
    int r = OK;

    std::string pbuf;
    if(ec->get(inum, pbuf) != OK){
        r = NOENT;
        lockclient.release(inum);
        return r;
    }

    dirent d;
    size_t st = 0;
    size_t mid = pbuf.find_first_of('/', st + 1);
    size_t ed = pbuf.find_first_of('/', mid + 1);
    while(true){
        st = ed;
        if(st == std::string::npos || st == pbuf.size())
          break;

        mid = pbuf.find_first_of('/', st + 1);
        d.inum = n2i(pbuf.substr(st + 1, mid - st - 1));

        ed = pbuf.find_first_of('/', mid + 1);
        if(ed == std::string::npos)
          ed = pbuf.size();
        d.name = pbuf.substr(mid + 1, ed - mid  - 1);
        ans.push_back(d);
    }

    lockclient.release(inum);
    return r;
}

int yfs_client::setattr(inum inum, fileinfo& attr){
    lockclient.acquire(inum);

    int r = OK;
    std::string buf;

    if(ec->get(inum, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    //If the new size is bigger than the current
    // file size, fill the new bytes with '\0'. 
    if(attr.size > buf.size()){
        buf.append(buf.size() - attr.size, '\0');
    }
    else
        buf = buf.substr(0, attr.size);

    if(ec->put(inum, buf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

release:
    lockclient.release(inum);
    return r;
}

// If there are fewer than @size bytes to read between @off and the
// end of the file, read just that many bytes. If @off is greater
// than or equal to the size of the file, read zero bytes.
int yfs_client::readfile(inum inum, size_t size, off_t off, 
  std::string &buf){
    lockclient.acquire(inum);
    int r = OK;

    std::string filebuf;
    size_t left;
    if(ec->get(inum, filebuf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

 //  If @off is greater than or equal to the size of the file, read zero bytes.
    if((size_t)off >= filebuf.size())
        goto release;
// If there are fewer than @size bytes to read between @off and the
// end of the file, read just that many bytes.
    left = filebuf.size() - off;
    if(left < size){
        buf = filebuf.substr(off, filebuf.size() - off);
    }
    else{
        buf = filebuf.substr(off, size);
    }
release:
    lockclient.release(inum);
    return r;
}

// Write @size bytes from @buf to file @ino, starting
// at byte offset @off in the file.
int yfs_client::writefile(inum inum, off_t off,
       const char* buf, size_t size){
    lockclient.acquire(inum);
    int r = OK;
    std::string towrite;
    towrite.append(buf, size);

    std::string filebuf;
    if(ec->get(inum, filebuf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

// If @off + @size is greater than the current size of the
// file, the write should cause the file to grow. If @off is
// beyond the end of the file, fill the gap with null bytes.
    if((size_t)off <= filebuf.size()){
        std::string padding = "";
        if(off + size < filebuf.size()){
          padding = filebuf.substr(off + size, filebuf.size());
        }
        filebuf = filebuf.substr(0, off) + towrite + padding;
    }
    else{
        filebuf.append(off - filebuf.size(), '\0');
        filebuf.append(towrite);
    }

    if(ec->put(inum, filebuf) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

release:
    lockclient.release(inum);
    return r;
}
