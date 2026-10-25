# ShengPreflight

Start with the [adaptation procedure](/porting/platform) for board inputs, source changes, build integration and verification order.

UEFI preflight checks for sheng. Validates the firmware volume, memory map and boot inputs before a build.

| Repository | Maintainer | Scope |
| --- | --- | --- |
| [ShengPreflight](https://github.com/UniXNU/ShengPreflight) | [UniXNU](https://github.com/UniXNU) | UEFI preflight checks for sheng. Validates the firmware volume, memory map and boot inputs before a build. |

## State

sheng only.

## Build

The device selects preflight scripts in `[firmware] preflight`. `./unixnu build firmware` invokes them before building the FD. Use the target board's preflight implementation and firmware inputs.
