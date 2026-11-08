# UniXNU-merlinx (iPadOS 17.7.11 / MT6768) 开发踩坑与“神奇特性”全记录

本文档汇总了 Redmi Note 9（merlinx / Helio G85 MT6768 / Mali-G52）移植 iPadOS 17.7.11（21H461）过程中遇到的所有隐蔽技术陷阱、非直觉行为（“神奇特性”），以及与 `ruby-ios-bringup`、`hakios-mt6768` 跨项目对比得出的关键教训。

---

## 一、 Bootloader / LK 与底层内存

### 1. 128 MiB 镜像全尺寸与 AVB Footer 校验
* **现象**：打包出的启动镜像若小于 128 MiB（例如精简为 36.7 MiB），小米 LK 在解包时报 `hdr_loaded` 错误，拒绝跳转内核。
* **原因**：LK 会校验分区末尾倒数 4 KiB 的 AVB Footer（`0x7ffffc0`）与头部元数据。若镜像尺寸不完整，尾部 AVB 签名错位或丢失。
* **规则**：必须保持严格的 128 MiB（134,217,728 B）全镜像布局，保留原厂 Gzip 内核流占位大小，且必须刷入 `boot_a`（`fastboot boot` 命令在小米 MTK LK 中未实现，`boot_b` 槽位逻辑异常）。

### 2. Framebuffer 扫描地址 vs 内核物理基址
* **现象**：将 console 写入内核 ATAGs 报告的基址 `0x7ba10000` 时，屏幕完全黑屏无显示。
* **神奇特性**：LK 真正送往显示面板的扫描缓冲区位于 `0x7cdfc000`（stride 4352）。内核必须直接向 `0x7cdfc000` 输出，而非 ATAG 声明的地址。

### 3. D-Cache 刷新非法物理地址 Panic
* **现象**：调用 `flush_dcache64(pa, len, TRUE)` 刷新屏幕缓冲区时触发内核 Panic：`phys_to_kv_range: illegal PA: 0x7cdfc000`。
* **原因**：`0x7cdfc000` 落在 XNU 内核托管的物理 RAM 范围之外（属于预留的显示硬件专用区）。
* **对策**：绝不能用物理地址刷新！必须转换为虚拟地址进行虚存缓存清理：`flush_dcache64((addr64_t)fPixels, len, FALSE)` 并配合 `dsb sy` 内存屏障。

---

## 二、 显示驱动与渲染管线（DISP RET）

### 1. `DISP RET` 假死与真死之谜（生产端饥饿）
* **现象**：开机初期显示 `RET 5/5`，随后跳到 `6/6`、`8/8`，最终停留在 `10/10`。
* **直觉误判**：容易被误认为“显示后端卡死、掉帧或丢帧”。
* **真实机理**：**分子永远等于分母**！这证明送入 GOP 渲染后端的每一个 Frame 都被 100% 成功 Retire（完成扫描输出）了，显示驱动和硬件管线完全畅通。数字停在 10 是因为**上游（SpringBoard）遭遇死锁停产**，不再产生新帧（Producer Starvation），绝非消费端故障。

### 2. `sheng-vsync=120` 与面板真实刷新率不匹配
* **现象**：`DISP RET` 会在 `5/5`、`6/6` 等位置随机停住，每次重启停点不同；`DISP COPY` 成功、`DISP VAL` 通过、分子恒等于分母。
* **原因**：`board.toml` 沿用了 120Hz 机型的 `sheng-vsync=120` + `sheng-vsync-fixed=1`。这会：
  1. 让定时器按 8.3ms（120Hz）节奏发送 vsync；
  2. 通过 `getRefreshAttributes` 向 render server 谎报面板固定为 120Hz，使其按 120fps 排帧。
  而这块面板的 GOP 扫描输出没有 vblank 中断，120Hz 只是人为定时器，render server 更容易在等待下一帧时挂死。
* **正确做法**：改为 `sheng-vsync=60`（不要 `-fixed`）：
  * `startVsync` 对 `hz=60` 仍会启动定时器；
  * `registerService` 的 `hz>60` 条件不成立，不再发布伪造的 `IOFBDisplayRefresh`/刷新属性；
  * `getRefreshAttributes` 的 `hz<=60` 分支直接返回原始 1/60 属性，render server 回到自带的 1/60 心跳。

