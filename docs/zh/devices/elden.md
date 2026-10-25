# 联想拯救者 Y700 四代（elden）

SM8750P 社区设备移植，由 jerry-2009 维护。

- [jerry-2009/UNIXNU_elden](https://github.com/jerry-2009/UNIXNU_elden)

## 当前状态

TB322FC 使用同一镜像完成了两次内存冷启动。菜单、1904x3040 GOP 显示、音量键输入和 FAT 负载可用，下一阶段需要准备可启动 XNU 的镜像。

使用仓库固定的第三方源码、固件覆盖层和阶段 1 构建步骤。设备驱动目录包含 `EldenRamDiskDxe`。

## 使用此移植

1. 阅读下方固定版本的源码说明和验证记录，准备该设备要求的固件和系统输入。
2. 克隆设备仓库，在检出目录内按其构建指南准备依赖：

```sh
git clone --branch main https://github.com/jerry-2009/UNIXNU_elden.git
cd UNIXNU_elden
git checkout c591b1a0a471f24978aba25c56c8f6ec9bd06668
git submodule update --init --recursive
```

3. 在该版本完成对应构建后，按[启动验证顺序](/zh/porting/bring-up)验证，并按[日志抓取](/zh/porting/log-capture)保存最后确认的阶段。
4. 将结果写入[移植记录](/zh/porting/records)。板级结构和组件选择见[移植设备](/zh/porting/steps)。

## 来源

- [README](https://github.com/jerry-2009/UNIXNU_elden/blob/c591b1a0a471f24978aba25c56c8f6ec9bd06668/README.md)
- [阶段 1 构建](https://github.com/jerry-2009/UNIXNU_elden/blob/c591b1a0a471f24978aba25c56c8f6ec9bd06668/docs/PHASE1.md)
- [实机结果](https://github.com/jerry-2009/UNIXNU_elden/blob/c591b1a0a471f24978aba25c56c8f6ec9bd06668/docs/PHASE1-STATUS.md)
