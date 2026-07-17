# AnalysisLib

A C++23 header-only library for interfacing with the `NowAnalysis` Python daemon. It provides RAII-based lifecycle management for the daemon and a client for posting analysis requests.

## Features

- **Daemon Management**: Launch and stop the Python daemon automatically using RAII.
- **JSON API**: Typed C++ structures for requests and responses.
- **Synchronous Client**: Easy-to-use client for health checks and analysis requests.
- **C++23**: Leverages modern C++ features.

## Requirements

- C++23 compatible compiler (MSVC 19.30+, GCC 12+, Clang 15+).
- Windows (for `CreateProcess`/`TerminateProcess` in `DaemonManager`).
- Python 3.x with dependencies (`pandas`, `matplotlib`, `reportlab`) for the daemon.

## Installation

This is a header-only library. You can include it in your project by adding the `include` directory to your include paths.

### CMake Integration

```cmake
add_subdirectory(analysis_lib)
target_link_libraries(your_target PRIVATE analysis_lib)
```

## API Reference

### `now_analysis::NowAnalysis` (Main Interface)

High-level class combining `DaemonManager` and `Client`.

- **`NowAnalysis(pythonExecutable, scriptPath, host, port)`**: Constructor. Launches the daemon and waits for it to be healthy.
- **`Analyze(AnalysisRequest)`**: Sends an analysis request.
- **`Shutdown()`**: Stops the daemon.
- **`GetClient()`**: Access the underlying client.
- **`GetDaemonManager()`**: Access the underlying daemon manager.

### `now_analysis::DaemonManager`

Handles the Win32 process lifecycle.

- **`DaemonManager(pythonExecutable, scriptPath, host, port)`**: Launches the daemon.
- **`Stop()`**: Terminates the process.
- **`GetPid()`**: Returns the process ID.

### `now_analysis::Client`

Handles HTTP communication.

- **`IsHealthy()`**: Returns `true` if the daemon is reachable and OK.
- **`Analyze(AnalysisRequest)`**: Returns `AnalysisResponse`.
- **`Shutdown()`**: Sends a shutdown request to the daemon.

## Sample Usage

```cpp
#include <NowAnalysis/NowAnalysis.hpp>
#include <iostream>
#include <filesystem>

int main() {
    try {
        std::wstring pythonPath = L"python"; 
        std::wstring scriptPath = L"C:\\path\\to\\create_now_analysis.py";

        // RAII: Launches daemon here
        now_analysis::NowAnalysis na(pythonPath, scriptPath);

        now_analysis::AnalysisRequest req;
        req.data_dir = "C:\\path\\to\\data";
        req.type = "ap";
        req.open_viewer = true;

        auto resp = na.Analyze(req);
        std::cout << "Status: " << resp.status << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    // RAII: Daemon stops here
    return 0;
}
```