### 3. `DISP RET` 停点随机：先分清“client 不提交”还是“vsync 发送阻塞”
* **诊断设计**：在 `package/MerlinxLegacyDisplay.cpp` 中用独立 `thread_call` 每 2 秒往第 32 行打印：
  `VS s<sent> a<arm> i<idle> l<late> n<inNotify>`
  * `n` 由包裹原厂 `native_legacy_vsync_notify` 的 wrapper 置位/清零（`n=1` 表示正卡在原厂发送函数内部）。
  * 该 `thread_call` 不在 display 的 work loop 上，work loop 卡死时仍能打印。
* **判读表**：
  | 现象 | 含义 |
  | :--- | :--- |
  | `s` 持续增长 | 定时器正常，是 client（SpringBoard/lsd 那条线）不再提交 swap |
  | `s` 不动、`n=1` | 卡在原厂 `native_legacy_vsync_notify` 内（大概率阻塞式发消息） |
  | `s` 不动、`n=0`、`i` 刚增长 | tick 因无 client 自行停止，之后无人重新 arm |
  | `s` 不动、`n=0`、`i` 不动 | 定时器没再触发，work loop 卡在别处 |
* **实现细节**：原厂发送函数通过链接别名 `native_legacy_vsync_notify_original` 绑定，wrapper 不修改 GopFramebuffer 子模块，避免向无权推送的上游仓库提交改动。

### 4. 第 34 行诊断在 1080x2340 面板上被裁掉
* **现象**：加了 `VS s… a… i… l… n…` 后，屏幕上完全看不到这一行。
* **原因**：UEFI GOP 与面板是 **1080x2340**，而 `native_gop_text` 把第 N 行画在 `320 + N*60`。第 34 行从 `2360` 开始，已经在 2340 之外，整行不可见。
* **对策**：改到第 32 行（`2240..2264`），或任何 `320 + N*60 + 24 <= 2340` 的行（N ≤ 33）。

### 5. AppleKeyStore 与 Legacy Display Mode
* **现象**：SpringBoard 启动后直接抛出 `DisplayMonitorException` 崩溃退出。
* **原因**：SpringBoard 在初始化显示时必须找到合法的 display monitor 和 `sheng-aks=1`（AppleKeyStore 状态发布）。必须提供 Legacy GOP 伪装单元缩放与相位参数（`offline-legacy-register=1 offline-legacy-gop=1`），让其认定存在合法的虚拟显示管道。

---

## 三、 用户态初始化与 launchd 的隐蔽特性

### 1. `rd=md0` 的 RAM 启动身份欺骗（来自 Hakios 的关键发现）
* **神奇特性**：`launchd` 启动时会读取 boot-args 中的 `rd=` 参数。
* **机理**：如果 boot-args 中没有 `rd=md0`（或没有相应的非物理设备参数），`launchd` 会认定自身运行于一台“真实的物理设备”上，从而强行启动完整的硬件安全阶梯（等待硬件 SEP 响应、Keybag 状态解密、Ignition 里程碑）。
* **后果**：在无 SEP 的第三方机型上，系统守护进程层会无限期等待该里程碑，导致 `backboardd`、`runningboardd` 甚至连 `posix_spawn` 都不会被触发。

### 2. `ignition_level=1` 与 `launchd_unsecure_cache=1`（来自 Ruby 的经验）
* **现象**：即使 Plist 标记为 `RunAtLoad=True`，launchd 依然不拉起后台服务。
* **解法**：必须在 boot-args 传入：
  * `launchd_unsecure_cache=1`：禁用 launchd 对 plist 缓存的加密信任链校验。
  * `ignition_level=1`：直接跳过多阶段 Ignition 检查，放行守护进程加载。
  * `wdt=-1`：彻底禁用用户态及内核看门狗，防止进程因等待响应被强行 SIGKILL。

### 3. iOS 17 Launch Constraints（启动约束）拦截
* **现象**：`posix_spawn` 返回错误，或服务被 AMFI 拒绝拉起。
* **原因**：iOS 17 引入了比 iOS 16 更严苛的 Launch Constraints 策略。非原厂平台签名的 owner-trusted 或 cryptex 二进制文件即使在 TrustCache 中，也会被 `mac_proc_check_launch_constraints` 拒绝。
* **对策**：内核需应用 `zzyc-owner-skip-launch-constraints.patch`，并在 boot-args 中传入 `sheng-no-lc=1` 直接跳过约束校验。

