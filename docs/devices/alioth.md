# Redmi K40 (alioth)

A community port for SM8250-AC, maintained by FFT-infra.

- [FFT-infra/UniXNU-alioth](https://github.com/FFT-infra/UniXNU-alioth)
- [FFT-infra/UniXNU-sm8250-common](https://github.com/FFT-infra/UniXNU-sm8250-common)
- [FFT-infra/uefi-alioth](https://github.com/FFT-infra/uefi-alioth)
- [FFT-infra/UniXNU](https://github.com/FFT-infra/UniXNU)

## State

The format 2 tree selects shared SM8250 support and iPad7,1 / iPadOS 17.7.11 (21H461). The firmware includes display, buttons, UFS and USB integration. Kernel and boot-image production, followed by hardware validation of this layout, remain pending.

## Use this port

1. Read the pinned source and validation records below. Prepare the required firmware and system inputs.
2. Clone the device repository and follow its build guide from the checkout:

```sh
git clone --branch main https://github.com/FFT-infra/UniXNU-alioth.git
cd UniXNU-alioth
git checkout 32988f00809d629721d69a34967a384710f929fd
git submodule update --init --recursive
```

3. Complete the build for that revision, follow the [bring-up order](/porting/bring-up), and use [log capture](/porting/log-capture) to record the last confirmed stage.
4. Save results in the [porting record](/porting/records). See [Port a device](/porting/steps) for board structure and component selection.

## Sources

- [README](https://github.com/FFT-infra/UniXNU-alioth/blob/32988f00809d629721d69a34967a384710f929fd/README.md)
- [Porting record](https://github.com/FFT-infra/UniXNU-alioth/blob/32988f00809d629721d69a34967a384710f929fd/porting.toml)
