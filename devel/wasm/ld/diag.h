// Errors, worded and separated as lld words and separates them: a message of
// several lines is set off from the next by a blank line, and the twentieth
// error (--error-limit) is the last.
#pragma once

#include "out.h"

struct Diag {
    Out text; // for stderr
    u32 errors   = 0;
    u32 limit    = 20; // 0: no limit
    bool stopped = false;

    void error(Str msg)
    {
        if (stopped)
            return;
        if (limit && errors >= limit) {
            emit("too many errors emitted, stopping now (use -error-limit=0 to see all errors)");
            stopped = true;
            return;
        }
        emit(msg);
        errors++;
    }

    bool failed() const { return errors != 0; }

    // --verbose: a line among the errors, where wasm-ld logs it.
    void log(Str msg) { text.put("ld: ").put(msg).put('\n'); }

private:
    bool sep_ = false;

    void emit(Str msg)
    {
        if (sep_)
            text.put('\n');
        text.put("ld: error: ").put(msg).put('\n');
        sep_ = msg.contains("\n");
    }
};
