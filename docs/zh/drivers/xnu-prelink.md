# XnuPrelink

先按[适配流程](/zh/porting/platform)准备板级输入、修改源码、接入构建并逐步验证。

内核预链接生成器与 ABI 工具。

[源码仓库](https://github.com/UniXNU/XnuPrelink).

XnuPrelink 将构建的 XNU 内核绑定到所选原始 kext，并写出预链接方案。OS 配置提供 kernelcache 固定版本、根组件、slide 和 ABI 设置。

## 集成

组件目录记录实现范围、硬件与 OS 要求、板级依赖、源码版本、构建阶段、输出和运行时标识符。所选配置按依赖顺序运行构建命令。

```sh
./unixnu build prelink -D ../UniXNU-sheng
```

[组件目录](/zh/reference/components)
