---
link: https://medium.com/@m-ibrahim.research/tracing-a-packet-in-the-linux-kernel-from-socket-to-wire-part-1-125edd4c021d
---
> Лучше читать в оригинале. Там более красивое форматирование в блоках кода

Months ago, I did some initial research on how packets travel from application-level protocols like HTTP, gRPC, and WebSockets down to the wire. I even shared a short **_LinkedIn_** post about it, but it was surface-level and didn’t satisfy my curiosity.

In this three-part article series, we will deep dive into Linux kernel internals, following a packet from `send()` / `sendto()` system calls all the way to NIC DMA (Network Interface Card Direct Memory Access).

This part serves as an entry point into the socket world and covers:

-   Tools used to explore the Linux kernel
-   What a socket is, socket types, and protocols
-   The socket life cycle
-   Socket hierarchy and representation levels
-   struct socket ops vs struct sock ops
-   Flags Across Socket Layers

**_Note:_** this articles series will go a TCP protocol focused, because it’s the building block of the modern networking systems.

Press enter or click to view image in full size

![](https://miro.medium.com/v2/resize:fit:700/1*MoDNKgVoJzrxkQizqaip6w.png)

Screenshot from tracing process

## Used Tools

-   `Linux kernel source tree`
-   `strace` : syscalls tracing
-   `ripgrep` : fast code search
-   `ctags / kernel tags` : fast code search
-   `vim`
-   `Obsidian` : taking notes

These tools were enough to follow execution from user space into the kernel and across multiple networking layers.

## 1 - Socket

A socket is a bidirectional / two-way communication endpoint that represents one end of a communication channel between processes, either over a network or locally on the same host `(AF_INET`, `AF_INET6`, `AF_UNIX)`.

Sockets form the core abstraction used by most networking protocols (TCP, UDP, QUIC, ICMP, etc.), acting as the gateway between applications and the kernel network stack.

```
int fd = socket(AF_INET, SOCK_STREAM, 0);struct sockaddr_in addr = {    .sin_family = AF_INET,    .sin_port = htons(8080),    .sin_addr.s_addr = inet_addr("127.0.0.1"),};bind(fd, (struct sockaddr *)&addr, sizeof(addr));connect(fd, (struct sockaddr *)&addr, sizeof(addr)); 
```

### Socket Types

```
int socket(int family, int type, int protocol); 
```

**_Family :_**

-   Socket network address family
-   `AF_INET` for ipv4 family
-   `AF_INET6` for ipv6

**_Types :_**

-   **SOCK\_STREAM** : ordered, reliable byte stream
-   **SOCK\_DGRAM** : message-based, no guarantees
-   **SOCK\_RAW** : bypass transport layer
-   **SOCK\_SEQPACKET** : reliable + preserves message boundaries

The socket type defines the communication semantics (stream vs datagram vs raw), while the protocol family determines which networking stack will implement them.

**_Protocol :_**

-   IPPROTO\_TCP
-   IPPROTO\_UDP
-   IPPROTO\_ICMP
-   IPPROTO\_IPV6
-   IPPROTO\_RAW

When `protocol == 0`, Linux performs:

```
(domain, type) → default protocol lookup
```

**Example :**

```
(AF_INET, SOCK_STREAM) → TCP(AF_INET, SOCK_DGRAM)  → UDP
```

Also, you must specify protocol when:

-   Raw sockets
-   Non-default protocols
-   Multiple protocols share the same type

## 2 - Socket Lifecycle

A socket is created, optionally bound, transitioned into a connected state (via `connect()` or `accept()`), used for data transfer, and finally destroyed with `close()`.

1) `socket()`

Creates a socket object in kernel + returns a file descriptor. _No network traffic yet._

2) `bind()` (optional)

Associates socket with:

```
(IP address, Port)   or   Unix path
```

Servers almost always bind.  
Clients often skip it, kernel auto-assigns ephemeral port.

3) `listen()` (servers)

Turns a passive socket into a listening socket. _TCP only._

4) `accept()` (servers)

Creates a **new socket** per client connection.

```
listening socket  -> stays openaccepted socket   -> used for data
```

5) `connect()` (clients)

Initiates connection.

-   TCP → 3-way handshake
-   UDP → just stores peer address (no handshake)

6) Data transfer

```
send(), recv()write(), read()sendmsg(), recvmsg()
```

All roads lead to kernel TX/RX paths.

7) `close()`

Releases FD.

-   TCP → FIN / teardown
-   UDP → immediate free

## 3 - Socket Hierarchy — Representation Levels

When an application calls `socket()`, Linux gradually builds a stack of objects, each operating at a different abstraction level:

