#include "Config.h"
#include "common.h"
#include "debug.h"
#include "global.h"
#include "ipc.h"
#include "socket_definitions.h"
#include "string.h"
#include "syscalls.h"

#include "net.h"
#include "net_memory_operation.h"

////////
// Customize this module

#define USE_CUSTOM_HEAP
#define USE_CUSTOM_THREAD_STACK
#define NINTENDONT_PORT 43673
#define MAX_NET_SOCKETS 4

// AFAIK wii result values can be from -78 to MAX_INPUT_BYTES/MAX_OUTPUT_BYTES (depending on whats higher), so
// in order to not conflict with anything i chose this unique out of range one.
#define INITIAL_RESULT_VALUE 133780085

////////

// General workflow of this file:
// - Call NetInit() to initialize and have the Wii connect to the network to receive commands
// - Call NetShutdown() to disconnect and free resources

// Internally, NetInit will create the global resources and create two threads:
// - NetUpdate: Is responsible for connecting to the network if not connected, and for doing IOCTL calls on
// the sockets
// - NetThread: Is responsible for receiving the results of the IOCTL calls, updating the socket states and
// disconnecting if any result came back as an error.

// Some questions from the past:
// - Why is NetUpdate running on a seperate thread? Because NetUpdate is also responsible for *connecting* and
// connecting is blocking. We don't want NetThread to be the one responsible for connecting, because that can
// mess with things depending when the scheduler calls that function.
// - Why are the IOCTL calls in NetUpdate Async? Because if they were sync, then later sockets would be
// blocked by earlier sockets.

// Relevant errors that can occur:
// -13 to -15 (EConnAborted, EConnRefused, EConnReset)
// -38 to -40 (ENetUnreach, ENetReset, ENetDown)
// -56 (ENotConn)?

////////

struct setsockopt_params {
  u32 socket;
  u32 level;
  u32 optname;
  u32 optlen;
  u8 optval[20];
};

typedef enum NetSocketState {
  NET_CLOSE,
  NET_ACCEPT,
  NET_RECEIVE,
  NET_SEND
} NetSocketState;
const char* NetSocketOperationStrings[] = {
    "Close",
    "Accept",
    "Receive",
    "Send",
};

typedef struct NetSocketData {
  bool busy;
  NetSocketState state;
  int socket;
  struct ipcmessage ipc_msg;
  struct sendto_params send_params;
  SocketOperation operation;
  u8 output_buffer[MAX_OUTPUT_BYTES];
  ioctlv ctlv[3];

} NetSocketData;

extern char __net_stack_addr_1, __net_stack_size_1, __net_stack_addr_2, __net_stack_size_2;

static u32 thread_id = 0;
static u32* net_thread_stack;
static u32* net_update_stack;

static s32 net_message_queue = -1;
static u8* net_queue_heap = NULL;
static int soFd = -1;
static int netHeap = -1;
static int mainSocket = -1;
static bool net_has_active_accept = false;
static NetSocketData* net_socket_data[MAX_NET_SOCKETS];
static bool connected = false; // maybe expose a getConnectionStatus?

static u32 NetThread();
static u32 NetUpdate();

