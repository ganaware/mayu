// ////////////////////////////////////////////////////////////////////////////
// msgstream.h


#include "misc.h"

#include "msgstream.h"

#include <malloc.h>
#include <mbstring.h>


using namespace std;


omsgbuf::omsgbuf(UINT i_messageid, HWND i_hwnd)
  : hwnd(i_hwnd),
    messageid(i_messageid),
    debugLevel(0),
    msgDebugLevel(0)
{
  buf = new char[lengthof_buf];
  ASSERT(buf);
  setp(buf, buf + lengthof_buf);
}


omsgbuf::~omsgbuf()
{
  omsgbuf::sync();
  delete [] buf;
}
  

// attach a msg control
omsgbuf* omsgbuf::attach(HWND i_hwnd)
{
  Acquire a(&cs);
  if (hwnd || !i_hwnd)
    return NULL;	// error if already attached
  
  hwnd = i_hwnd;
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
int omsgbuf::overflow(int i_c)
{
  if (omsgbuf::sync() == EOF) // sync before new buffer created below
    return EOF;
    
  if (i_c != EOF)
  {
    *pptr() = static_cast<char>(i_c);
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
    setp(buf, buf + NUMBER_OF(buf));
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
void omsgbuf::acquire(int i_msgDebugLevel)
{
  cs.acquire();
  msgDebugLevel = i_msgDebugLevel;
}


// end writing
void omsgbuf::release()
{
  if (!str.empty())
    PostMessage(hwnd, messageid, 0, (LPARAM)this);
  msgDebugLevel = debugLevel;
  cs.release();
}


omsgstream::omsgstream(UINT i_message, HWND i_hwnd)
  : ostream(new omsgbuf(i_message, i_hwnd))
{
}
