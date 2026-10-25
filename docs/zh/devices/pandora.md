# 小米 17 Pro（pandora）

搭载 SM8850 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [data-a-live](https://github.com/ShueiTsuki/data-a-live) | [ShueiTsuki](https://github.com/ShueiTsuki) |

## 当前状态

iOS 16.7.16 已进入主屏幕和锁屏，触控及物理按键可用。移植使用设备加载器、原生 UFS，以及 Android userdata 中由文件承载的根卷。

Adreno 840 已集成 Vulkan 和 Metal，维护者记录了首个 GPU 渲染的主屏幕画面。快速交互会出现系统挂起，当前默认使用 CPU 渲染，GPU 稳定性仍在排查。

## 来源

- [README: ShueiTsuki/data-a-live](https://github.com/ShueiTsuki/data-a-live/blob/1a6c05fbc03695adb41a636210b9c48f40e14563/README.md)

## 构建

内核由 GitHub Actions 工作流 `xnu.yml` 以 `machine_config=PANDORA` 构建。仓库中的脚本在 WSL 中构建 ADT 和负载，用 Android NDK 构建加载器：

```sh
python3 tools/mkprelink.py <kernel> <kernelcache> <out>
KERNEL=<out> tools/boot_ios.sh --file /data/local/tmp/iosroot.img --run <N> --rd ufsroot0
```

`tools/boot_ios.sh` 解析固定根文件的 extent，构建负载和加载器镜像，并用 `fastboot boot` 启动。内核、kernelcache、根镜像和信任缓存是外部输入，不存放在仓库中。

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