void NetInit() {
  // Initializes important lifelong variables.

  int i, result;

  dbgprintf("[Net] NetInit\r\n");

  // Open the /dev resource
  soFd = IOS_Open("/dev/net/ip/top", 0);
  dbgprintf("[Net] IOS_Open: %d\r\n", soFd);

#ifdef USE_CUSTOM_HEAP
  // stealing the 128KB heap from sock.c
  netHeap = heap_create((void*)0x13040000, 0x20000);
#else
  // use global heap
  netHeap = 0;
#endif

  net_queue_heap = (u8*)heap_alloc_aligned(netHeap, 0x200, 32);
  net_message_queue = mqueue_create(net_queue_heap, MAX_NET_SOCKETS);

  for (i = 0; i < MAX_NET_SOCKETS; ++i) {
    NetSocketData* data = net_socket_data[i] =
        (struct NetSocketData*)heap_alloc_aligned(netHeap, sizeof(struct NetSocketData), 32);
    data->busy = false;
    data->state = NET_ACCEPT;
    data->socket = -1;
    data->ipc_msg.seek.origin = i;
    data->ipc_msg.result = INITIAL_RESULT_VALUE;
  }

#ifdef USE_CUSTOM_THREAD_STACK
  // from Heap
  u32 net_thread_size = 0x400;
  net_thread_stack = (u32*)heap_alloc_aligned(netHeap, net_thread_size, 32);
  u32 net_update_size = 0x400;
  net_update_stack = (u32*)heap_alloc_aligned(netHeap, net_update_size, 32);
#else
  // from kernel.ld
  u32 net_thread_size = ((u32)(&__net_stack_size_1));
  net_thread_stack = ((u32*)&__net_stack_addr_1);
  u32 net_update_size = ((u32)(&__net_stack_size_2));
  net_update_stack = ((u32*)&__net_stack_addr_2);
#endif

  thread_id = thread_create(NetThread, NULL, net_thread_stack, net_thread_size / sizeof(u32), 0x78, 1);
  dbgprintf("[Net] thread_create NetThread: %d\r\n", thread_id);

  result = thread_continue(thread_id);
  dbgprintf("[Net] thread_continue NetThread: %d\r\n", result);

  thread_id = thread_create(NetUpdate, NULL, net_update_stack, net_update_size / sizeof(u32), 0x78, 1);
  dbgprintf("[Net] thread_create NetUpdate: %d\r\n", thread_id);

  result = thread_continue(thread_id);
  dbgprintf("[Net] thread_continue NetUpdate: %d\r\n", result);
}

static void NetDisconnect() {
  // Disconnects from the main network socket.

  int i, result;
  connected = false;

  dbgprintf("[Net] NetDisconnect\r\n");

  unsigned int* params = (unsigned int*)heap_alloc_aligned(netHeap, 4, 32);
  params[0] = mainSocket;
  result = IOS_Ioctl(soFd, IOCTL_SO_CLOSE, params, 4, 0, 0);
  dbgprintf("[Net] SOClose: %d\r\n", result);
  heap_free(netHeap, params);
  mainSocket = -1;

  static s32 kdData[8] ALIGNED(32); // TODO: see the todo in NetConnect about this.
  s32 kdFd = IOS_Open("/dev/net/kd/request", 0);
  result = IOS_Ioctl(kdFd, IOCTL_KD_NWC24ICLEANUPSOCKET, NULL, 0, kdData, 32);
  IOS_Close(kdFd);
  dbgprintf("[Net] NWC24iCleanupSocket: %d\r\n", result);

  for (i = 0; i < MAX_NET_SOCKETS; ++i) {
    NetSocketData* data = net_socket_data[i];
    data->busy = false;
    data->state = NET_ACCEPT;
    data->socket = -1;
    data->ipc_msg.seek.origin = i;
    data->ipc_msg.result = INITIAL_RESULT_VALUE;
  }
  net_has_active_accept = false;
}

