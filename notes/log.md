
--------------------------------------------
## про `ss`  (socket statistics)

### позволяет смотреть
- таймеры
	- retransmission
	- keepalive
	- persist (не знаю что это)
- сколько было retransmissions
- буферы на сокетах
- сколько пакетов дропнуто до прихода на сокет
- размер бэклога (пакеты, которые может прочитать процесс моментально, но не читает)
- может закрыть сокет 
- очень много инфы по сокетам
	- время с последней отправки пакета
	- время с последнего приёма пакета
	- время с последнего приёма ACK
	- сколько байтов подтверждено
	- сколько байтов получено
	- сколько сегментов отправлено
	- сколько сегментов получено

### с филтрацией по:
- pid
- listen/non-listen
- ipv4/ipv6
- protocol (tcp, udp, etc..)

ещё есть интеграция с ebpf. можно через ebpf привязать мапу к сокету, а через эту программу красиво эту инфу выводить (не тестировал)

-------------------------------------
## что можно взять из bcc

Для каждой подзадачи привожу ссылку на соответствующий инструмент из bcc/tools,
если такой есть. Берём те сисколы, которые используют в bcc и либо сами пишем программки
на bpftrace, либо тырим код.

| задача                            | инструмент                      |
| --------------------------------- | ------------------------------- |
| создание пакета                   |                                 |
| bind сокета                       | bindsnoop.py                    |
| tcp listen                        | solisten.py, tcpaccept.py       |
| активные подключения              | tcpconnect.py                   |
| состояния перегрузки              | tcpcong.py                      |
| дропы пакета со стек трейсом ядра | tcpdrop.py                      |
| retransmitions                    | tcpretrans.py                   |
| передача пакета                   |                                 |
| приём пакета                      |                                 |
| **общая top-подобная стата**      | tcptop                          |
| заполненность буферов             | bpf_itertor по сокетам или `ss` |

есть ещё netqtop. его, как мне кажется, нет смысла использовать, так как
он показывает распределение пакетов по очередям на сетевой карте. к
мониторингу процессов это не имеет никакого отношения.

---------------------------------------------------------

Внутри ядра есть функции для eBPF со стабильным api:

- Generate random numbers
- Get current time & date
- eBPF map access
- Get process/cgroup context
- Manipulate network packets and forwarding logic

**dtrace** - есть такая штука, работает на BPF. По сути bpftrace - это наследние Dtrace.

-----------------------------------------------------------

### netlink (7) - коммуникация между ядром и user-space через через сокет `AF_LINK`

`sock_diag(7)` - Получить отчёт по сокетам. **заполненность буферов на чтение, запись**

можно посмотреть:

- число pending connections для слушающего сокета размер данных в входящей очереди
- для слушащего сокета: backlog size (ещё не обработанные? не понял сам)
- размер данных, которые готовы к отправке
- таймеры на TCP сокета:
  1.  retransmit timer
  2.  keep-alive timer
  3.  TIME_WAIT timer
  4.  zero window probe timer

С помощью netlink смог собрать вот такую статистику по сокетам.
Запускал под нагрузкой от `iperf3`

```sh
iperf3 -s # term 1
iperf3 -c 127.0.0.1
```

Вывод:
Список всех открытых tcp сокетов. (Можно фильтровать по `inode` и `/proc/pid/fd`)

```
-------------------------------
(большая часть сокетов выглядит так)
inode = 1178146
idiag_state = 1: ESTABLISHED
SK_MEMINFO_RMEM_ALLOC	0
SK_MEMINFO_RCVBUF	    131072
SK_MEMINFO_WMEM_ALLOC	0
SK_MEMINFO_SNDBUF	    2626560
SK_MEMINFO_FWD_ALLOC	0
SK_MEMINFO_WMEM_QUEUED	0
SK_MEMINFO_OPTMEM	    0
SK_MEMINFO_BACKLOGS	    0
SK_MEMINFO_DROPS	    0
-------------------------------
(а вот тут необычно)
inode = 1178147
idiag_state = 1: ESTABLISHED
SK_MEMINFO_RMEM_ALLOC	  0
SK_MEMINFO_RCVBUF	      131072
SK_MEMINFO_WMEM_ALLOC	  0
SK_MEMINFO_SNDBUF	      4194304
SK_MEMINFO_FWD_ALLOC	  1216
SK_MEMINFO_WMEM_QUEUED	  133952
SK_MEMINFO_OPTMEM	      0
SK_MEMINFO_BACKLOGS	      960
SK_MEMINFO_DROPS	      0
-------------------------------
```