### 4. launchd 命名空间遮盖与探测器
* **坑点**：launchd 读取的 `/System/Library/LaunchDaemons` 极易受 union 挂载点或 bindfs 覆盖影响。若挂载顺序颠倒，会导致目录为空或丢失核心 job。
* **解法**：在内核 `load_init_program` 完成后引入 `J 11111`（5 大关键 plist）与 `P 11111`（5 大关键可执行文件）的原生 vnode 查找探测，直观判断是否在根视图被遮盖。

### 5. `disabled.501.plist` 阻止用户域服务崩溃风暴
* **现象**：`com.apple.ScreenTimeAgent` 在启动后不断退出又被 KeepAlive 重新拉起（返回码 `R2:6`，即 SIGABRT）。
* **原因**：该服务运行在 uid 501 用户域，且在无 iCloud/SEP 环境下必然崩溃。
* **对策**：在 Data 种子的 `db/com.apple.xpc.launchd/` 中，必须同时部署 `disabled.plist`（系统域）和 `disabled.501.plist`（用户域），将其显式禁用。

---

## 四、 进程死锁、崩溃与系统架构死结

### 1. SpringBoard `x16 = -47` (`mach_msg2_trap`) 死锁
* **现场**：SpringBoard（PID 26）采样显示：
  `S26 pc 1cb780b58 lr 1cb784224 sp 16ef56900 x16 -47 f1 1cb7831b0 f2 1cb783fb8`
* **机理**：`x16 = -47` 是 Mach 同步消息陷阱 `mach_msg2_trap`。SpringBoard 在发出 Mach 请求后阻塞在 `mach_msg2_receive`，等待服务提供者的应答：
  1. 等待 `backboardd` 托管的 `com.apple.CARenderServer` 连接以构建窗口层。
  2. 等待 `lsd` 处理 `_LSContextInitCommon` 响应。
  若 `backboardd` 未起或 `lsd` 崩溃，SpringBoard 将永远死等，导致整个 UI 管线停摆。

### 2. `lsd` 在 `_objc_msgSend` 中的 SIGSEGV（`EX 263 lsd b R2:b`）
* **现场**：PC `0x1800e9068`（libobjc `_objc_msgSend` +0x1068），LR `0x1800edcd4`。指令 `ldr p14, [x0]` 尝试从接收者寄存器 `x0` 加载类指针，但 `x0` 为野指针（如 `0x100b020` / `0x119d4258`）。
* **Ruby 的历史教训（重大避坑指南）**：
  * `lsd` 首次启动必然走 `InstalledContentLibrary.__GenerateBuiltInAppInstallSessionID`，并在内部向 MobileGestalt 请求会话信息。
  * **切忌给信号量打超时补丁**：Ruby 曾尝试把内核等待信号量改为超时即退（`mov x1, #-1 -> #0`），结果上层函数拿到了未初始化的空数据，进入 `_objc_msgSend` 时瞬间触发与我们完全相同的 `_objc_msgSend` SIGSEGV 崩溃循环！
  * **唯一有效绕过**：在 dyld shared cache 中定位该函数调用 MobileGestalt 的 `bl` 指令，将其打为 4 字节 `nop`（`02 25 5e 95` -> `1f 20 03 d5`）。

### 3. `containermanagerd` 的 `personas` 阻塞陷阱（来自 Hakios）
* **现象**：`containermanagerd` 启动后会反复向用户管理器查询非苹果机型上根本不存在的 `personas`，连续以 1 秒为间隔超时重试 20 次，期间所有容器查询请求全被锁死。
* **Hakios 解法**：通过 `DYLD_INSERT_LIBRARIES` 注入代码，将 Objective-C 方法 `+[MCMUserIdentityCache personasAreSupported]` 强制篡改返回 `NO`，彻底消除轮询等待。

### 4. `zzyb` 补丁中的硬编码 UUID 幽灵缓存
* **陷阱**：在 `zzyb-native-cryptex-os-view.patch` 中，有一段尝试清除 LaunchServices 缓存的代码：
  `/private/var/mobile/Containers/Data/InternalDaemon/<other-device-UUID>/Library/Caches`
* **事实**：该 UUID 是其他移植设备生成后的随机散列值，在全新的 Data 种子卷上根本不存在该路径，该清理逻辑实际上每次都返回 ENOENT。

---

## 五、 内核调试与事件环（Event Ring）