```
Userspace  |glibc wrapper  |Syscall entry  |struct file        (VFS object)  |struct socket      (generic socket abstraction)  |struct sock        (protocol-agnostic core)  |struct sk_buff     (actual packets) 
```

### 1\. VFS Layer — `struct file`

**Path:** `include/linux/fs.h`

Linux treats **everything as a file**, So a socket first becomes:

```
struct file 
```

Where Key fields:

-   `f_op` → table of file operations
-   `private_data` → points to the real object
-   `f_inode` → `inode`metadata

For sockets:

```
file->private_data = struct socket
```

This is why sockets get file descriptors.

### 2\. Generic Socket Layer — `struct socket`

**Path:** `include/linux/net.h`

Represents a socket from userspace’s perspective.

Main fields:

-   `state` `: SS_UNCONNECTED`, `SS_CONNECTED`, …
-   `type` `: SOCK_STREAM`, `SOCK_DGRAM`, …
-   `flags` `: O_NONBLOCK`, `CLOEXEC`, …
-   `file` : back pointer to struct file
-   `sk` : pointer to protocol engine (`struct sock`)
-   `ops` !: function table (`connect`, `sendmsg`, `recvmsg`, …)
-   `wq` : wait queue

### 3\. Protocol-Agnostic Core — `struct sock`

**Path:** `include/net/sock.h`

This is the **heart of networking**, One `struct sock` represents one protocol instance.

## Get Meriah Abderrahim’s stories in your inbox

Join Medium for free to get updates from this writer.

Remember me for faster sign in

It owns queues, buffers, timers, and state.

-   `sk_receive_queue` for incoming packets
-   `sk_write_queue` for packets waiting to send
-   `sk_error_queue` for errors / control messages
-   `sk_backlog_queue` for temporary queue when busy

```
struct sock  ├─ RX queue   → skb → skb → skb  ├─ TX queue   → skb → skb  └─ ERR queue  → skb
```

```
sk_receive_queue (sk_buff_head)   next ------------------> skb1   prev ------------------> skb3skb1 <-> skb2 <-> skb3
```

**_Other important fields:_**

-   `sk_rcvbuf` : max receive buffer size
-   `sk_sndbuf` : max send buffer size
-   `sk_send_head` : next skb to transmit
-   `sk_socket` : back pointer to struct socket

```
struct socket   └── sk → struct sockstruct sock   └── sk_socket → struct socket
```

### The `**sock_common**` **field (shared by all sockets)**

Contains addressing and identity:

-   `skc_family` : AF\_INET / AF\_INET6
-   `skc_state` : TCP state (ESTABLISHED, LISTEN, …)
-   `skc_daddr :`remote IP
-   `skc_rcv_saddr` : local IP
-   `skc_dport` : remote port
-   `skc_num` : local port
-   `skc_prot` : pointer to protocol implementation

```
sk->sk_prot = &tcp_prot;   sk->sk_prot = &udp_prot;   
```

Although called “protocol-agnostic”, `struct sock` it is **heavily optimized for TCP:**

-   Retransmissions
-   Keepalive
-   Delayed ACK
-   Time-wait handling

Press enter or click to view image in full size

