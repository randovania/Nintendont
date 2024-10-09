#include "global.h"
#include "common.h"
#include "string.h"
#include "ipc.h"
#include "Config.h"
#include "debug.h"
#include "syscalls.h"
#include "socket_definitions.h"

#include "net.h"
#include "net_memory_operation.h"

////////
// Customize this module

#define USE_CUSTOM_HEAP
#define USE_CUSTOM_THREAD_STACK

////////

struct setsockopt_params {
    u32 socket;
    u32 level;
    u32 optname;
    u32 optlen;
    u8 optval[20];
};

typedef enum NetSocketOperation {
  NET_CLOSE,
  NET_ACCEPT,
  NET_RECEIVE,
  NET_SEND
} NetSocketOperation;
const char* NetSocketOperationStrings[] =
{
	"Close",
  "Accept",
  "Receive",
  "Send",
};

#define NET_BUFFER_SIZE 128

typedef struct NetSocketData {
  bool busy;
  NetSocketOperation operation;
  int socket;
  struct ipcmessage ipc_msg;
  struct sendto_params send_params;
  MemoryOperation memory_op;
  u8 output_buffer[MAX_OUTPUT_BYTES];
  ioctlv ctlv[3];

} NetSocketData;

#define MAX_NET_SOCKETS 4
extern char __net_stack_addr, __net_stack_size;

static u32 net_thread_id = 0;
static u32 *net_thread_stack;

static s32 net_message_queue = -1;
static u8 *net_queue_heap = NULL;
static int soFd = -1;
int netHeap = -1;
static int mainSocket = -1;
static bool net_has_active_accept = false;
static NetSocketData* net_socket_data[MAX_NET_SOCKETS];

void NetInit() {
  int i, result;

#ifdef USE_CUSTOM_HEAP
  //stealing the 128KB heap from sock.c
	netHeap = heap_create((void*)0x13040000, 0x20000);
#else
  // use global heap
  netHeap = 0;
#endif

  net_queue_heap = (u8*)heap_alloc_aligned(netHeap, 0x200, 32);
	net_message_queue = mqueue_create(net_queue_heap, MAX_NET_SOCKETS);

  // if (!ConfigGetConfig(NIN_CFG_PRIME_DUMP)) {
  //   netStart = 0;
  //   return 0;
  // }

  for (i = 0; i < MAX_NET_SOCKETS; ++i)
  {
    NetSocketData* data = net_socket_data[i] = (struct NetSocketData *) heap_alloc_aligned(netHeap, sizeof(struct NetSocketData), 32);
    data->busy = false;
    data->operation = NET_ACCEPT;
    data->socket = -1;
    data->ipc_msg.seek.origin = i;
  }

  // Open the /dev resource
	char *soDev = "/dev/net/ip/top";
	void *name = heap_alloc_aligned(netHeap, 32, 32);
	memcpy(name, soDev, 32);
	soFd = IOS_Open(name, 0);	
  heap_free(netHeap, name);
  dbgprintf("[Net] IOS_Open: %d\r\n", soFd);

  //SOStartup
  result = IOS_Ioctl(soFd, IOCTL_SO_STARTUP, 0, 0, 0, 0);
  dbgprintf("[Net] SOStartup: %d\r\n", result);

  // //SOGetHostId
  // int ip = 0;
  // do {
  //   mdelay(500);
  //   ip = IOS_Ioctl(soFd, IOCTL_SO_GETHOSTID, 0, 0, 0, 0);
  //   dbgprintf("[Net] Attempting to get IP: %x\r\n", ip);
  // } while (ip == 0);

  //SOSocket
  unsigned int *params = (unsigned int *) heap_alloc_aligned(netHeap, 12, 32);
  params[0] = AF_INET;
  params[1] = SOCK_STREAM;
  params[2] = IPPROTO_IP;
  mainSocket = IOS_Ioctl(soFd, IOCTL_SO_SOCKET, params, 12, 0, 0);
  dbgprintf("[Net] SOSocket: %d\r\n", mainSocket);

  //SOBind
  struct bind_params *bParams = (struct bind_params *) heap_alloc_aligned(netHeap, sizeof(struct bind_params), 32);
  memset(bParams, 0, sizeof(struct bind_params));
  bParams->socket = mainSocket;
  bParams->has_name = 1;
  bParams->addr.len = 8;
  bParams->addr.family = AF_INET;
  bParams->addr.port = 43673;
  bParams->addr.name = INADDR_ANY;
  result = IOS_Ioctl(soFd, IOCTL_SO_BIND, bParams, sizeof(struct bind_params), 0, 0);
  dbgprintf("[Net] SOBind: %d\r\n", result);
  heap_free(netHeap, bParams);

  //SOListen
  params[0] = mainSocket;
  params[1] = 1;
  result = IOS_Ioctl(soFd, IOCTL_SO_LISTEN, params, 8, 0, 0);
  dbgprintf("[Net] SOListen: %d\r\n", result);
  heap_free(netHeap, params);

#ifdef USE_CUSTOM_THREAD_STACK
  // from Heap
  u32 net_thread_size = 0x400;
	net_thread_stack = (u32*)heap_alloc_aligned(netHeap, net_thread_size, 32);
#else
  // from kernel.ld
  u32 net_thread_size = ((u32)(&__net_stack_size));
  net_thread_stack = ((u32*)&__net_stack_addr);
#endif
	
  net_thread_id = thread_create(NetThread, NULL, net_thread_stack, net_thread_size / sizeof(u32), 0x78, 1);
  dbgprintf("[Net] thread_create: %d\r\n", net_thread_id);
	
  result = thread_continue(net_thread_id);
  dbgprintf("[Net] thread_continue: %d\r\n", result);  
}