### 1. Throttle 槽位太小导致的事件环淹没 Bug
* **现象**：开机约几十秒后，事件环被重复出现的 `SV fairplayd.A2`、`cryptexd` 占满，早期（如前 100 秒）的所有真实进程启动/崩溃记录全被冲走。
* **原因**：`zzze` 补丁中的节流表宏定义 `#define NATIVE_DIAG_THROTTLE_SLOTS 8` 只有 8 个槽位。当周期性守护进程不断唤醒时，槽位迅速被填满并从 slot 0 开始覆盖，导致已经被记录过的进程名被当作“首次启动”，进而无限重触发打印。
* **对策**：将节流槽位扩展至 32 或 64，保证整个开机生命周期内的事件历史完整保留。

### 2. Phase 1 突破：`zzyb` Cryptex 视图挂载顺序 Bug（`lsd` 崩溃消失的主因）
* **现象**：根分区上指向 `/System/Cryptexes/OS/...` 的符号链接全部悬空（`ENOENT`），`cryptexd` 每 10 秒重启，`lsd` 在 `libobjc +0x1068` 反复 `SIGSEGV`。
* **真因**：`zzyb-native-cryptex-os-view.patch` 把 `native_cryptex_os_view()`、`native_gestalt_cache_drop()` 和 `native_ls_store_drop()` 放在了 `native_gpu_addon_prepare()` 末尾，而该函数第一行就是 `if (!PE_parse_boot_argn("sheng-gpu-addon", ...)) return;`！没开 `sheng-gpu-addon=1` 时这三个函数每次开机都被直接跳过。
* **效果**：将这三个调用移到 `sheng-gpu-addon` 判断之前无条件执行后，`ScreenTimeAgent`（位于 `/System/Cryptexes/OS/...`）能被正常找到并启动（PID 153），`cryptexd` 崩溃循环彻底消失，`lsd` 的 `UG/UF` 硬件异常也完全消失。

### 3. Phase 2 四个隐蔽诊断陷阱（2026-11-07）
1. **64 槽位在 PID ~80 填满导致后续进程与 `PS` 静默丢失**：
   * iPadOS 17.7.11 开机会拉起 120～150+ 个不同名字的守护进程。`NATIVE_DIAG_THROTTLE_SLOTS = 64` 且满槽返回 `FALSE` 时，PID ~80 之后启动的所有新进程（含 `lsd`）全部被静默丢弃；且 `PS` 被包在节流判断内，从此锁死不再更新。
   * **修复**：扩到 `256` 槽；`PS` 移到左侧独立第 19 行无条件实时刷新，6 个核心服务（`sb/bb/rb/ls/cm/cf`）全部带 `*` 历史标记。
2. **全屏白苹果帧拷贝直接擦除右侧 64 行事件环**：
   * 当第 5/6 帧（`DISP RET 5/5 F5`）整屏拷贝进 GOP framebuffer 时，白苹果画面会覆盖整个屏幕（包括右侧已经画好的事件环）。
   * **修复**：新增 `native_gop_redraw_ring()`，在每次帧退休（`DISP RET`）和每 4 秒焦点采样时自动重绘右侧全部 64 个非空槽位；并去掉 `SP`/`SV` 噪音。
3. **`iomfb_fdr_loader` 覆盖 `DISP CLIENT` PID**：
   * `native_legacy_client_pid` 在每次任何进程调用 `IOServiceOpen(IOMobileFramebufferLegacy)` 时都会被覆盖。PID 82（`iomfb_fdr_loader`）只是打开过 framebuffer，并不是提交那 5/6 帧的进程。
   * **修复**：在 `native_legacy_swap_validation_observed()` 中单独记录真正提交画面的进程 `S<pid>:<name>`，第 30 行显示 `DISP RET 5/5 F5 S<提交PID>:<名> C<总打开次数>/<最后打开PID>:<名>`。
4. **`mach_msg2_trap` 前 4 层调用栈全在 `libsystem_kernel.dylib` 内**：
   * `pc: mach_msg2_trap -> lr: mach_msg2_internal -> f1: mach_msg_overwrite -> f2: mach_msg` 全部在 `libsystem_kernel` 内部，只回溯 2 层（`f1/f2`）永远看不到上层调用者；且 `containermanagerd` 名字 17 字节与 16 字节 `strcmp` 失配。
   * **修复**：栈回溯扩到 8 层（`f1..f8`），`native_diag_tracked_slot()` 改为 `strncmp(..., 16)`。

