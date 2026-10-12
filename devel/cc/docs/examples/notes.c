/*
 * notes — a notebook kept in a file: the worked example of docs/Braam_Example.md.
 *
 *     notes                list the notes, numbered
 *     notes add WORDS...   add a note
 *     notes del N          delete note N
 *     notes ask            add the lines typed, one note each, until ^D
 *
 * Build it with `vcc -t wasm32-braam notes.c -o notes`.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NOTES "notes.txt"
#define SPARE "notes.new"

/* Every function that waits for the system is a coroutine, up to main. */
static coro(braam_call *) int list(void)
{
    FILE *f = await fopen(NOTES, "r");
    if (!f) {
        if (errno != ENOENT) {
            await perror(NOTES);
            return 1;
        }
        await printf("no notes\n");
        return 0;
    }
    char line[256];
    int n = 0;
    while (await fgets(line, sizeof line, f))
        await printf("%3d  %s", ++n, line);
    await fclose(f);

    struct stat st;
    if (await stat(NOTES, &st) == 0)
        await printf("(%d notes, %d bytes)\n", n, (int)st.st_size);
    return 0;
}

/* fprintf fills the stream's buffer, writing it out when full; fclose writes the rest. */
static coro(braam_call *) int add(int argc, char **argv)
{
    FILE *f = await fopen(NOTES, "a");
    if (!f) {
        await perror(NOTES);
        return 1;
    }
    for (int i = 0; i < argc; i++)
        await fprintf(f, i ? " %s" : "%s", argv[i]);
    await fputc('\n', f);
    return await fclose(f) == 0 ? 0 : 1;
}

/* Copy every note but the k-th to a spare file, then put the spare in its place. */
static coro(braam_call *) int del(int k)
{
    FILE *in = await fopen(NOTES, "r");
    if (!in) {
        await perror(NOTES);
        return 1;
    }
    FILE *out = await fopen(SPARE, "w");
    if (!out) {
        await perror(SPARE);
        await fclose(in);
        return 1;
    }
    char line[256];
    int n = 0;
    while (await fgets(line, sizeof line, in))
        if (++n != k)
            await fputs(line, out);
    await fclose(in);
    await fclose(out);
    if (k < 1 || k > n) {
        await printf("there is no note %d\n", k);
        await remove(SPARE);
        return 1;
    }
    return await rename(SPARE, NOTES) == 0 ? 0 : 1;
}

/* Standard input is a stream too: reading it writes out the prompt first. */
static coro(braam_call *) int ask(void)
{
    char line[256];
    int added = 0;
    for (;;) {
        await printf("note? ");
        if (!await fgets(line, sizeof line, stdin))
            break;
        char *end = strchr(line, '\n');
        if (end)
            *end = 0;
        if (!line[0])
            continue;
        char *words[] = { line };
        if (await add(1, words) != 0)
            return 1;
        added++;
    }
    await printf("\n%d added\n", added);
    return 0;
}

coro(braam_call *) int main(int argc, char **argv)
{
    if (argc == 1)
        return await list();
    if (!strcmp(argv[1], "add") && argc > 2)
        return await add(argc - 2, argv + 2);
    if (!strcmp(argv[1], "del") && argc == 3)
        return await del(atoi(argv[2]));
    if (!strcmp(argv[1], "ask") && argc == 2)
        return await ask();
    await fprintf(stderr, "usage: notes [add words... | del n | ask]\n");
    return 2;
}
