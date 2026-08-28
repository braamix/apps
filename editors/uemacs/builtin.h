/*	builtin.h
 *
 *	The files uemacs looks for at run time, compiled in.  mkdata.py
 *	writes the table; fileio.cpp serves it when nothing on disk answers.
 */
#ifndef BUILTIN_H_
#define BUILTIN_H_

struct builtin_file {
    const char *name;
    const char *text;
    unsigned size;
};

extern const struct builtin_file builtin_files[];

#endif /* BUILTIN_H_ */