### 4. Phase 3（2026-11-07）：追踪名字、崩溃栈与 Data 种子
1. **`xpcproxy` 启动的守护进程名字被记成 `xpcproxy`**：
   * `exec_activate_image()` 开头在 `IMGPF_EXEC` 下把 `p` 设为 `current_proc()`（旧 proc）；`exec_mach_imgact()` 的新名字写进了 `vfs_context_proc(imgp->ip_vfs_context)`（shadow proc）。于是 `native_diag_note_exec(p->p_pid, p->p_comm)` 对所有经 `xpcproxy` SETEXEC 启动的进程（`backboardd`、`lsd`、`runningboardd`、`containermanagerd`、`cfprefsd`）都传了 `"xpcproxy"`；只有 `SpringBoard`（launchd 直接 posix_spawn）被正确记录。
   * **修复**：优先读 `np = vfs_context_proc(imgp->ip_vfs_context)` 的 `p_name`/`p_comm`。
   * 这也解释了 `PS` 长期显示 `bb0`/`rb0`/`ls0`，而 `DISP RET` 的 `S56:backbo` 证明 `backboardd` 其实一直在跑。
2. **`lsd` 崩溃点不是 `_objc_msgSend`，而是 `objc_storeStrong -> objc_retain`**：
   * `0x1800edcd4` = `libobjc.A.dylib + 0x5cd4`（`objc_storeStrong` 的 `str x19,[x21]`，返回地址）；
   * `0x1800e9068` = `libobjc.A.dylib + 0x1068`（`objc_retain` 的 `ldr x17,[x17,#0x20]`，`x17 = isa & ISA_MASK`）；
   * 传入的 `obj`（`x0 = 0x1014dcbf0`）不是有效 ObjC 对象，其首字被当作 isa 后指向未映射地址。
   * **诊断增强**：`UF` 后追加 `UO`（`[x0]`/`[x0+8]`）、`U1/U2/U3`（6 层栈），直接定位 `lsd` 在 LaunchServices 的哪个函数里调用了 `objc_storeStrong`。
3. **`SpringBoard` 的同步 XPC 等待链**（21H461 反汇编确认）：
   * `pc 0x1cb780b58` = `libsystem_kernel mach_msg2_trap`；`f3 0x18f589538` = `libdispatch _dispatch_mach_send_and_wait_for_reply`；`f5 0x1dd673df0` = `libxpc xpc_connection_send_message_with_reply_sync`；`f6 0x186e80d10` = Foundation 的 `__NSXPCCONNECTION_IS_WAITING_FOR_A_SYNCHRONOUS_REPLY__`；`f8 0x186eecb10` = `-[NSXPCDistantObjectSimpleMessageSend1 ...]`。
   * 结论：`SpringBoard` 通过 `NSXPCConnection` 同步等 `lsd` 的回复；`lsd` 崩溃导致其永久阻塞。
4. **Data 种子补齐**：新增 `db/lsd`、`containers/{Bundle/Application,Data/SystemGroup,Data/InternalDaemon,Data/PluginKitPlugin,Data/Temp}`、`containers/Shared/SystemGroup/systemgroup.com.apple.{lsd.iconscache,mobilegestaltcache}`、`mobile/Containers/...`、`mobile/Library/Caches/com.apple.{LaunchServices,IconsCache}`；`disabled.plist` 与 `disabled.501.plist` 同时禁用 `ScreenTimeAgent`、`dmd`、`UsageTrackingAgent`、`cryptexd`。

---

## 六、 上游同步与构建链（2026-11-06）

### 1. 合并上游 `UniXNU/UniXNU` main（e2fdae4，423 个提交）
* 冲突集中在 `scripts/unixnu/*`、`build-xnu-4k-baseline.sh`、`components.toml`、`.gitmodules` 与大量文档。
* 取舍原则：
  * 设备自身的 `board.toml`、`porting.toml`、`devices/merlinx` 记录、macOS workflow 保留本地版本；
  * `scripts/unixnu/*` 采用上游实现，但保留本板的 512 字节 ramdisk 对齐、DTB 追加开关、`append_dtb`；
  * `build-xnu-4k-baseline.sh` 同时接受 `XNU_SOURCE_STORAGE` 与 `XNU_NATIVE_EMMC`，并保留本板补丁需要的 `kern_exit/vfs_syscalls/sys_generic/kern_descrip` 的 `XNU_OWNER_CACHE_MOUNT` 编译开关；
  * `components.toml` 以上游为底，再删除 Adreno740/AdrenoMetal，补回 NativeEmmc、MaliG52、MaliMetal；
  * `.gitmodules` 保留三个驱动的删除，并补上上游新增的 `drivers/QcomBaseband`，否则递归子模块 checkout 会失败。

