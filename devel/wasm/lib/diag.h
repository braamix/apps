// Errors, worded and separated as lld words and separates them: a message of
// several lines is set off from the next by a blank line, and the twentieth
// error (--error-limit) is the last.
#pragma once

#include "out.h"

struct Diag {
    Str tool = "ld"; // what each line starts with
    Out text;        // for stderr
    u32 errors   = 0;
    u32 limit    = 20; // 0: no limit
    bool stopped = false;

    void error(Str msg) { error_at(tool, msg); }

    // An error that names its own place, such as file:line:col, where
    // error() names the tool.
    void error_at(Str where, Str msg)
    {
        if (stopped)
            return;
        if (limit && errors >= limit) {
            emit(tool, "error",
                 "too many errors emitted, stopping now (use -error-limit=0 to see all errors)");
            stopped = true;
            return;
        }
        emit(where, "error", msg);
        errors++;
    }

    bool failed() const { return errors != 0; }

    void warn(Str msg)
    {
        if (!stopped)
            emit(tool, "warning", msg);
    }

    // --verbose: a line among the errors, where wasm-ld logs it.
    void log(Str msg) { text.put(tool).put(": ").put(msg).put('\n'); }

private:
    bool sep_ = false;

    void emit(Str where, Str kind, Str msg)
    {
        if (sep_)
            text.put('\n');
        text.put(where).put(": ").put(kind).put(": ").put(msg).put('\n');
        sep_ = msg.contains("\n");
    }
};
