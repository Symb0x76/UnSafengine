# UnSafengine

Unpack Safengine 2.3.x ~ 2.4.0 protected executables.

## Prerequisite

[Intel Pin 3.18 (Windows MSVC)](https://www.intel.cn/content/www/cn/zh/developer/articles/tool/pin-a-binary-instrumentation-tool-downloads.html).

Extract Pin 3.18 into "{Workspace}/pin".

## Build

Requires Visual Studio 2019 or higher.
Build two projects.
A pintool dll file and a CUI executable will be copied into "{Workspace}/pintool".

## Usage

The following command will unpack a safengine protected executable.

```
UnSafengine.exe -deob .\mb_se.exe
```