### 2. 上游资料里对本问题真正有用的内容
* `docs/porting/display.md`（上游新增段）：
  > 一次 300 秒的启动完成了三次 swap、没有内核 panic，并显示了 Apple logo；Home-screen 启动仍未验证。**在把“静止的 logo”当成 framebuffer 传输失败之前，先抓 display-server 与 SpringBoard 的等待链。**
  这与我们 `DISP RET` 分子恒等于分母、停在上游不再提交的观察完全一致：显示后端没问题，问题在用户态。
* `docs/porting/display.md`：刷新策略必须按实测面板模式设置；`sheng-vsync=120` 只是 sheng 参考板的取值。→ 我们的 60Hz 修正正确。
* `docs/guide/mobilegestalt.md`：MG 缓存位于 `systemgroup.com.apple.mobilegestaltcache/.../com.apple.MobileGestalt.plist`，按 build 与平台 UUID 缓存；身份变化后旧答案会保留。这解释了为什么“删缓存/没有缓存”会影响 lsd 的 MG 路径。
* 构建链实际收益：新的 `scripts/unixnu/kernel.py` 会把 OS profile 的 `[root] os_volume_uuid` 映射成 `XNU_OWNER_ROOT_VOLUME_UUID`，而新的 `build-xnu-4k-baseline.sh` 在 owner boot manifest 下强制要求它。旧的本地脚本没有这条映射，升级后由上游补齐。

### 3. 上游带来的新构建前置条件（CI 上实际踩到）
* `unixnu build payload` 现在要求 `--data-ramdisk`（`CONFIG_XNU_OWNER_DATA_MOUNT=y` 时）。用 `.cache/data-seed-v4.dbr`（SHA `dff2e3c2...`，与 panel26/27 同一个种子）。
* `make-merlinx-adt.py` 要求 `--unique-chip-id` 与 `--chip-id` 同时给出；沿用 `0x123456789abcdef0`。
* CI 的 payload 步骤不会自带 Mu-Silicium 的 `Font.h`；本地打包时用 `UNIXNU_UEFI=<uefi-merlinx checkout>`。

---

### 3. Ruby 启动参数对照（2026-11-06）

| Ruby `0155-boot.cmdline` | 我们（panel32） | 作用 |
| :--- | :--- | :--- |
| `rd=ufsroot0` | **已加** `rd=ufsroot0` | `IOKitBSDInit` 只在 `rd=md0` 时走 RAM root；非 `md0` 且 ADT 有 `root-matching` 时不会按 BSD 名找根，安全。launchd 可能用 `rd=` 的存在判断“不是真机启动任务”。 |
| `wdt=-1 wdt_test=no_user_checkin` | `wdt=-1` + **已加** `wdt_test=no_user_checkin` | 关闭看门狗与用户态 check-in 要求 |
| `launchd_unsecure_cache=1` | 已有 | 跳过 launchd job cache 信任链 |
| `ignition_level=1` | 已有 | 跳过 ignition 阶梯 |
| `keepsyms=1` | **已加** | panic 保留符号名 |
| `-noprogress` | **已加** | 跳过启动进度任务 |
| `unentitled_ios_sim_launch=1` | 不适用 | 代码在 `#if XNU_TARGET_OS_OSX` 内，我们编的是 `XNU_TARGET_OS_IOS` |
| `cpus=6` | 保持 8 | Ruby 已验证 cpus=6 与 8 的栈完全一致，不是 LS 死锁原因 |
| `debug=0xc` | 保持 `0x14e` | 仅日志级别，不是原因 |
| `sweetfb_hz=30` | 保持 `sheng-vsync=60` | 不同显示驱动；且 render server 从未注册 vsync client（`VS a0`） |

**结论**：Ruby 的参数主要服务于日志、看门狗和 root 设备命名；他们真正越过 LS/MG 靠的是 **ICL 4 字节 NOP** + 完整拉取的 `/private/var` 首启状态，不是 boot-args。

---

## 七、 Phase 6–11 的“雷霆问题”（2026-11-08）

这一节记录从“所有守护进程都起来”到“SpringBoard 等 lsd”之间真正卡住过的坑。

