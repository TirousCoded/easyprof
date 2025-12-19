# EasyProf
## Overview
*EasyProf* is an *easy to use*, *single header file*, C++ performance profiler library.

`example.cpp` provides a *quick-n'-dirty example-based tutorial* on how to use EasyProf.

The `easyprof.h` header file acts as *de-facto documentation* on the API frontend.

EasyProf is thread-safe in terms of multiple distinct profiler instances being able
to be used concurrently for profiling seperate threads. However, EasyProf currently
does not support one profiler profiling multiple threads at once, but this is
something I'd like to add at some point.

EasyProf is *alright* from a performance standpoint, however I've not done vary much
optimizing yet, so the API may suffer a bit from processing time overhead.

## Setup
EasyProf can just be used out-of-the-box by just cloning the repo and then copying,
or otherwise referencing, `easyprof.h` in your codebase.

To build the example app, you'll need to download the [`Premake`](https://premake.github.io)
build system, and put it on your *path*. Then, just run `setup.bat` to build the solution.

Users on Mac/Linux will need to run `Premake` manually. I don't know how to write equivalent
shell scripts for Mac/Linux, *sorry*.