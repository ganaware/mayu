// ////////////////////////////////////////////////////////////////////////////
// msgstream.h


#include "misc.h"

#include "msgstream.h"

#include <malloc.h>
#include <mbstring.h>


using namespace std;


omsgbuf::omsgbuf(UINT messageid_, HWND hwnd_)
  : hwnd(hwnd_),
    messageid(messageid_),
    debugLevel(0),
    msgDebugLevel(0)
{
  buf = new char[lengthof_buf];
  assert(buf);
  setp(buf, buf + lengthof_buf);
}


omsgbuf::~omsgbuf()
{
  omsgbuf::sync();
  delete [] buf;
}
  

// attach a msg control
omsgbuf* omsgbuf::attach(HWND hwnd_)
{
  Acquire a(&cs);
  if (hwnd || !hwnd_)
    return NULL;	// error if already attached
  
  hwnd = hwnd_;
  if (!str.empty())
    PostMessage(hwnd, messageid, 0, (LPARAM)this);
  return this;
}


// detach a msg control
omsgbuf* omsgbuf::detach()
{
  Acquire a(&cs);
  omsgbuf::sync();
  hwnd = NULL;
  return this;
}


// for stream
int omsgbuf::overflow(int c)
{
  if (omsgbuf::sync() == EOF) // sync before new buffer created below
    return EOF;
    
  if (c != EOF)
  {
    *pptr() = (char)c;
    pbump(1);
    omsgbuf::sync();
  }
  return 1;  // return something other than EOF if successful
}
  

// for stream
int omsgbuf::sync()
{
  char *begin = pbase();
  char *end = pptr();
  char *i;
  for (i = begin; i < end; i++)
    if (_ismbblead(*i))
      i++;
  if (i == end)
  {
    if (msgDebugLevel <= debugLevel)
      str += string(begin, end - begin);
    setp(buf, buf + lengthof_buf);
  }
  else // end < i
  {
    if (msgDebugLevel <= debugLevel)
      str += string(begin, end - begin - 1);
    buf[0] = end[-1];
    setp(buf, buf + lengthof(buf));
    pbump(1);
  }
  return 0;
}


// acquire string
const std::string &omsgbuf::acquireString()
{
  cs.acquire();
  return str;
}


// release the string
void omsgbuf::releaseString()
{
  str.resize(0);
  cs.release();
}


// begin writing
void omsgbuf::acquire()
{
  cs.acquire();
}


// begin writing
void omsgbuf::acquire(int msgDebugLevel_)
{
  cs.acquire();
  msgDebugLevel = msgDebugLevel_;
}


// end writing
void omsgbuf::release()
{
  if (!str.empty())
    PostMessage(hwnd, messageid, 0, (LPARAM)this);
  msgDebugLevel = debugLevel;
  cs.release();
}


omsgstream::omsgstream(UINT message, HWND hwnd)
  : ostream(new omsgbuf(message, hwnd))
{
}