---------------------------------------

### bpf iteratos

https://docs.kernel.org/6.2/bpf/bpf_iterators.html

можно периодически итерировать по каждому файлу внутри процесса (итератор `bpf_iter__task_file` + параметр `target_pid`)
файла -> сокет -> буферы (всё, что есть в struct socket)

## tetragon

подпрпоект [[cillium]], но при этом работает отдельно

> [!В cравнение с bpftrace]
> Отчасти покрывает функционал [[bpftrace]] по трассировнию, только не надо писать программки под каждый сискол. Также есть более продвинутые фишки типа matchAction, которые позволяют осуществлять некоторые действия при ивенте.
>
> **Минус:** если хотим написать свою логику для мониторинга, то tetragon не подойдёт, так как он декларативный

штука, которой я могу указать список ивентов, которые хочу отлавливать (например, вход в `connect()`).
Когда ивент происходит, он логируется, и могут быть применены действия:

- послать процессу сигнал
- подменить retval системного вызова
- ...

### Отслеживание всех tcp_connect

Вот под такой конфиг

```yaml
apiVersion: cilium.io/v1alpha1
kind: TracingPolicy
metadata:
  name: "monitor-tcp-connections"
spec:
  kprobes:
    - call: "tcp_connect"
      syscall: false
      args:
        - index: 0
          type: "sock"
```

с помощью этой команды

```sh
❯ sudo tetra getevents --processes 'firefox' | jq .
```

обновив, вкладку в браузере, я получил такой вывод (я взял только 1 ивент)

```json
{
  "process_kprobe": {
    "process": {
      "exec_id": "YmVuYmVuOjcwMDA3NDAwMDAwMDAwOjgxNDUzMw==",
      "pid": 814533,
      "uid": 1000,
      "cwd": "/home/alex",
      "binary": "/usr/lib/firefox/firefox",
      "flags": "procFS",
      "start_time": "2026-09-15T21:24:46.683339557Z",
      "auid": 1000,
      "parent_exec_id": "YmVuYmVuOjI1MTAwMDAwMDAwOjEwNjM=",
      "refcnt": 3,
      "tid": 814556,
      "in_init_tree": false
    },
    "parent": {...}
    "function_name": "tcp_connect",
    "args": [
      {
        "sock_arg": {
          "family": "AF_INET",
          "type": "SOCK_STREAM",
          "protocol": "IPPROTO_TCP",
          "saddr": "10.98.56.228",
          "daddr": "151.101.65.155",
          "sport": 37360,
          "dport": 443,
          "cookie": "18446614267298931072",
          "state": "TCP_SYN_SENT"
        }
      }
    ],
    "action": "KPROBE_ACTION_POST",
    "policy_name": "monitor-tcp-connections",
    "return_action": "KPROBE_ACTION_POST"
  },
  "node_name": "benben",
  "time": "2026-09-16T15:35:32.722282303Z"
}
```

### Посылка сигнала, который делает tcp_connect

```yaml
apiVersion: cilium.io/v1alpha1
kind: TracingPolicy
metadata:
  name: "kill-tcp-connections-by-binary"
spec:
  kprobes:
    - call: "tcp_connect"
      syscall: false
      args:
        - index: 0
          type: "sock"
      selectors:
        - matchBinaries:
            - operator: "In"
              values:
                - "/usr/bin/curl" # можно и по pid
          matchActions:
            - action: Sigkill
```

результат:

```sh
❯ curl 1.1.1.1
[1]    837511 killed     curl 1.1.1.1
```
