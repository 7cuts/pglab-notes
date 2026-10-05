(иду по коду ядра, оглядываясь на  [гайд](https://medium.com/@m-ibrahim.research/tracing-a-packet-in-the-linux-kernel-from-socket-to-wire-part-1-125edd4c021d))
## Жизненный цикл сокета

1. `connect()`
2. `bind()` (обычно так делает только сервер) 
3. `listen()` 
4. `accept()` (клинетов)
5. `connect()` - инициирует соединение (3-way handshake)
6. работа с данными 
	1. `send()`, `recv()`
	2. `write()`, `read()`,
	3. `sendmsg()`, `recvmsg()`
7. `close()` (FIN)

## Иерархия сокетов

#todo зачем вообще разделили `struct socket` и `struct sock`
```c
Userspace  
  |  
glibc wrapper  
  |  
Syscall entry  
  |  
struct file        (VFS object)  
  |  
struct socket      (generic socket abstraction)  
  |  
struct sock        (protocol-agnostic core)  
  |  
struct sk_buff     (actual packets)
```



### `struct socket` - сокет для юзера
`include/linux/net.h`

Из интересного
- `proto_ops` - методы
	- `sendmsg`, `recvmsg`, `connect`, `listen`, .... #todo а где `send` и `recv`? 
- `struct sock *sk` - уровень ниже
- `struct file *file` - уровень выше
- `wq`: wait queue


### `struct sock` - сущность-протокол 

Здесь буферы, таймеры, состояния и т.д.
- `sk_backlog_queue`
- `sk_write_queue`
- `sk_error_queue`
- `sk_receive_queue`

- `sk_recbbuff`- max receive buffer size
- `sk_sendbug` - max send buffer size



### `sk_buff` - отдельный пакет

Из `include/linux/skbuff.h:735`
```
/*
 *                                  ---------------
 *                                 | sk_buff       |
 *                                  ---------------
 *     ,---------------------------  + head
 *    /          ,-----------------  + data
 *   /          /      ,-----------  + tail
 *  |          |      |            , + end
 *  |          |      |           |
 *  v          v      v           v
 *   -----------------------------------------------
 *  | headroom | data |  tailroom | skb_shared_info |
 *   -----------------------------------------------
 *                                 + [page frag]
 *                                 + [page frag]
 *                                 + [page frag]
 *                                 + [page frag]       ---------
 *                                 + frag_list    --> | sk_buff |
 *                                                     ---------
 */
```

Пакет считается созданным, когда инициализирован `sk_buff`. Его и надо искать.


#todo нарисовать граф вызовов
## Создание сокета
1. `syscall socket() -> __sys_socket() ->__sys_socket_create() -> sock_create -> __sock_create()` 
2.  `sock_alloc()` и  `pf->create() -> inet_create()`. Здесь пытаемся найти протокол в `inetsw_array[]`, который знает, как именно инициализировать `struct sock`. Если не можем найти, пробуем подгрузить модуль ядра, и если не находим, то возвращаем ошибку.  
3. также в `inet_create()` вызываем `sk_alloc`
