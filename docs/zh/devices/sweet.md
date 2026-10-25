# Redmi Note 10 Pro（sweet）

搭载 SM7150 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-sweet](https://github.com/CrackerCat/UniXNU-sweet) | [CrackerCat](https://github.com/CrackerCat) |

## 当前状态

源码包含 Sweet ADT、ArmCpu 和 DarwinBoot 覆盖层，以及 Adreno 618 HFI 传输实现。当前源码记录了内核和平台静态库构建，以及 QEMU CPU 注册和持续的定时器中断。自发 IPI 投递待验证。

设备固件集成、Apple 组件预链接和根卷目标待完成。准备镜像前，在仓库根目录运行宿主机检查。

## 来源

- [README: CrackerCat/UniXNU-sweet](https://github.com/CrackerCat/UniXNU-sweet/blob/d3dd6fdf9e889f08b2564d92f7fef5b5c913263e/README.md)

## 构建

主机检查：

```sh
python3 scripts/check-host.py
python3 scripts/make-sweet-adt.py --output out/sweet.adt
```

设备仓库集成：

```sh
python3 ../UniXNU/unixnu defconfig sweet_defconfig -D . --out out/driver
python3 ../UniXNU/unixnu status -D . --out out/driver
```

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
