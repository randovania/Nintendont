
#ifndef _NET_H_
#define _NET_H_

void NetInit();
void NetUpdate();
void NetShutdown();
u32 NetThread(void *arg);

#endif
