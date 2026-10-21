Это cli утилита, с помощью которой можно быстро писать трассирующие eBPF программы. 
# События
Мы можем трейсить разные типы событий (ссылки ведут на статью на Хабре с примерами)

- Специальные: [`BEGIN`, `END`](https://habr.com/ru/articles/542560/#events-begin-end)
- kprobes: [`kprobe`, `kretprobe`, `uprobe`, `uretprobe`](https://habr.com/ru/articles/542560/#events-kprobes)
- BPF trampolines: [`kfunc`, `kretfunc`](https://habr.com/ru/articles/542560/#events-bpf-trampolines)
- bpf tracepoints: [`tracepoint`](https://habr.com/ru/articles/542560/#events-tracepoints)
- Статическая отладка в пространстве пользователя: [`usdt`](https://habr.com/ru/articles/542560/#events-usdt)
- perf: [`software`, `hardware`, `profile`, `interval`, `watchpoint`](https://habr.com/ru/articles/542560/#events-perf)


  > [!INFO] 
  >  Вообще `tracepoint` событий гораздо меньше, чем `kprobe` событий, но зато у них **стабильный API**, так как первые явно объявлены в коде ядра, а `kprobe` - хак, о котором разработчик функции не знает. 