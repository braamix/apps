/*
 * The size of the root task's block: its frame and the arena of everything main awaits
 * (rt.c).  A member of libc.a of its own, so a program that defines the name gets its
 * own size and this one is never linked: the archive stands in for a weak symbol.
 */
const unsigned __braam_task_bytes = 64 * 1024;
