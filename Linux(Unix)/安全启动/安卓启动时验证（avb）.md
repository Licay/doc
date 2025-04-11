https://source.android.google.cn/docs/security/features/verifiedboot?hl=zh-cn

## 设备状态

设备状态指明了能够以多大的自由度将软件刷写到设备上，以及是否强制执行验证。设备状态为 `LOCKED` 和 `UNLOCKED`。状态为 `LOCKED` 的设备禁止您将新软件刷写到设备上，而状态为 `UNLOCKED` 的设备允许您进行修改。

当设备开机后，引导加载程序会先检查设备状态是 `LOCKED` 还是 `UNLOCKED`。如果设备状态为 `UNLOCKED`，引导加载程序会向用户显示警告，然后继续启动，即使加载的操作系统并非由信任根签名也是如此。

如果设备状态为 `LOCKED`，引导加载程序会完成[验证启动](https://source.android.google.cn/docs/security/features/verifiedboot/verified-boot?hl=zh-cn)中的步骤，以验证该设备的软件。只有加载的操作系统由信任根正确签名后，状态为 `LOCKED` 的设备才会启动。如需了解详情，请参阅[启动流程](https://source.android.google.cn/docs/security/features/verifiedboot/boot-flow?hl=zh-cn)。

### 更改设备状态

如需[更改设备状态](https://source.android.google.cn/docs/core/architecture/bootloader/locking_unlocking?hl=zh-cn)，请使用 `fastboot flashing [unlock | lock]` 命令。为了保护用户数据，只要设备状态发生了变化，都会先擦除 data 分区中的数据，并会在删除数据之前要求用户确认。

用户购买二手开发设备后，应将设备状态从 `UNLOCKED` 改为 `LOCKED`。锁定设备后，只要没有警告，用户就应该确信设备所处的状态符合设备制造商的设计。如果开发者出于开发目的而希望停用设备上的验证功能，应将设备状态从 `LOCKED` 改为 `UNLOCKED`。

### 信任根

信任根是用来为设备上存储的 Android 副本进行签名的加密密钥。信任根的不公开部分仅为设备制造商所知，用来为将要分发的每个 Android 版本进行签名。信任根的公开部分嵌入到设备中，并存储在一个不会被篡改的位置（通常是只读存储区）。

加载 Android 时，引导加载程序会使用信任根来验证真实性。如需详细了解此流程，请参阅[验证启动](https://source.android.google.cn/docs/security/features/verifiedboot/verified-boot?hl=zh-cn)。设备可能具有多个引导加载程序，因此可能有多个加密密钥。

#### 可由用户设置的信任根

设备可根据需要选择允许用户配置信任根（例如公钥）。设备可以使用可由用户设置的信任根（而非内置信任根）来进行启动时验证。这样，用户既可以安装并使用自定义的 Android 版本，又不会牺牲启动时验证这一安全改进功能。

如果实现了可由用户设置的信任根，则应满足以下要求：

-   需要进行物理确认才能设置/清除可由用户设置的信任根。
-   可由用户设置的信任根只能由最终用户来设置，而不能在出厂时或从出厂到最终用户获得设备之间的任意时点进行设置。
-   可由用户设置的信任根存储在防篡改的存储空间中。“防篡改”是指可以检测到 Android 数据是否遭到了篡改（例如，数据是否被覆盖或更改）。
-   如果设置了可由用户设置的信任根，则设备应该允许启动使用内置信任根或可由用户设置的信任根签名的 Android 版本。
-   设备每次使用可由用户设置的信任根启动时，系统都应通知用户，指出设备正在加载自定义的 Android 版本，例如警告屏幕；请参阅[状态为 `LOCKED` 并已设置自定义密钥的设备](https://source.android.google.cn/docs/security/features/verifiedboot/boot-flow?hl=zh-cn#locked-devices-with-custom-key-set)。

要实现可由用户设置的信任根，其中一种方法是将虚拟分区设置为仅当设备处于 `UNLOCKED` 状态时才能刷写或清除。Google Pixel 2 设备使用的便是此方法以及名为 `avb_custom_key` 的虚拟分区。`avbtool extract_public_key` 命令会输出此分区中数据的格式。以下示例展示了如何设置可由用户设置的信任根：

```bash
avbtool extract_public_key --key key.pem --output pkmd.bin
```

可通过发出以下命令来清除可由用户设置的信任根：

```bash
fastboot erase avb_custom_key
```

## 启动流程

建议的设备启动流程如下所示：

![[mk.att/Pasted image 20230706105049.png]]

---
其他说明

![[mk.att/Pasted image 20230706165110.png]]

自行开发的应用程序是如何启动的？
![[mk.att/Pasted image 20230706165204.png]]

### 适用于 A/B 设备的流程

如果设备使用的是 A/B 系统，则启动流程略有不同。必须先使用[启动控件 HAL](https://android.googlesource.com/platform/hardware/interfaces/+/master/boot/1.0/IBootControl.hal) 将要启动的槽位标记为 `SUCCESSFUL`，**然后**再更新回滚保护 (Rollback Protection) 元数据。

如果平台更新失败（未标记为 `SUCCESSFUL`），A/B 堆栈便会回退至仍具有先前 Android 版本的另一槽位。不过，如果已设置回滚保护元数据，之前的版本会因回滚保护而无法启动。

### 将启动时验证状态传达给用户

确定设备的启动状态后，您需要将该状态传达给用户。如果设备没有任何问题，则会继续运行，而不显示任何内容。启动时验证问题分为以下几类：

-   黄色：针对设有自定义信任根的已锁定设备的警告屏幕
-   橙色：针对未锁定设备的警告屏幕
-   红色 (eio)：针对 dm-verity 损坏的警告屏幕
-   红色 (no os found)：未找到有效的操作系统

### 将启动时验证状态传达给 Android

引导加载程序通过内核命令参数或 bootconfig（从 Android 12 开始）将启动时验证状态传达给 Android。它会将 `androidboot.verifiedstate` 选项设置为以下其中一个值：

-   `green`：如果设备处于 `LOCKED` 状态且未使用可由用户设置的信任根
-   `yellow`：如果设备处于`LOCKED`状态且使用了可由用户设置的信任根
-   `orange`：如果设备处于`UNLOCKED`状态

`androidboot.veritymode` 选项设置为 `eio` 或 `restart`，具体取决于启动加载程序在处理 dm-verity 错误时所处的状态。如需了解详情，请参阅[处理验证错误](https://source.android.google.cn/docs/security/features/verifiedboot/verified-boot?hl=zh-cn#handling-verification-errors)。

## 实现 dm-verity

Android 4.4 及更高版本支持通过可选的 device-mapper-verity (dm-verity) 内核功能进行启动时验证，以便对块存储设备进行透明的完整性检查。dm-verity 有助于阻止可以持续保有 root 权限并入侵设备的持续性 Rootkit。验证启动功能有助于 Android 用户在启动设备时确定设备状态与上次使用时是否相同。

具有 root 权限的潜在有害应用 (PHA) 可以躲开检测程序的检测，并以其他方式掩蔽自己。可以获取 root 权限的软件就能够做到这一点，因为它通常比检测程序的权限更高，从而能够“欺骗”检测程序。

通过 dm-verity 功能，您可以查看块存储设备（文件系统的底部存储层），并确定它是否与预期配置一致。该功能是利用加密哈希树做到这一点的。对于每个块（通常为 4k），都有一个 SHA256 哈希。

由于哈希值存储在页面树中，因此顶级“根”哈希必须可信，才能验证树的其余部分。能够修改任何块相当于能够破坏加密哈希。下图描绘了此结构。

启动分区中包含一个公钥，该公钥必须已由设备制造商在外部进行验证。该密钥用于验证相应哈希的签名，并用于确认设备的系统分区是否受到保护且未被更改。

### 操作

dm-verity 保护机制位于内核中。因此，如果获取 Root 权限的软件在内核启动之前入侵系统，它将会一直拥有该权限。为了降低这种风险，大多数制造商都会使用烧录到设备的密钥来验证内核。该密钥在设备出厂后即无法更改。

制造商会使用该密钥验证第一级引导加载程序中的签名，而该引导加载程序会依次验证后续级别引导加载程序、应用引导加载程序和内核中的签名。希望利用[[#安卓启动时验证（avb）]]功能的每个制造商都应该有验证内核完整性的方法。内核经过验证后，可以在块存储设备装载时由内核对其进行检查和验证。

验证块存储设备的一种方法是直接对其内容进行哈希处理，然后将其与存储的值进行比较。不过，尝试验证整个块存储设备可能会需要较长的时间，并且会消耗设备的大量电量。设备将需要很长时间来启动，从而在可供使用之前便消耗了大量电量。

而 dm-verity 只有在各个块被访问时才会对其进行单独验证。将块读入内存时，会以并行方式对其进行哈希处理。然后，会从第一级开始逐级验证整个哈希树的哈希。由于读取块是一项耗时又耗电的操作，因此这种块级验证带来的延时相对而言就有些微不足道了。

> [!note]
>**注意**：作为针对 Android Go 和类似的低 RAM 设备进行的优化，dm-verity 可以配置为仅在首次（而不是每次）从数据设备读取页面时验证这些页面。首次验证之后，它会设置一个位以指示验证成功。由于此项优化提供的完整性保证级别略低，因此不应将其用于 RAM 更高的设备。如需了解详情，请参阅[这些内核补丁](https://android.googlesource.com/kernel/common/+/a73c9bca682673630cd95a7fa55190f53bab73cf)。

如果验证失败，设备会生成 I/O 错误，指明无法读取相应块。设备看起来与文件系统损坏时一样，也与预期相同。

应用可以选择在没有结果数据的情况下继续运行，例如，当这些结果并不是应用执行主要功能所必需的数据时。不过，如果应用在没有这些数据的情况下无法继续运行，则会失败。

### 前向纠错

Android 7.0 及更高版本通过前向纠错 (FEC) 功能提高了 dm-verity 的稳健性。AOSP 实现首先使用常用的 [Reed-Solomon](https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction) 纠错码，并应用一种称为交错的技术来减少空间开销并增加可以恢复的损坏块的数量。有关 FEC 的更多详情，请参阅[严格强制执行的启动时验证与纠错](https://android-developers.googleblog.com/2016/07/strictly-enforced-verified-boot-with.html)。

### 实现

#### 摘要

1.  生成 EXT4 系统映像。
2.  为该映像[[#生成哈希树]]。
3.  为该哈希树[[#构建 dm-verity 映射表]]。
4.  [[#为 dm-verity 表签名]]以生成表签名。
5.  将表签名和 dm-verity 表[[#将表签名绑定到元数据]]到 Verity 元数据。
6.  将系统映像、Verity 元数据和哈希树连接起来。

如需关于哈希树和 dm-verity 表的详细说明，请参阅 [Chromium 项目 - 验证启动](http://www.chromium.org/chromium-os/chromiumos-design-docs/verified-boot)。

#### 生成哈希树

如简介中所述，哈希树是 dm-verity 不可或缺的一部分。[cryptsetup](https://gitlab.com/cryptsetup/cryptsetup/wikis/DMVerity) 工具将为您生成哈希树。或者，也可以使用下面定义的兼容方式：

```
<your block device name> <your block device name> <block size> <block size> <image size in blocks> <image size in blocks + 8> <root hash> <salt>
```

为了形成哈希，该工具会将系统映像在第 0 层拆分成 4k 大小的块，并为每个块分配一个 SHA256 哈希。然后，通过仅将这些 SHA256 哈希组合成 4k 大小的块来形成第 1 层，从而产生一个小得多的映像。接下来再使用第 1 层的 SHA256 哈希以相同的方式形成第 2 层。

直到前一层的 SHA256 哈希可以放到一个块中，该过程就完成了。获得该块的 SHA256 哈希后，就相当于获得了树的根哈希。

哈希树的大小（以及相应的磁盘空间使用量）会因已验证分区的大小而异。在实际中，哈希树一般都比较小，通常不到 30 MB。

如果某个层中的某个块无法由前一层的哈希正好填满，您应在其中填充 0 来获得所需的 4k 大小。这样一来，您就知道哈希树没有被移除，而是填入了空白数据。

为了生成哈希树，需要将第 2 层哈希组合到第 1 层哈希的上方，将第 3 层哈希组合到第 2 层哈希的上方，依次类推。然后将所有这些数据写入到磁盘中。请注意，这种方式不会引用根哈希的第 0 层。

总而言之，构建哈希树的一般算法如下：

1.  选择一个随机盐（十六进制编码）。
2.  将系统映像拆分成 4k 大小的块。
3.  获取每个块的加盐 SHA256 哈希。
4.  组合这些哈希以形成层。
5.  在层中填充 0，直至达到 4k 块的边界。
6.  将层组合到哈希树中。
7.  重复第 2-6 步（使用前一层作为下一层的来源），直到最后只有一个哈希。

该过程的结果是一个哈希，也就是根哈希。在构建 dm-verity 映射表时会用到该哈希和您选择的盐。

#### 构建 dm-verity 映射表

构建 dm-verity 映射表，该映射表会标明内核的块存储设备（或目标）以及哈希树的位置（是同一个值）。在生成 `fstab` 和设备启动时会用到此映射。该映射表还会标明块的大小和 hash_start，即哈希树的起始位置（具体来说，就是哈希树在映像开头处的块编号）。

如需关于 Verity 目标映射表字段的详细说明，请参阅 [cryptsetup](https://code.google.com/p/cryptsetup/wiki/DMVerity?hl=zh-cn)。

#### 为 dm-verity 表签名

为 dm-verity 表签名以生成表签名。在验证分区时，会首先验证表签名。该验证是对照位于启动映像上某个固定位置的密钥来完成的。密钥通常包含在制造商的构建系统中，以便自动添加到设备上的固定位置。

如需使用这种签名和密钥的组合来验证分区，请执行以下操作：

1.  将一个格式与 libmincrypt 兼容的 RSA-2048 密钥添加到 `/verity_key` 分区的 `/boot` 中。确定用于验证哈希树的密钥所在的位置。
2.  在相关条目的 fstab 中，将 `verify` 添加到 `fs_mgr` 标记。

#### 将表签名绑定到元数据

将表签名和 dm-verity 表绑定到 Verity 元数据。为整个元数据块添加版本号，以便它可以进行扩展，例如添加第二种签名或更改某些顺序。

一个魔数（作为一个健全性检查项目）会与每组表元数据相关联，以协助标识表。由于长度包含在 EXT4 系统映像标头中，因此这为您提供了一种在不知道数据本身内容的情况下搜索元数据的方式。

这可确保您未选择验证未经验证的分区。如果是这样，缺少此魔数将会导致验证流程中断。该数字类似于：
0xb001b001

十六进制的字节值为：

-   第一字节 = b0
-   第二字节 = 01
-   第三字节 = b0
-   第四字节 = 01

下图展示了 Verity 元数据的细分：

```
<magic number>|<version>|<signature>|<table length>|<table>|<padding>
\-------------------------------------------------------------------/
\----------------------------------------------------------/   |
                            |                                  |
                            |                                 32K
                       block content
```

下表介绍了这些元数据字段。

| 字段         | 用途                             | 大小               | 值         |
| ------------ | -------------------------------- | ------------------ | ---------- |
| magic number | 供 fs_mgr 用作一个健全性检查项目 | 4 个字节           | 0xb001b001 |
| version      | 用于为元数据块添加版本号         | 4 个字节           | 目前为 0   |
| signature    | PKCS1.5 填充形式的表签名         | 256 个字节         |            |
| table length | dm-verity 表的长度（以字节数计） | 4 个字节           |            |
| table        | 上文介绍的 dm-verity 表          | 字节数与表长度相同 |            |
| padding      | 此结构会通过填充 0 达到 32k 长度 | 0                  |            |

#### 优化 dm-verity

为了充分发挥 dm-verity 的最佳性能，您应该：

-   在内核中开启 NEON SHA-2（如果是 ARMv7）或 SHA-2 扩展程序（如果是 ARMv8）。
-   使用不同的预读设置和 prefetch_cluster 设置进行实验，找出适合您设备的最佳配置。

## 验证 system_other 分区

搭载 Android 9 及更低版本且具有 A/B 分区的 Android 设备可以使用不活跃的 `system_other` 分区（例如，当 `slot_a` 处于活跃状态时，`system_b` 闲置）存储预优化的 VDEX/ODEX 文件。使用 `system_other` 时，`ro.cp_system_other_odex` 被设置为 1，以便软件包管理器服务设置 `sys.cppreopt=requested`，使 `cppreopts.rc` 能对其执行操作。

Android 10 中引入了 [`libfs_avb`](https://android.googlesource.com/platform/system/core/+/refs/heads/master/fs_mgr/libfs_avb/)，以便支持对 `system_other` 分区进行独立的 AVB 验证。此类分区的 VBMeta 结构体附加在分区末尾，将由文件系统中的预期公钥验证。Android 构建系统支持对 `system_other.img` 签名，并将相应的签名密钥包含在 `/product/etc/security/avb/system_other.avbpubkey` 下。发布工具 `sign_target_files_apks.py` 还支持将签名密钥替换为发布版本。

如果 A/B 设备搭载的 Android 版本低于 Android 10，即便升级到 Android 10 并将 `PRODUCT_RETROFIT_DYNAMIC_PARTITIONS` 设置为 `true`，也具有一个 `system_other` 物理分区。

> [!note]
> **注意**：建议不要在这些设备上启用 AVB。无线下载软件包中不包含 `system_other.img`，这可能会在一些 A/B 更新后导致验证错误。

搭载 Android 10 的 A/B 设备必须具有一个 `system_other` 逻辑分区。以下示例显示了对 `system_other` 启用 AVB 的典型 `fstab.postinstall` 文件。

```
#<dev> <mnt_point> <type>  <mnt_flags options>  <fs_mgr_flags>  
system /postinstall ext4 ro,nosuid,nodev,noexec  
slotselect_other,logical,avb_keys=/product/etc/security/avb/system_other.avbpubkey  
```

需要对 `system_other` 分区启用 AVB 的设备应将 `fstab` 文件放到产品分区中，并将属性 `ro.postinstall.fstab.prefix` 设置为 `/product`。

```plain
# Use /product/etc/fstab.postinstall to mount system_other. PRODUCT_PRODUCT_PROPERTIES += \  
ro.postinstall.fstab.prefix=/product  
  
PRODUCT_COPY_FILES += \  
$(LOCAL_PATH)/fstab.postinstall:$(TARGET_COPY_OUT_PRODUCT)/etc/fstab.postinstall  
```

## 参考实现

Android 8.0 及更高版本包含启动时验证的一个供参考的实现，名为 Android 启动时验证 (AVB) 或启动时验证 2.0。AVB 是支持 [[Android Verified Boot 2.0.mhtml|Project Treble]] 架构的一个启动时验证版本，可以将 Android 框架与底层供应商实现分离开来。

AVB 与 Android 构建系统相集成，并通过一行代码（负责生成所有必要的 dm-verity 元数据并为其签名）进行启用。如需了解详情，请参阅[构建系统集成](https://android.googlesource.com/platform/external/avb/+/master/README.md#Build-System-Integration)。

AVB 提供 libavb，后者是一个在启动时用于验证 Android 的 C 库。您可以通过以下方式将 libavb 与引导加载程序集成在一起：针对 I/O 实现[特定于平台的功能](https://android.googlesource.com/platform/external/avb/+/master/libavb/avb_ops.h)，提供信任根，并获取/设置回滚保护元数据。

AVB 的主要功能包括：针对不同分区委托更新、提供用于对分区进行签名的通用页脚格式，以及防止攻击者回滚到存在漏洞的 Android 版本。

如需了解实现方面的详细信息，请参阅 [/platform/external/avb/README.md](https://android.googlesource.com/platform/external/avb/+/master/README.md)。
