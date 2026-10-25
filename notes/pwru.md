packet, where are you?

рисунок: [pwru.excalidraw](Excalidraw/pwru.excalidraw.md)

---
Программка, которая позволяет получить информацию о нахождении пакета с точностью до ядерной функции. Также даёт подробную информацию об отклонённых пакетах:
- причина отмены
- на каком этапе

# Какие пробы использует?

- `kprobe:skb_by_stackid`
- `kprobe:skb_liftime_termination`
- `fexit:skb_clone`
- `fexit:skb_copy`
- `fentry:tc`
- `fentry:xdp
- `fexit:xdp`
- `k(ret)probe:veth_convert_skb_to_xdp_buff`
- `kprobe:bpf_map_update_elem`
- `kprobe:bpf_map_delete_elem`
- `k(ret)probe:bpf_map_lookup_elem`


# Как ловит пакеты
