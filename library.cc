#include <libelf.h>
#include <fcntl.h>
#include <unistd.h>
#include <gelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "library.h"

// 函数用于读取动态链接库依赖
int analyze_executable_libraries(const char* path, std::vector<std::string>& libs) {
    int fd, result = -1;
    Elf *elf;
    Elf_Scn *scn = NULL;
    GElf_Shdr shdr;
    Elf_Data *data;

    if (elf_version(EV_CURRENT) == EV_NONE) {
        fprintf(stderr, "ELF library initialization failed: %s\n", elf_errmsg(-1));
        printf("[DEBUG] failed1");

        return -1;
    }

    if ((fd = open(path, O_RDONLY, 0)) < 0) {
        printf("[DEBUG] failed2 %s", path);
        perror("open");
        return -1;
    }

    if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
        fprintf(stderr, "elf_begin() failed: %s\n", elf_errmsg(-1));
                printf("[DEBUG] failed3");

        goto cleanup;
    }

    while ((scn = elf_nextscn(elf, scn)) != NULL) {
        if (gelf_getshdr(scn, &shdr) != &shdr)
            continue;

        if (shdr.sh_type == SHT_DYNAMIC) {
            if ((data = elf_getdata(scn, NULL)) == NULL)
                continue;

            int count = shdr.sh_size / shdr.sh_entsize;

            // 遍历动态段，寻找 NEEDED 条目
            for (int i = 0; i < count; ++i) {
                GElf_Dyn dyn;
                if (gelf_getdyn(data, i, &dyn) != &dyn)
                    continue;

                if (dyn.d_tag == DT_NEEDED) {
                    // 输出动态链接库名
                    printf("Needed library: %s\n", elf_strptr(elf, shdr.sh_link, dyn.d_un.d_val));
                    libs.emplace_back(elf_strptr(elf, shdr.sh_link, dyn.d_un.d_val));
                }
            }
            result = 0; // 成功处理动态段
        }
    }

cleanup:
    elf_end(elf);
    close(fd);
    return result;
}
