// qos - crash reports on Windows.
#pragma once

namespace qos::crash {

// On an unhandled exception (or abort), writes logs\crash-<pid>.txt (exception, faulting
// address, host stack as module+offset) and logs\crash-<pid>.dmp (minidump) next to
// qos.exe.
void Install();

}  // namespace qos::crash
