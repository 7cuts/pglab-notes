подпрпоект [cillium](cillium.md), но при этом работает отдельно

> [!В cравнение с bpftrace]
>Отчасти покрывает функционал [[bpftrace]] по трассировнию, только не надо писать программки под каждый сискол. Также есть более продвинутые фишки типа matchAction, которые позволяют осуществлять некоторые действия при ивенте.
>
>**Минус:** если хотим написать свою логику для мониторинга, то tetragon не подойдёт, так как он декларативный


штука, которой я могу указать список ивентов, которые хочу отлавливать (например, вход в `connect()`).
Когда ивент происходит, он логируется, и могут быть применены действия:
- послать процессу сигнал
- подменить retval системного вызова
- ...

## Отслеживание всех tcp_connect
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

## Посылка сигнала, который делает tcp_connect
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
        - "/usr/bin/curl"  # можно и по pid 
      matchActions:
      - action: Sigkill

```

результат:
```sh
❯ curl 1.1.1.1
[1]    837511 killed     curl 1.1.1.1
```
