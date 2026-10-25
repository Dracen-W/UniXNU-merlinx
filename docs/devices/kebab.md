# OnePlus 8T (kebab)

A community-maintained port for SM8250. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-kebab](https://github.com/GFander/UniXNU-kebab) | [GFander](https://github.com/GFander) |
| [UniXNU-kebab](https://github.com/stevedang67889/UniXNU-kebab) | [stevedang67889](https://github.com/stevedang67889) |
| [KebabRamDisk](https://github.com/stevedang67889/KebabRamDisk) | [stevedang67889](https://github.com/stevedang67889) |

## State

The GFander port reaches MMU enablement, `arm_init`, `PE_init_platform` and the early framebuffer console on hardware. KVA mapping and `_enable_timebase_event_stream` diagnostics remain open; the timer reports an invalid bit index.

The stevedang67889 port separately verifies firmware, menu, physical buttons and logs. Kernel and payload builds are recorded; its firmware UFS integration remains pending.

## Sources

- [README: GFander/UniXNU-kebab](https://github.com/GFander/UniXNU-kebab/blob/5d609fa28397f352103510eb37efe50b8570737f/README.md)
- [README: stevedang67889/UniXNU-kebab](https://github.com/stevedang67889/UniXNU-kebab/blob/06dc6ca9704f07b2a7b5922173dba302f3a09f33/README.md)

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
