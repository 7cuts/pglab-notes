#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <errno.h>
#include <unistd.h>

int create_hash_map(void) {
    int fd = bpf_map_create(BPF_MAP_TYPE_HASH, "hash_map_example",
                            sizeof(int),      // key_size
                            sizeof(int),     // value_size
                            1024,             // max_entries
                            NULL);            // map_flags
    if (fd < 0) {
        fprintf(stderr, "Failed to create hash map: %s\n", strerror(errno));
    }
    return fd;
}

int main() {
    int fd = create_hash_map();
    if (fd >= 0) {
        printf("Hash map created successfully with fd: %d\n", fd);

        // Insert elements in the hash map
        int key = 5;
        long value = 100;
        if (bpf_map_update_elem(fd, &key, &value, BPF_ANY) == 0) {  // BPF_ANY means insert or update
            printf("Element inserted or updated successfully: key = %d, value = %ld\n", key, value);
        } else {
            fprintf(stderr, "Failed to insert or update element: %s\n", strerror(errno));
            close(fd);  // Close the map before returning
            return -1;
        }

        // Update an element in the hash map
        value = 200;
        if (bpf_map_update_elem(fd, &key, &value, BPF_EXIST) == 0) {
            printf("Element inserted or updated successfully: key = %d, value = %ld\n", key, value);
        } else {
            fprintf(stderr, "Failed to insert or update element: %s\n", strerror(errno));
            close(fd);  // Close the map before returning
            return -1;
        }

        // Update an element that doesn't exist in the hash map
        key = 4;
        value = 300;
        if (bpf_map_update_elem(fd, &key, &value, BPF_EXIST) == 0) {
            printf("Element inserted or updated successfully: key = %d, value = %ld\n", key, value);
        } else {
            fprintf(stderr, "Failed to insert or update element with key=%d: %s\n", key, strerror(errno));
            close(fd);  // Close the map before returning
            return -1;
        }
        close(fd);
        return 0;
    }
    return -1;
}
