# KrEv

This is a minimal Visual Studio + WDK kernel-mode driver sample.

## Build

1. Open `KrEv.sln` in Visual Studio 2022.
2. Make sure the Windows Driver Kit (WDK) is installed.
3. Build the `Debug|x64` or `Release|x64` configuration.

## What it does

- Prints `KrEv: hello from kernel mode.` when the driver loads.
- Prints `KrEv: driver unloaded.` when the driver unloads.

## Notes

- This project is a simple legacy WDM sample meant for learning.
- Use a test machine or VM with test signing enabled when loading the built `.sys`.
