### Как искать, что хотим трейсить

```sh
sudo bpftrace -l | cut -d: -f1 | sort -u
```

Далее, чтобы получить вполне конкрентые функции

```sh
> sudo bpftrace -l "tracepoint:tcp_*"
```