static void NetConnect() {
  // Connects to the network by preparing the main socket for accepting connections.

  // All of the IOS_IOCTL calls can return negative codes if theres something wrong with the network
  // connection.

  // TODO:
  // 1) are all of the heap allocations necessary here? Or can we put them on the stack? Would be nice to not
  // worry about freeing stuff correctly.
  // 2) i kinda don't like how kdData jumps out as being different *and*
  // is static.

  int result;

  dbgprintf("[Net] NetConnect\r\n");

  // All socket funtionality on dev/net/ip/top is locked behind the WC24 socket.
  // Thus, for those to work correctly, we need to open (or close when we want to reset things) this one
  // first.
  static s32 kdData[8] ALIGNED(32);
  s32 kdFd = IOS_Open("/dev/net/kd/request", 0);
  result = IOS_Ioctl(kdFd, IOCTL_KD_NWC24ISTARTUPSOCKET, NULL, 0, kdData, 32);
  IOS_Close(kdFd);
  dbgprintf("[Net] NWC24iStartupSocket: %d\r\n", result);
  if (result < 0) {
    goto handle_error;
  }

  // SOStartup.
  result = IOS_Ioctl(soFd, IOCTL_SO_STARTUP, 0, 0, 0, 0);
  dbgprintf("[Net] SOStartup: %d\r\n", result);
  if (result < 0) {
    goto handle_error;
  }

  // SOSocket.
  unsigned int* params = (unsigned int*)heap_alloc_aligned(netHeap, 12, 32);
  params[0] = AF_INET;
  params[1] = SOCK_STREAM;
  params[2] = IPPROTO_IP;
  mainSocket = IOS_Ioctl(soFd, IOCTL_SO_SOCKET, params, 12, 0, 0);
  dbgprintf("[Net] SOSocket: %d\r\n", mainSocket);
  if (mainSocket < 0) {
    heap_free(netHeap, params);
    goto handle_error;
  }

  // SOBind.
  struct bind_params* bParams =
      (struct bind_params*)heap_alloc_aligned(netHeap, sizeof(struct bind_params), 32);
  memset(bParams, 0, sizeof(struct bind_params));
  bParams->socket = mainSocket;
  bParams->has_name = 1;
  bParams->addr.len = 8;
  bParams->addr.family = AF_INET;
  bParams->addr.port = NINTENDONT_PORT;
  bParams->addr.name = INADDR_ANY;
  result = IOS_Ioctl(soFd, IOCTL_SO_BIND, bParams, sizeof(struct bind_params), 0, 0);
  dbgprintf("[Net] SOBind: %d\r\n", result);
  heap_free(netHeap, bParams);
  if (result < 0) {
    heap_free(netHeap, params);
    goto handle_error;
  }

  // SOListen.
  params[0] = mainSocket;
  params[1] = 1;
  result = IOS_Ioctl(soFd, IOCTL_SO_LISTEN, params, 8, 0, 0);
  dbgprintf("[Net] SOListen: %d\r\n", result);
  heap_free(netHeap, params);
  if (result < 0) {
    goto handle_error;
  }

  connected = true;
  return;

handle_error:
  dbgprintf("[Net] NetConnect failed...\n\r");
  NetDisconnect();
  return;
}

void NetShutdown() {
  // Tears everything down. TODO: never called. should look where nintendont tries to reboot the kernel.

  int i;

  dbgprintf("[Net] NetShutdown\r\n");

  IOS_Close(soFd);
  heap_free(netHeap, net_queue_heap);
  for (i = 0; i < MAX_NET_SOCKETS; ++i) {
    heap_free(netHeap, net_socket_data[i]);
  }
  soFd = -1;

  NetDisconnect();

#ifdef USE_CUSTOM_THREAD_STACK
  heap_free(netHeap, net_thread_stack);
#endif

  if (netHeap != 0) {
    heap_destroy(netHeap);
  }
  netHeap = -1;
}

static u32 NetThread() {
  struct ipcmessage* msg = NULL;
  while (soFd != -1) {
    // dbgprintf("[NetThread] Waiting for a message in queue\r\n");
    mqueue_recv(net_message_queue, &msg, 0);
    int i = msg->seek.origin;
    int res = msg->result;
    mqueue_ack(msg, 0);

    NetSocketData* data = net_socket_data[i];

    dbgprintf("[NetThread] [Sock %d] [State %s] had result %d\r\n", i, NetSocketOperationStrings[data->state],
              res);

    if (res < 0) {
      // If an error occurs, just restart the whole connection.
      NetDisconnect();
      continue;
    }

    if (res == INITIAL_RESULT_VALUE) {
      // We should only get the initial result value if we're trying to handle more sockets immediately
      // after a disconnect. In which case they contain invalid data, so ignore them.
      dbgprintf("[NetThread] [Sock %d] Skipping, as it's the initial result value.", i);
      continue;
    }

    NetSocketState new_state;
    switch (data->state) {
    case NET_ACCEPT: {
      net_has_active_accept = false;
      data->socket = res;
      new_state = NET_RECEIVE;
      break;
    }
    case NET_RECEIVE: {
      if (res < MINIMUM_MESSAGE_SIZE) {
        new_state = NET_CLOSE;
      } else {
        sync_after_write(&data->operation, res);
        new_state = NET_SEND;
      }
      break;
    }
    case NET_SEND: {
      if (!data->operation.header.keep_alive) {
        new_state = NET_CLOSE;
      } else {
        new_state = NET_RECEIVE;
      }
      break;
    }
    case NET_CLOSE: {
      data->socket = -1;
      new_state = NET_ACCEPT;
      break;
    }
    default: {
      continue;
    }
    }
    data->busy = false;
    data->state = new_state;
  }
  return 0;
}

