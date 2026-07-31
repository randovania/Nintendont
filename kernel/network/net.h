
#ifndef _NET_H_
#define _NET_H_

void NetInit();
void NetConnect();
void NetUpdate();
void NetShutdown();
u32 NetThread(void* arg);

#endif
