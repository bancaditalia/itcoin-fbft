# SqliteModernCpp library

`sqlite-modern-cpp` (https://github.com/SqliteModernCpp/sqlite_modern_cpp) is a
C++14 wrapper around the SQLite library.

Modified from version 34f9b076348d731e0f952a8ff264a26230ce287b:
    https://raw.githubusercontent.com/SqliteModernCpp/sqlite_modern_cpp/34f9b076348d731e0f952a8ff264a26230ce287b/hdr/sqlite_modern_cpp.h

## ITCOIN-specific modification
The library was modified adding the SQLITE_DETERMINISTIC flag in the
implementation of `sqlite_modern_cpp::define()` function.

In this way, **EVERY** User Defined Functions will be interpreted as
deterministic.

This means that they can be used in generated columns. On the other side, since
the change is blindly made to every possible UDF, it is no longer possible to
reliably register non deterministic functions.

A better way would have been to introduce a dedicated `defineDeterministic()`
method, and leave `define()` unchanged.

As of 2021-10-20, this modification was **not** proposed upstream.

# License

MIT License

Copyright (c) 2017 aminroosta

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