![](https://miro.medium.com/v2/resize:fit:700/1*a4hlHOD__COIdpKmyuApDw.png)

Aliasing \_\_sk\_common fields in the sock struct

### 4\. Packet Representation — `struct sk_buff`

**Path:** `include/linux/skbuff.h`

Every packet on Linux is an **skb** (socket buffer / struct sk\_buff). Where Memory layout looks:

```
[ HEADROOM ][ PACKET DATA ][ TAILROOM ]
```

![](https://miro.medium.com/v2/resize:fit:550/1*zh9gRPN-vbWktyQ09VGDRA.png)

sk\_buff

`skb` or the `struct sk_buff`, Is the main representation of a packet and that will be passed between functions from creation to the NIC.

Headroom and tail-room exist so headers/payload can be added or removed by moving pointers, not bytes.

The struct contains Pointers about the data to be sent / received :

-   `head` : start of allocation
-   `data` : start of packet
-   `tail` : end of packet
-   `end` : end of allocation

```
head ------------------------------ end        ^data ----------- ^tail
```

Linux adjusts pointers instead of moving bytes. Where :

-   head/data/tail: manipulate bytes inside one packet
-   next/prev : link multiple packets together

skbs usually form linked lists; sometimes they live in rbtrees for ordered lookup.

```
1          struct rb_node      rbnode;   20         struct list_head    list;  19         struct llist_node   ll_node;
```

When an `sk_buff` is allocated, Linux also reserves space at the **end of the buffer** for a companion structure called `skb_shared_info`.  
This structure stores metadata about the buffer and describes any **non-linear data** associated with the packet.

Rather than placing all packet bytes in the main linear area (`data` → `tail`), large or additional payload segments can be stored as **fragments** referenced from `skb_shared_info`.

This allows the kernel to avoid copying data and to build packets from multiple memory regions efficiently, something heavily used in the transmit path (TX).

Press enter or click to view image in full size

![](https://miro.medium.com/v2/resize:fit:700/1*PcurCHSpdW1IetPRQnVZjw.png)

Data Fragments

**What lives inside** `**skb_shared_info**`

-   `frags[]` : page-backed fragments holding payload data
-   `nr_frags` : number of active fragments
-   `gso_size`, `gso_type` : segmentation offload metadata
-   `tx_flags` : transmit-related flags
-   `refcount` : shared buffer reference count

## 4 - operations differences between layers

Linux splits socket operations into **two layers**:

-   A **generic socket layer** visible to the VFS and system calls
-   A **protocol layer** that implements actual networking behavior

### `socket` `-> proto_ops`

Defined in: `include/linux/net.h`

Press enter or click to view image in full size

![](https://miro.medium.com/v2/resize:fit:700/1*gL_EsOcBOnZq1h14PXuqOg.png)

Struct definition in Linux kernel

This operation table defines **how system calls interact with a socket**.

This layer receives requests from syscalls (connect, sendmsg, recvmsg, …), performs generic checks and forwards work to protocol layer

Press enter or click to view image in full size

![](https://miro.medium.com/v2/resize:fit:700/1*D_e7k9bN_r0JnRYdGTHmPA.png)

TCP operations example

### `sock` `-> struct proto`

Defined in: `include/net/sock.h`

Press enter or click to view image in full size

![](https://miro.medium.com/v2/resize:fit:700/1*dIUi7vKl_mNZ1ZkAVR4LcA.png)

Definition in the Linux kernel

`struct proto` contains **protocol-specific logic**.

This layer does Implement TCP state machine, handles segmentation, retransmission, builds skbs and interacts with routing, qdisc, and drivers

Press enter or click to view image in full size

![](https://miro.medium.com/v2/resize:fit:700/1*1fTqpY2GqT1gxa53zxVH-g.png)

TCP sk\_prot

## 5 - Flags Across Socket Layers

Linux uses flags at **multiple layers**, each controlling different aspects of behavior. Same word (flag), very different meaning depending on the layer.

-   File flags : VFS behavior
-   Socket flags : Generic socket behavior
-   Sock flags : Protocol engine state
-   SKB flags : Per-packet properties

### 1 - File Descriptor Layer (`struct file`)

These are the classic POSIX-style flags.

-   `O_NONBLOCK` : non-blocking I/O
-   `O_CLOEXEC` : close on exec
-   `O_APPEND` : append writes
-   `O_SYNC` : synchronous I/O

### 2 - Socket Layer (`struct socket`)

-   `SOCK_NONBLOCK`
-   `SOCK_CLOEXEC`
-   `SOCK_NOSPACE`

These are often **mirrored** into file flags during creation.

### 3 - Protocol Core Layer (`struct sock`)

Stored in multiple bitfields:

```
sk->sk_flagssk->sk_shutdownsk->sk_state
```

Common flags:

-   `SOCK_DEAD` : socket being destroyed
-   `SOCK_DONE` : no more data expected
-   `SOCK_ZAPPED` : not fully initialized
-   `SOCK_LINGER` : linger enabled

Shutdown state:

-   `RCV_SHUTDOWN`
-   `SEND_SHUTDOWN`

### 4 - SKB (Packet) Flags

Stored mainly in:

```
skb->ip_summedskb->devskb->priorityskb->markskb_shinfo(skb)->tx_flags
```

-   `**skb->ip_summed**` : checksum status / offload mode  
    (`CHECKSUM_NONE`, `CHECKSUM_PARTIAL`, `CHECKSUM_COMPLETE`)
-   `**skb->dev**` : network device this packet is associated with
-   `**skb->priority**` : packet QoS / scheduling priority
-   `**skb->mark**` : generic tag used by routing, firewall, tc, policy
-   `**skb_shinfo(skb)->tx_flags**` : TX offload & timestamping flags  
    (GSO, TSO, HW/SW timestamp)

In this part, we built the mental model: what a socket is, how Linux represents it internally, and how different layers connect together.

In Part 2, we’ll follow the actual birth of a socket — walking through `socket()`, `bind()`, and `connect()` and seeing how these calls wire everything up inside the kernel.