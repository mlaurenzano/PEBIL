#ifndef _foo_h_
#define _foo_h_

#define XY(x,y) if(x) { x = y * x; } return x;

#ifdef CPP 
#define C_PREFIX(__s) cpp_ ## __s
#else
#define C_PREFIX(__s) __s
#endif

#endif
