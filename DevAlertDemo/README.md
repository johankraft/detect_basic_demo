**DFM**: The DevAlert client library. This provides an API for generating "alerts" (errors/warnings) that typically include "payloads", provided by the TraceRecorder and CrashCatcher libraries.

**TraceRecorder**: The tracing library for Percepio Tracealyzer. This is configured to use the "RingBuffer" stream port, that keeps the most recent trace data in memory until requested.

**CrashCatcher**: Provides core dumps on Arm Cortex-M devices, that can be loaded into GDB. This library also include fault exception handlers to catch hard faults. Based on Adam Green's [CrashCatcher](https://github.com/adamgreen/CrashCatcher) library.

More information about this demo is found in the main README file in the root folder.