### 1. 拦截 `MobileGestaltHelper` 反而更糟（Phase 6 → 7）
* **现象**：Phase 6 把 `MobileGestaltHelper` 的 spawn 改成返回 `ENOENT` 后，`backboardd` 连第一帧都画不出来，`lsd` 仍卡在 `0x1ad27c6e8`。
* **原因**：plist 在只读根上，`launchd` 仍会创建并持有 `com.apple.mobilegestalt.xpc` 的 Mach 接收端口；程序不启动，消息只会在内核队列里积压。`backboardd`/`lsd` 的同步 XPC 请求永远等不到 `CONNECTION_INVALID`。
* **正确做法**：恢复 `MobileGestaltHelper`，改为解决它的 workqueue 自锁（见下一条）。

### 2. `wq_max_parallelism = 8` 被 `workq_open()` 覆盖回 1（Phase 7 → 8）
* **Bug 1（时序）**：`load_init_program()` 在 launchd 进入用户态**之前**执行 boost；launchd 首次 `workq_open()` 时 `wq_init_constrained_limit == 1`，又用 `qos_max_parallelism()`（按在线 CPU 数 = 1）把 `wq_max_parallelism[0..5]` 覆盖回 1。
* **Bug 2（判断落空）**：`workq_open()` 里的 `num_cpus` 来自 `ml_wait_max_cpus()`，返回的是 **ADT 声明的 CPU 总数 8**，不是在线数 1；`if (num_cpus < 8)` 永远为假。
* **修复**：
  * 在 `workq_open()` 赋值**之后**逐桶检查 `if (wq_max_parallelism[_wq_bucket(qos)] < 8) ... = 8;`
  * 导出 `wq_init_constrained_limit`，boost 时置 0；
  * 单独生成 `zzzf-workqueue-parallelism.patch`（手写 hunk 曾被 `git apply` 拒绝，用 `diff -u` 生成才可靠）。

### 3. Jetsam 批量杀进程（`EX … 9 R1:f`）
* **现象**：约 1007 秒出现满屏 `R1:f`。
* **原因**：`owned_memory_size` 只有 896 MiB，且 `studentd`、`biomed`、`assistant_cd`、`corespeechd`、`siriinferenced`、`itunescloudd`、`nfcd`、`nearbyd`、`OTATaskingAgent` 等后台进程 crash-loop，持续制造内存压力。
* **修复**：`owned_memory_size` → `0x50000000`（1.25 GiB）；上述进程在 `disabled.plist`、`data-seed.json` 与 spawn 拦截表中禁用。
* **判断**：Jetsam 只杀后台 → 资源调优；若杀到 `SpringBoard`/`backboardd`/`lsd`，说明还有泄漏或自旋。

### 4. `fairplayd.A2` 每 10 秒 `exit(0)`（正常但刷屏）
* **原因**：无 FairPlay/SEP 硬件，进程启动后立即 `exit(0)`；plist 的 `KeepAlive` 让 launchd 每 10 秒重拉，`ThrottleInterval` 默认 10 秒。
* **修复**：加入 disabled 与 spawn 拦截；左侧 `EXIT` 行只在 `rv != 0` 时刷新，避免正常退出覆盖真正的崩溃记录。

### 5. 每次开机误删 LaunchServices 数据库（Phase 9）
* **现象**：`lsd` 每 ~125 秒崩溃重启，反复扫描应用。
* **原因**：`native_ls_store_drop()` 用 `/private/var/ngpu/ls-store-rebuilt-1` 标记只删一次；但 Data 是内存盘，标记每次重启都不存在，于是每次开机都把预置的 `com.apple.LaunchServices-*.csstore` 删掉，`lsd` 只能从零重建。
* **修复**：改为仅在显式 `merlinx-drop-ls-store=1` 时删除（与 `merlinx-drop-mg-cache` 一致）。

### 6. `lsd` 同步等 `iconservicesagent`（Phase 9）
* **现象**：`lsd` 主线程卡在 `f10:1ad27c6e8`（`MobileGestaltExtensions`/`NSXPCConnection` 同步等待），紧跟在 `GO lsd` 之后是 `GO iconservicesagen`。
* **原因**：`lsd` 建库时同步请求 `com.apple.iconservices.iconservicesagent` 刷新图标缓存；该服务在无用户态 GPU 驱动时挂死。
* **修复**：在 disabled plist / data seed / spawn 拦截表加入 `iconservicesagent`，客户端立即收到 `XPC_ERROR_SERVICE_UNAVAILABLE`。

