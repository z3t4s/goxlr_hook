# GoXLR control panel hook
![](https://github.com/z3t4s/goxlr_hook/blob/main/example.gif?raw=true)

## Why?
Because having APIs is apparently not cool anymore, duh.

## What does this project do right now?
Next to nothing. It identifies the constructor of JUCE::Slider in the application, performs a detour hook on it and collects information about every Slider that is created.

It then starts a worker thread that notifies a callback everytime a value change happens.


## How to build
Make sure that you have [CMake](https://cmake.org/download/) installed!

1. Open git prompt and do a **recursive!!** clone
2. Open VS developer console
3. cd "folder\path"
4. `cmake -B build`
5. Enter the build folder
6. Open the freshly generated goxlr_hook.sln
7. Build for release x86

## How to install
1. Download and install the [CFF Explorer Suite](https://ntcore.com/?page_id=388) 
2. Copy your freshly built DLL from `folder\path\build\Release\goxlr_hook.dll` to `C:\Program Files (x86)\TC-Helicon\GOXLR`
3. Use CFF explorer to add a new import to the `GoXLR App.exe`

There is a [tutorial video](https://streamable.com/wfyq3y) on how to do this

## License
IDGAF do what you want with it. Please respect the license of the dependency minhook