void NetShutdown() {
  int i;

  dbgprintf("[Net] NetShutdown\r\n");
  
  IOS_Close(soFd);
  heap_free(netHeap, net_queue_heap);
  for (i = 0; i < MAX_NET_SOCKETS; ++i)
  {
    heap_free(netHeap, net_socket_data[i]);
  }
  soFd = -1;
  
#ifdef USE_CUSTOM_THREAD_STACK
  heap_free(netHeap, net_thread_stack);
#endif
  
  if (netHeap != 0) {
	  heap_destroy(netHeap);
  }
	netHeap = -1;
}

u32 NetThread(void *arg) {
	struct ipcmessage *msg = NULL;
	while(soFd != -1)
	{
    //dbgprintf("[NetThread] Waiting for a message in queue\r\n");
		mqueue_recv(net_message_queue, &msg, 0);
		int i = msg->seek.origin;
		int res = msg->result;
		mqueue_ack(msg, 0);
    
    //dbgprintf("[NetThread] [Sock %d] Got result %d\r\n", i, res);
    NetSocketData* data = net_socket_data[i];

    NetSocketOperation new_op;
    switch(data->operation) {
      case NET_ACCEPT: {
        net_has_active_accept = false;
        data->socket = res;
        new_op = NET_RECEIVE;
        break;
      }
      case NET_RECEIVE: {
        if (res < MINIMUM_MESSAGE_SIZE) {
          new_op = NET_CLOSE;
        } else {
          sync_after_write(&data->memory_op, res);
          new_op = NET_SEND;
        }
        break;
      }
      case NET_SEND: {
        if (res < 0 || !data->memory_op.keep_alive) {
          new_op = NET_CLOSE;
        } else {
          new_op = NET_RECEIVE;
        }
        break;
      }
      case NET_CLOSE: {
        data->socket = -1;
        new_op = NET_ACCEPT;
        break;
      }
      default: {
        continue;
      }
    }
    data->busy = false;
    data->operation = new_op;
  }
  return 0;
}

void NetUpdate()
{
  int i, result;

  for (i = 0; i < MAX_NET_SOCKETS; ++i)
  {
    NetSocketData* data = net_socket_data[i];
    if (data->busy || (data->operation == NET_ACCEPT && net_has_active_accept)) {
      continue;
    }
    NetSocketOperation current_op = data->operation;
    dbgprintf("[Net] [Sock %d] Will execute %s; Last result: %d\r\n", i, NetSocketOperationStrings[current_op], data->ipc_msg.result);
    data->busy = true;
    switch(current_op) {
      case NET_ACCEPT: {
        if (net_has_active_accept) {
          continue;
        }
        net_has_active_accept = true;

        //SOAccept
        memset(&data->send_params.addr, 0, sizeof(struct address));
        data->send_params.addr.len = 8;
        data->send_params.addr.family = AF_INET;

        result = IOS_IoctlAsync(soFd, IOCTL_SO_ACCEPT, &mainSocket, 4, &data->send_params.addr, 8, net_message_queue, &data->ipc_msg);
        break;
      }
      case NET_RECEIVE: {

        //SORecvFrom
        memset(&data->send_params, 0, sizeof(struct sendto_params));
        data->send_params.socket = data->socket;
        data->send_params.flags = 0;

        data->ctlv[0].data = &data->send_params;  // for just the first 8 bytes
        data->ctlv[0].len = 8;
        data->ctlv[1].data = &data->memory_op;
        data->ctlv[1].len = sizeof(MemoryOperation);
        data->ctlv[2].data = NULL;
        data->ctlv[2].len = 0;

        result = IOS_IoctlvAsync(soFd, IOCTLV_SO_RECVFROM, 1, 2, data->ctlv, net_message_queue, &data->ipc_msg);
        break;
      }
      case NET_SEND: {
        int outputBytes = processMemoryOperation(&data->memory_op, data->output_buffer);

        //SOSendTo preparation
        memset(&data->send_params, 0, sizeof(struct sendto_params));
        data->send_params.socket = data->socket;
        data->send_params.flags = 0;
        data->send_params.has_destaddr = 0;

        data->ctlv[0].data = data->output_buffer;
        data->ctlv[0].len = outputBytes;
        data->ctlv[1].data = &data->send_params;
        data->ctlv[1].len = sizeof(struct sendto_params);
        data->ctlv[2].data = NULL;
        data->ctlv[2].len = 0;

        result = IOS_IoctlvAsync(soFd, IOCTLV_SO_SENDTO, 2, 0, data->ctlv, net_message_queue, &data->ipc_msg);
        break;
      }
      case NET_CLOSE: {
        result = IOS_IoctlAsync(soFd, IOCTL_SO_CLOSE, &data->socket, 4, NULL, 0, net_message_queue, &data->ipc_msg);
        break;
      }
      default:
        continue;
    }
    dbgprintf("[Net] [Sock %d] Received %d after performing %s\r\n", i, result, NetSocketOperationStrings[current_op]);
  }
}