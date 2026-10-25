# Redmi K80（zorn）

SM8650-AB 社区设备移植，由 kerrinz 维护。

- [kerrinz/UniXNU-zorn](https://github.com/kerrinz/UniXNU-zorn)
- [kerrinz/uefi-zorn](https://github.com/kerrinz/uefi-zorn)

## 当前状态

12 GB / 256 GB 设备已运行 UEFI、菜单、DarwinBoot 和 XNU 平台初始化。iPadOS 17.7.11 恢复内存盘已作为 APFS `md0` 挂载，内核打开 `/sbin/launchd` 后停在 `parse_machfile` 调用前，屏幕显示 `PARSE`。

原生 UFS、触控和 GPU 支持待完成。负载使用固件仓库的 `scripts/make-zorn-payload.py` 打包，其中 FAT 簇大小设为 512 字节。

## 使用此移植

1. 阅读下方固定版本的源码说明和验证记录，准备该设备要求的固件和系统输入。
2. 克隆设备仓库，在检出目录内按其构建指南准备依赖：

```sh
git clone --branch main https://github.com/kerrinz/UniXNU-zorn.git
cd UniXNU-zorn
git checkout af23788d9ca7d48ff1576ff83664bbac975ba3ad
git submodule update --init --recursive
```

3. 在该版本完成对应构建后，按[启动验证顺序](/zh/porting/bring-up)验证，并按[日志抓取](/zh/porting/log-capture)保存最后确认的阶段。
4. 将结果写入[移植记录](/zh/porting/records)。板级结构和组件选择见[移植设备](/zh/porting/steps)。

## 来源

- [README](https://github.com/kerrinz/UniXNU-zorn/blob/af23788d9ca7d48ff1576ff83664bbac975ba3ad/README.md)
