Рисунок: [packet-journey-drawing.excalidraw](Excalidraw/packet-journey-drawing.excalidraw.md)

По этой [статье](_sources/Article_The-Path-of-a-Packet-Through-the-Linux-Kernel.md) (пункт 4: Packet Flow) пакет проходит черел следующие этапы:
## Отправка
Здесь я указываю списком, но по факту следующая пункт может вызываться в предыдущем


### Transmission Layer: L4 to L3
1. Socket Layer - системный вызов `write()` или `sendto()`.  В статье рассматривается только `write()`.
2. `sock_sendmsg()`
3. Из `task_struct` получаем котнрольное сообщение, содержащее PID, UID, GID. С этой информацией проходим всякого рода проверки безопасности (например, SELinux)
4. `tcp_sendmsg()` Ждёт, когда tcp соединение будет установлено
5. `tcp_write_queue_tail()`. Биллдит TCP хедер и кладёт данные из юзер спейса в `sk_buff`. *Видимо, эта функция пишет пакет в write_queue()?*
6.  Если данные укладываются в существующий буфер, то вызывается `skb_add_data_nocache()`, иначе создаём новый буфер. Выставляем `transport_header` на начало построенного хедера.
7. Билдим хедер уровня L3 (IP). 
8. `tcp_write_xmit()` заводит *retransmission timers* 
9. `tcp_transmit_skb()` читает очередь на запись, в которую мы до этого положили пакет и отправляет их на уровень L3 через `queue_xmit()`

### Internet Layer: L3 to L2

1. ``__ip_queue_xmit()``. Определяем, куда роутить пакет. Если указан `sk_buff->skb_refdst`, то делать этого не надо. 
2. **Роутинг**. 
	1. Пытаемся понять адресата через поля `sk_buff` (если до этого на сокет пришёл какой-то IP пакет, то мы точно знаем, куда отправлять)
	2. Если не поняли, то смотрим **Forwarding Information Base (FIB)** - табличка, которая генерируется по таблице ip роутинга (???)
3. `ip_options_build()` - заполняем ip-опции. Также триггерит состояние `LOCAL_OUT` у фаервола `netfilter`.
4. `dst_output()` вызывает роутинг-функцию по указателю
5. **Состояние POST_ROUTING**. Вызывает `NF_INET_POST_ROUTING` hook. Он фрагментирует пакет, если размер превышает Maximum Transmission Unit (MTU)
6. `INET_LOCAL_OUT` hook. Затем `ip_output()` вызывает `ip_finish_output()`. Эта штука проверяет, что в `sk_buff` влезет MAC header. 
7. Остаётся только определить MAC-адрес получаелся. (функция `neigh_resolve_output()`). Для этого исползуется ARP (address resolution protocol). 
8. В конце создаём Ethernet header и добавляем его в `sk_buff`
### Ethernet Layer: L2 to L1

1. `dev_queue_xmit()` выставляет поле `mac_header` в `sk_buff`
2. `tc_egreses`. Добавляет пакет в очередь *queue_discipline* (`qdisc`). 
