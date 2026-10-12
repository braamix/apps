#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

coro(braam_call *) int main(int argc, char **argv)
{
    struct stat st;
    char line[64], cwd[256];
    if (await mkdir(argv[1], 0755) < 0 || await chdir(argv[1]) < 0)
        return 1;
    FILE *f = await fopen("t.txt", "w");
    if (!f)
        return 2;
    fprintf(f, "one\ntwo\nthree\n");
    fwrite("four\n", 1, 5, f);
    if (await fclose(f) != 0 || await stat("t.txt", &st) < 0)
        return 3;
    printf("size %d reg %d dir %d\n", (int)st.st_size, S_ISREG(st.st_mode), S_ISDIR(st.st_mode));
    f = await fopen("t.txt", "r");
    int n = 0;
    while (await fgets(line, sizeof line, f))
        printf("%d: %s", ++n, line);
    printf("eof %d err %d\n", feof(f) != 0, ferror(f) != 0);
    await rewind(f);
    int c = await fgetc(f);
    ungetc(c, f);
    printf("first %c again %c\n", c, await getc(f));
    await fseek(f, -6, SEEK_END);
    printf("tell %ld\n", await ftell(f));
    char buf[8] = { 0 };
    printf("read %d '%s'\n", (int)await fread(buf, 1, 5, f), buf);
    if (await fstat(fileno(f), &st) == 0)
        printf("fstat size %d\n", (int)st.st_size);
    await fclose(f);
    f = await fopen("t.txt", "a");
    fputs("five\n", f);
    await fclose(f);
    await stat("t.txt", &st);
    printf("appended %d\n", (int)st.st_size);
    await mkdir("d", 0755);
    await rename("t.txt", "d/u.txt");
    int old = await stat("t.txt", &st);
    printf("old %d new %d\n", old, await stat("d/u.txt", &st));
    int full = await rmdir("d");
    printf("rmdir full %d %s\n", full, strerror(errno));
    await chdir("d");
    if (await getcwd(cwd, sizeof cwd))
        printf("cwd ends %s\n", strrchr(cwd, '/'));
    int u = await unlink("u.txt");
    await chdir("..");
    int r = await rmdir("d");
    await chdir("..");
    printf("unlink %d rmdir %d rmdir %d\n", u, r, await rmdir(argv[1]));
    return 0;
}