static u32 NetUpdate() {
  int i, result;

  if (soFd == -1 || netHeap == -1) {
    // Failsafe just in case
    dbgprintf("[Net] NetUpdate called before NetInit\r\n");
    return 0;
  }

  while (soFd != -1) {
    // Block this thread so that scheduler lets other threads run.
    // Number could maybe be adjusted, I just chose this randomly (around 6 frames, 99% respond percentile is
    // usually less than 80ms)
    mdelay(100);

    if (!connected) {
      dbgprintf("[Net] NetUpdate while not being connected, connecting now...\r\n");
      NetConnect();
      if (!connected) {
        dbgprintf("[Net] NetUpdate failed to connect\r\n");
        mdelay(1000);
      }
      continue;
    }

    for (i = 0; i < MAX_NET_SOCKETS; ++i) {
      NetSocketData* data = net_socket_data[i];
      if (data->busy || (data->state == NET_ACCEPT && net_has_active_accept)) {
        continue;
      }
      NetSocketState current_state = data->state;
      dbgprintf("[NetUpdate] [Sock %d] Will execute %s; Last result: %d\r\n", i,
                NetSocketOperationStrings[current_state], data->ipc_msg.result);
      data->busy = true;
      switch (current_state) {
      case NET_ACCEPT: {
        if (net_has_active_accept) {
          continue;
        }
        net_has_active_accept = true;

        // SOAccept
        memset(&data->send_params.addr, 0, sizeof(struct address));
        data->send_params.addr.len = 8;
        data->send_params.addr.family = AF_INET;

        // SOAccept should always return 0.
        result = IOS_IoctlAsync(soFd, IOCTL_SO_ACCEPT, &mainSocket, 4, &data->send_params.addr, 8,
                                net_message_queue, &data->ipc_msg);
        break;
      }
      case NET_RECEIVE: {

        // SORecvFrom
        // Clean up the data to avoid garbage from previous calls.
        memset(&data->send_params, 0, sizeof(struct sendto_params));
        memset(&data->operation, 0, sizeof(SocketOperation));
        data->send_params.socket = data->socket;
        data->send_params.flags = 0;

        data->ctlv[0].data = &data->send_params; // for just the first 8 bytes
        data->ctlv[0].len = 8;
        data->ctlv[1].data = &data->operation;
        data->ctlv[1].len = sizeof(SocketOperation);
        data->ctlv[2].data = NULL;
        data->ctlv[2].len = 0;

        // SORecVFrom should always return 0.
        result =
            IOS_IoctlvAsync(soFd, IOCTLV_SO_RECVFROM, 1, 2, data->ctlv, net_message_queue, &data->ipc_msg);
        break;
      }
      case NET_SEND: {
        int outputBytes = processSocketOperation(&data->operation, data->output_buffer);

        // SOSendTo preparation
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

        // SOSendTo should always return 0.
        result = IOS_IoctlvAsync(soFd, IOCTLV_SO_SENDTO, 2, 0, data->ctlv, net_message_queue, &data->ipc_msg);
        break;
      }
      case NET_CLOSE: {
        // SOClose can return -8 (EBADF), but this shouldn't ever happen.
        result = IOS_IoctlAsync(soFd, IOCTL_SO_CLOSE, &data->socket, 4, NULL, 0, net_message_queue,
                                &data->ipc_msg);
        break;
      }
      default:
        continue;
      }
      dbgprintf("[NetUpdate] [Sock %d] Returned %d after performing %s\r\n", i, result,
                NetSocketOperationStrings[current_state]);
    }
  }
  return 0;
}
