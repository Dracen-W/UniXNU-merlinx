# Redmi K40（alioth）

SM8250-AC 社区设备移植，由 FFT-infra 维护。

- [FFT-infra/UniXNU-alioth](https://github.com/FFT-infra/UniXNU-alioth)
- [FFT-infra/UniXNU-sm8250-common](https://github.com/FFT-infra/UniXNU-sm8250-common)
- [FFT-infra/uefi-alioth](https://github.com/FFT-infra/uefi-alioth)
- [FFT-infra/UniXNU](https://github.com/FFT-infra/UniXNU)

## 当前状态

格式 2 设备树选择共享 SM8250 支持及 iPad7,1 / iPadOS 17.7.11（21H461）。固件包含显示、按键、UFS 和 USB 集成。该布局的内核及启动镜像生成、实机验证待完成。

## 使用此移植

1. 阅读下方固定版本的源码说明和验证记录，准备该设备要求的固件和系统输入。
2. 克隆设备仓库，在检出目录内按其构建指南准备依赖：

```sh
git clone --branch main https://github.com/FFT-infra/UniXNU-alioth.git
cd UniXNU-alioth
git checkout 32988f00809d629721d69a34967a384710f929fd
git submodule update --init --recursive
```

3. 在该版本完成对应构建后，按[启动验证顺序](/zh/porting/bring-up)验证，并按[日志抓取](/zh/porting/log-capture)保存最后确认的阶段。
4. 将结果写入[移植记录](/zh/porting/records)。板级结构和组件选择见[移植设备](/zh/porting/steps)。

## 来源

- [README](https://github.com/FFT-infra/UniXNU-alioth/blob/32988f00809d629721d69a34967a384710f929fd/README.md)
- [移植记录](https://github.com/FFT-infra/UniXNU-alioth/blob/32988f00809d629721d69a34967a384710f929fd/porting.toml)