### 7. `lsd` 在 `objc_release` 释放已释放对象（Phase 10 → 11）
* **现象**：`UG/UF` 的 `pc = 0x1800e9068`、`lr = 0x1800edcd4`；`x0` 首字是 `libsystem_malloc` 空闲链表的 `(cookie << 60) | (next >> 4) | 1`。
* **机理**：最低位为 1，`objc_release` 把它当 nonpointer isa；`isa & ISA_MASK` 得到右移 4 位的假类指针（`0x100cfee8` 等，落入 `__PAGEZERO` 下方），读 `cls + 0x20` 时精确段错误。
* **修复**：在 `handle_user_abort` 中识别该模式，打印 `UR <pid> <name> <fault>`，把 `x0` 置 0、`pc` 退回 `lr`，等效于 `objc_release(nil)`。
* **Phase 11 扩展**：同一清理路径还有第二个叶函数 `0x1800eac14`；改为匹配 `pc ∈ [0x1800e0000, 0x180120000)` 且 fault 在 `0x10000000..0xffffffff`，覆盖所有同类释放。
* **注意**：这是恢复/绕过，不是修复 `lsd` 的悬垂指针本身；如果后续出现别的 `objc_release` 地址，仍要单独确认。

### 8. 采样器的小坑（Phase 8 → 9）
* **16 字符截断**：`boot-ui-focus=MobileGestaltHel` 是 16 字符，而进程名 `MobileGestaltHelper` 是 19 字符；`native_diag_focus_same()` 必须按前 16 字符前缀比较。
* **空闲线程**：workqueue 线程停在 `__workq_kernreturn`（`x16 = 368/-368`）、`ulock_wait`（515）、`ulock_wait2`（544）或 `pc == 0`，采样要跳过或排到最后，否则永远只看到空闲线程。
* **多线程**：`lsd`/`SpringBoard` 的阻塞线程不一定是 0 号；按 `pid/thread/total` 轮询，才能抓到持锁线程。

### 9. ADT / 首启状态的小坑（Phase 4）
* **伪造 `boot-manifest-hash` 导致 EAUTH**：`offline-owner-boot-manifest.patch` 会把 `/chosen/boot-manifest-hash` 当成真实 OS manifest 去认证；固定 SHA-384 字符串直接让 SystemOS mount 失败并 panic `EAUTH (80) @ kern_exec.c:8733`。必须让该属性缺失，由内核计算真实值。
* **P3 + 无 Metal**：`artwork-display-gamut = P3` 会让 CoreUI 走 P3/Metal 路径；无用户态 GPU 时 `CUIShapeEffectStack` 回退 `CI::GLContext::init()` 并 `blr x8`（x8=0）崩溃。改为 `sRGB`，并用 text-fixup 把 `dyld_shared_cache_arm64.11,34f3b84` 的 `bl` 改成 `mov x0, xzr`。
* **DataMigrator 文件名**：iOS 17 读 `com.apple.DataMigration.plist`（大写 D/M），只写旧 `com.apple.migration.plist` 会被重跑迁移；内核在 launchd 前同时写两份并写 `disabled.migrated`。

### 10. 构建链的小坑
* `unixnu build payload` 现在要求 `--data-ramdisk`（`CONFIG_XNU_OWNER_DATA_MOUNT=y`）。
* `make-merlinx-adt.py` 要求 `--unique-chip-id` 与 `--chip-id` 同时给出。
* CI 的 payload 步骤没有 Mu-Silicium 的 `Font.h`；本地打包用 `UNIXNU_UEFI=<uefi-merlinx checkout>`。
* 对“补丁文件”再打补丁时，外层 hunk 的行数很容易错；`git apply` 报 `corrupt patch` 时用 `git apply --recount` 或重新生成 diff，不要手改 `@@` 计数。

---

## 八、 总结与快速排查法则

1. **看屏幕左侧 `J` 和 `P`**：如果非 `11111`，说明挂载遮盖，查 union/bindfs。
2. **看 `PS` 行有没有 `!`**：如果服务是 `0` 且无 `!`，说明 launchd 压根没尝试 spawn 它，查 `ignition_level`、`rd=` 和 `launchd_unsecure_cache`。
3. **看 `RET X/Y`**：若 $X=Y$，说明显示层毫无问题，死锁在用户态。
4. **看 `EX ... R2:b`**：如果是 `lsd` 报 SIGSEGV，不要去改内核信号量，检查 Data 种子结构或考虑 ICL 调用的 4 字节 NOP 绕过。
