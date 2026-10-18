

# CLI
### Как искать, что хотим трейсить
```sh
sudo bpftrace -l | cut -d: -f1 | sort -u 
```

**Вывод**:
```
fentry
hardware
iter
kprobe
rawtracepoint
software
tracepoint
```
Странно, что тут нет uprobe? Навреное, это часть kprobe...

Далее, чтобы получить вполне конкрентые функции
```sh
> sudo bpftrace -l "tracepoint:tcp_*"
```

Получаем такой вывод. Это уже можно использовать в bpftrace-программах
```
tracepoint:tcp:tcp_ao_handshake_failure
tracepoint:tcp:tcp_ao_key_not_found
tracepoint:tcp:tcp_ao_mismatch
tracepoint:tcp:tcp_ao_rcv_sne_update
tracepoint:tcp:tcp_ao_rnext_request
tracepoint:tcp:tcp_ao_snd_sne_update
tracepoint:tcp:tcp_ao_synack_no_key
tracepoint:tcp:tcp_ao_wrong_maclen
tracepoint:tcp:tcp_bad_csum
tracepoint:tcp:tcp_cong_state_set
tracepoint:tcp:tcp_destroy_sock
tracepoint:tcp:tcp_hash_ao_required
tracepoint:tcp:tcp_hash_bad_header
tracepoint:tcp:tcp_hash_md5_mismatch
tracepoint:tcp:tcp_hash_md5_required
tracepoint:tcp:tcp_hash_md5_unexpected
tracepoint:tcp:tcp_probe
tracepoint:tcp:tcp_rcv_space_adjust
tracepoint:tcp:tcp_rcvbuf_grow
tracepoint:tcp:tcp_receive_reset
tracepoint:tcp:tcp_retransmit_skb
tracepoint:tcp:tcp_retransmit_synack
tracepoint:tcp:tcp_send_reset
tracepoint:tcp:tcp_sendmsg_locked
```