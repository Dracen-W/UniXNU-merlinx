# Redmi K80（zorn）

SM8650-AB 社区设备移植，由 kerrinz 维护。

- [kerrinz/UniXNU-zorn](https://github.com/kerrinz/UniXNU-zorn)
- [kerrinz/uefi-zorn](https://github.com/kerrinz/uefi-zorn)

## 当前状态

设备已运行 UEFI、菜单、DarwinBoot 和 XNU 平台初始化。只读 UFS 探针读取 userdata 中的 SHNGROOT 清单，将 OS 的 APFS 容器注册为 `md0`，根卷已挂载，屏幕记录 `APOK`。

当前停在用户映射加载过程中的 `MEND`，此时两次 `vm_map_delete` 调用已返回。负载另含恢复内存盘。触控、GPU 和桌面待完成。负载使用固件仓库的打包器及其 512 字节 FAT 簇设置。

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

- [README: kerrinz/UniXNU-zorn](https://github.com/kerrinz/UniXNU-zorn/blob/8e9d3a9cce8ea1f4e0641a124bc22baf7f4fd712/README.md)
