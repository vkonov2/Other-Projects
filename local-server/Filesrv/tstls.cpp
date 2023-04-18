#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

int main() {
    DIR *dir = opendir(".");
    const struct dirent *entry = readdir(dir);
    if (entry == NULL) {
        printf("%s\n", strerror(errno));
        return 1;
    }
    while (entry != NULL) {
        unsigned char d_type = entry->d_type;
        if (d_type == DT_DIR) {
            printf("D   ");
        } else if (d_type == DT_LNK) {
            printf("L   ");
        } else {
            printf("    ");
        }
        printf("%s\n", entry->d_name);
        entry = readdir(dir);
    }
    closedir(dir);
    return 0;
}
