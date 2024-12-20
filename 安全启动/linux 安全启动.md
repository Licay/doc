## 大纲

当涉及到Linux安全启动时，通常需要考虑以下几个方面：硬件和软件的安全性、启动流程的可信度、系统和数据的完整性等。基于ARM平台的Linux系统安全启动也不例外。以下是一份较为详细的学习大纲：

1. 硬件安全

- 了解ARM平台的架构和特点，理解ARM SoC 的设计原理。
- 学习ARM TrustZone技术和硬件加密模块，掌握如何使用这些技术实现硬件级别的安全。
- 了解板级支持包（BSP）的安全配置，学习如何进行BSP硬件平台的安全启动。

2. 软件安全

- 学习如何在Linux内核中开启可信启动（Secure Boot），保证内核的完整性和可信度。
- 学习如何配置bootloader，在启动过程中验证kernel image 的完整性和数字签名等安全特性。
- 掌握如何在文件系统中使用SELinux、AppArmor等安全模块，对系统进程和应用程序进行安全访问控制。

3. 启动流程与可信启动

- 学习ARM平台的启动流程，了解U-Boot、ATF、TFA等软件组件在启动流程中的作用。
- 学习如何通过开启可信启动机制，保证系统的启动和运行过程得到验证和保护。
- 掌握如何使用签名工具、证书管理工具等，在启动流程中实现数字签名和证书验证等安全机制。

4. 安全监控和完整性保护

- 学习如何使用监控工具和异常处理机制，及时发现和处理系统的异常状况。
- 掌握如何备份和还原系统数据，实现数据完整性和恢复性保护。
- 熟悉Linux安全模块和工具，如SELinux、AppArmor、AIDE、Tripwire等，实现系统进程和文件的安全访问控制和完整性保护。

## 为什么需要安全启动

安全启动又叫secureboot，它是一种通过逐级验证启动镜像，实现固件可信加载的技术。 如以下为嵌入式系统的基本启动流程：

该流程中由bootrom开始，逐级通过spl、uboot以启动linux操作系统。我们假设spl、uboot和linux镜像都被保存在flash上，则在启动时，各级启动程序都需要从flash中加载下一级启动镜像，其流程示意图如下：

![[mk.att/linux 安全启动 2023-09-12 16.12.42.excalidraw]]

如果以上流程未执行secureboot，则flash中的镜像一旦被恶意攻击者替换掉，那么最终系统上将会运行被攻击者篡改过的固件。假设linux和rootfs被替换掉以后，那么启动后整个系统都将掌握在攻击者的手里。从而导致在操作系统之上构建的所有安全机制都形同虚设。

综上所述，对于需要提供安全服务的平台，其基本要求是系统本身是可信的。因此，必须要保证从系统启动时运行的第一条指令，到操作系统加载完成的整个流程中，其所有代码都经过完整性和真实性验证。而secureboot就是被设计为完成这一目标的。

## 安全启动实现

### 安全启动的信任链

由于操作系统启动时可能需要多级启动镜像，而只要其中任意一级镜像未执行secureboot流程，则其后的所有镜像实际上都是不可信的。典型的例子如下：

![[mk.att/linux 安全启动 2023-06-05 16.52.51.excalidraw]]

以上例子中bootrom验证了spl镜像，若spl未验证uboot镜像，则一旦uboot镜像被替换以后，那么攻击者就可以控制后面所有的启动流程。如被替换的uboot可以从其它位置加载非法的linux镜像，而在该linux镜像中任意植入后门等。

因此，secureboot需要建立安全启动的信任链，在启动流程中，每一级镜像都由其前级镜像执行合法性验证。这样只要保证第一级镜像是合法的，那么第二级镜像的合法性由第一级镜像保证，第三级镜像的合法性由第二级镜像保证。从而像链条一样将整个启动流程的信任链连接起来，最终保证整个系统是可信的。

### 安全启动的信任根

由于信任链建立流程中，镜像合法性是由其前级镜像验证的，那么第一级镜像的合法性如何保证呢？这就是接下来要讨论的信任根问题。既然无法由前级镜像为其背书，那么按照惯例，软件没办法解决的问题自然就需要硬件上马了。

我们知道rom是一种只读存储器，它只能被编程一次且内容在其后不能被再次更改。因此若在SOC内部集成一片ROM，并在芯片生产时就将第一级启动镜像刷到这块ROM中，那么也就保证了它是可信的，这也是现代SOC的普遍做法。

由于SOC可能会支持不同的启动方式，如xip启动可以直接从外部的norflash开始启动。因此在rom中集成bootrom镜像之后，还需要保证芯片每次启动时都必须从bootrom开始执行，否则攻击者还可以通过xip方式绕过整个secureboot流程。而一般xip启动模式都是在调试阶段用于问题定位的，因此在产品调试完成，启动secureboot前必须要关闭该模式。通常它可以通过OTP或EFUSE中的特定bit实现。

### 镜像验证

secureboot的信任链是通过逐级镜像验证实现的，而合法的镜像应该要包含以下特征：

（1）镜像是由官方发布的，即其需要验证镜像发布者的身份

（2）镜像没有被其它任何人篡改过，即镜像的内容是完整的

　　以上特性的验证可以通过特定的密码算法实现。如可通过消息摘要算法验证数据的完整性，而通过非对称签名验签算法验证镜像发布者的身份。下面简单介绍一下这两种算法，关于其更具体的原理，可参考[[crypto]]。

#### 消息摘要算法介绍

消息摘要算法是通过单向散列函数，将一段消息的内容转换为另一段固定长度消息的过程，而被其计算后生成的消息称为hash值，它具有以下特征：

（1）任意长度的输入消息都会输出固定长度的hash值
（2）hash函数必须具备单向性，而无法通过hash值反向算出其消息原文
（3）不同的输入消息其输出消息也不同，即其具有强抗碰撞性
（4）hash值的计算速度要快

以上特性使其可以被很好地用于验证消息的完整性，接下来我们对其做个简单的推演：

（1）若某一段消息被篡改以后，由于hash函数具有强抗碰撞性，因此其算出的hash值就会与被篡改前内容的hash值不同。从而能很容易地知道消息是否被篡改
（2）由于hash值的单向性，即不能通过hash值推算出原文。因此即使理论上存在具有hash值相同的不同原文，攻击者也无法通过该hash值推算出原文。这就使得攻击者无法通过hash值，伪造hash值相同而原文不同的消息。从而在密码学维度可以认为hash值与原文是一一对应的
（3）由于其输出的hash值长度固定，且占用的空间很小，如sha256占256 bit，而sha512占512 bit。因此为其它消息附加hash值在存储空间上是完全可接受的

有了消息摘要算法后，我们就可以通过它验证镜像的完整性，从而很容易地就能把被篡改的镜像给识别出来。

在密码学中有多种消息摘要算法，如md5、sha1、sha256、sha512、sha3等。随着计算机技术的发展，有些原先认为具有强碰撞性的算法（如md5、sha1），在当前已经被认为并不安全，因此secureboot中一般使用sha256、sha512等算法作为完整性算法。

#### 非对称算法介绍

非对称密码算法是相对于对称性密码算法而言的，在对称密码算法中加密和解密时使用的密钥相同。而非对称算法加密和解密时使用的密钥不同，其中私钥由密钥属主保管，且不能泄露，而公钥可以分发给其它人。且通过公钥加密的数据只能由私钥解密，通过私钥签名的数据只能由公钥验签。

在secureboot流程中，会通过私钥对镜像的hash值签名，并将其附加到镜像文件中。这样，在系统启动时只需要通过其公钥验签镜像，即可确定镜像的来源。其原理如下：

（1）由于私钥只有密钥的属主能获得，其它人无法获取到私钥，从而无法伪造签名。
（2）公钥验签时，由于只能验签其对应私钥签名的数据，而其它伪造的数据都会验签失败。即只有该公钥对应私钥属主签名的镜像才能通过验签，从而确保了镜像发布者的身份。

#### 镜像签名和验签流程

由以上分析可知镜像签名的基本流程如下：

![[mk.att/Pasted image 20230605112224.png]]

（1）使用hash算法生成镜像的hash值hash(image)。
（2）通过镜像发布者的私钥，使用非对称算法对镜像的hash值执行签名流程，并生成其签名值sig(hash)。

最后将镜像的hash值、签名值与镜像一起发布，在芯片启动时可通过以下流程验证镜像的合法性：

![[mk.att/Pasted image 20230605112255.png]]

（1）重新计算镜像的hash值hash(image)'，将hash(image)'与原始的hash(image)做比较，校验镜像完整性。
（2）使用非对称算法的公钥和签名值，对镜像的hash值进行验签。若其验签成功则表明镜像的安全性验证通过。

只有完整性和安全性都通过校验才能进入下一步，否则启动失败。

#### 公钥保护

由于验签操作依赖于公钥，若设备上的公钥被攻击者替换成他们自己的，那么攻击者只需要用与其匹配的私钥伪造镜像签名即可，因此必须要保证设备上的公钥不能被替换。一般SOC芯片的片内会含有若干OTP或EFUSE空间，它们只能被编程一次，且在被编程后不能被再次修改。

故若将公钥保存到OTP或EFUSE中，则可很好地保证其不能被修改。由于OTP的空间一般很小，而像RSA之类的公钥长度却比较长，如RSA 2048的公钥长度为2048 bits。因此为了节约OTP资源，一般在OTP中只保存公钥的hash值（如sha256的hash值为256 bits），而将公钥本身附加到镜像中。

在使用公钥之前，只需要使用OTP中的公钥hash值验证镜像附带公钥的完整性，即可确定公钥是否合法。

## bootrom 安全实现

## uboot 安全实现

### 背景 uImage 和 FIT uImage

内核经过编译后，会生成一个elf的可执行程序，叫vmlinux，这个就是原始的未经任何处理加工的**原版内核elf文件**。不过，最终烧写在嵌入式设备上的并不是这个文件。而是经过objcopy工具加工后的专门用于**烧录的镜像格式**Image。

原则上Image就可以直接被烧录到Flash上进行启动执行，但linux的内核开发者觉得Image还是太大了，因此对Image进行了压缩，并且在Image压缩后的文件的前端附加了一部分解压缩代码，构成了一个**压缩**格式的镜像文件就叫**zImage**。

解压的时候，通过zImage镜像头部的解压缩代码进行自解压，然后执行解压出来的内核镜像。

Uboot要正确启动Linux内核，就需要知道内核的一些信息，比如镜像的类型（kernel image，dtb，ramdisk image），镜像在内存的位置，镜像的链接地址，镜像文件是否有压缩等等。
![[mk.att/linux 安全启动 2023-09-13 17.31.52.excalidraw]]
Uboot为了拿到这些信息，发明了一种内核格式叫uImage，也叫Legacy uImage。uImage是由zImage加工得到的，uboot中有一个工具mkimage，该工具会给zImage加一个64字节的header，将启动内核所需的信息存储在header中。uboot启动后，从header中读取所需的信息，按照指示，进行相应的动作即可。

> header格式可以参考：include/image.h。mkimage源码在tools/mkimage。

### fastleaver（及spl） 安全实现

### FIT Image 验签

u-boot配置项

```config
CONFIG_FIT=y
CONFIG_CMD_IMI=y   # 可选，支持iminfo命令
CONFIG_IMAGE_FORMAT_LEGACY=y
```

#### FIT image的来源

有了Legacy uImage后，为什么又搞出来一个FIT uImage呢？

在Linus Torvalds 看来，内核中arch/arm/mach-xxx充斥着大量的垃圾代码。因为内核并不关心板级细节，比如板上的platform设备、resource、i2c_board_info、spi_board_info等等。大家有兴趣可以看下s3c2410的板级目录，代码量在数万行。

因此，ARM社区引入了Device Tree，使用Device Tree后，许多硬件的细节可以直接透过它传递给Linux，而不再需要在kernel中进行大量的冗余编码。

为了更好的支持单个固件的通用性，Uboot也需要对这种uImage固件进行支持。FIT uImage中加入多个dtb文件 和ramdisk文件，当然如果需要的话，同样可以支持多个kernel文件。

内核中的FDT全程为flattened device tree，FIT全称叫flattened image tree。FIT利用了Device Tree Source files（DTS）的语法，生成的Image文件也和dtb文件类似（称作itb）。

这样的目的就是能够使同一个uImage能够在Uboot中选择特定的kernel/dtb和ramdisk进行启动了，达成一个uImage可以通用多个板型的目的。

---
**功能**

FIT支持对映像进行哈希处理，以便在加载时检查这些哈希值。这可以保护映像免受破坏。然而，它不能防止将一个映像替换为另一个映像。

签名功能允许使用私钥对哈希值进行签名，以便稍后可以使用公钥进行验证。只要保密私钥并将公钥存储在非易失性位置，任何映像都可以通过这种方式进行验证。

有关验证启动的一般信息，请参见verified-boot.txt。

---
**流程**

签名的过程如下：

   - 对FIT中的映像进行哈希处理
   - 使用私钥对哈希值进行签名，生成签名
   - 将生成的签名存储在FIT中

验证的过程如下：

   - 读取FIT
   - 获取公钥
   - 从FIT中提取签名
   - 对FIT中的映像进行哈希处理
   - 使用公钥验证提取的签名与哈希值是否匹配

通常由mkimage执行签名操作，作为为设备创建固件映像的一部分。验证通常在设备上的U-Boot中进行。

---
**算法**

原则上，可以使用任何适合的算法对哈希值进行签名和验证。
目前只支持一类算法：SHA1哈希与RSA。它通过对映像进行哈希处理生成一个20字节的哈希值。

虽然在主机端（如mkimage）引入大型加密库（如openssl）是可以接受的，但在U-Boot中并不理想。
对于运行时的验证，尽量保持代码和数据的大小尽可能小是非常重要的。

因此，RSA映像验证使用经过预处理的公钥，可以使用非常少量的代码 - 仅需要从FDT提取一些数据和执行模n的指数运算。例如，在Tegra Seaboard上，代码大小影响略低于5KB。

如果需要，添加新的算法相对比较简单。如果需要另一种RSA变体，则可以将其添加到image-sig.c中的表中。如果需要其他算法（如DSA），则可以将其放置在rsa.c旁边，并将其函数也添加到image-sig.c中的表中。

---
**创建RSA密钥对和证书**

要创建一个新的公私钥对，大小为2048位：

```bash
$ openssl genpkey -algorithm RSA -out keys/dev.key \
    -pkeyopt rsa_keygen_bits:2048 -pkeyopt rsa_keygen_pubexp:65537
```

为此创建包含公钥的证书：

```bash
$ openssl req -batch -new -x509 -key keys/dev.key -out keys/dev.crt
```

如果需要，您也可以查看公钥：

```bash
$ openssl rsa -in keys/dev.key -pubout
```

若rnd错误，使用以下命令修复：

```bash
openssl rand -writerand .rnd
```

---

#### 公钥存储

为了验证使用公钥签名的图像，我们需要拥有一份受信任的公钥。这不能存储在签名图像中，因为容易被篡改。对于这个实现，我们选择将公钥存储在U-Boot的控制FDT中（使用CONFIG_OF_CONTROL）。

- algo: 算法名称（例如"sha1,rsa2048"）

可选属性包括：

- key-name-hint: 用于签名的密钥的名称。这只是一个提示，因为名称可以更改。可以通过检查所有可用的签名密钥进行验证，直到找到匹配的密钥。

- required: 如果存在，表示必须验证图像/配置才能被视为有效。通常只有所需的密钥由FIT图像引导算法进行验证。有效的取值是"image"，以强制验证所有图像，和"conf"，以强制验证所选配置（然后依赖图像中的哈希来验证这些图像）。

每个签名算法都有自己的其他属性。

- `base`: It is the same with ``N''

对于RSA，以下属性是必需的：

- rsa,num-bits: 密钥位数（例如2048）
- rsa,modulus: 模数（N），以大端字节顺序表示的多字节整数
- rsa,exponent: 公共指数（E），作为64位无符号整数
- rsa,r-squared: `(2^num-bits)^2`，以大端字节顺序表示的多字节整数
- rsa,n0-inverse: `-1 / modulus[0] mod 2^32`

下面看一个例子，以下是一个uboot.dtb存放RSA的例子。RSA key被mkimage打包在u-boot.dtb和u-boot-spl.dtb中，然后它们再被打包进u-boot.bin和u-boot-spl.bin。

```bash
$ dtc -I dts -O dtb uboot.dts > uboot.dtb
$ mkimage -f uboot.its uboot.itb -k keys -K uboot.dtb
	... ...
$ fdtdump uboot.dtb                                   

**** fdtdump is a low-level debugging tool, not meant for general use.
**** If you want to decompile a dtb, you probably want
****     dtc -I dtb -O dts <filename>

/dts-v1/;
// magic:               0xd00dfeed
// totalsize:           0x2848 (10312)
// off_dt_struct:       0x38
// off_dt_strings:      0x2e4
// off_mem_rsvmap:      0x28
// version:             17
// last_comp_version:   16
// boot_cpuid_phys:     0x0
// size_dt_strings:     0x56
// size_dt_struct:      0x2ac

/ {
    signature {
        key-dev {
            algo = "sha1,rsa2048";
            rsa,r-squared = <0x06438a85 0xe36227ac 0x99610688 0x96136fce 0x5e90829c 0xa2dc3f58 0x5ea60dec 0xac5f5b99 0x0f54037a 0xc2399d2b 0x07e120c6 0x1f2c877d 0x56990d64 0x456e65b9 0x9cc7d9fa 0xa18f4b85 0xe24e46f1 0x074d0b07 0xd9c3bc17 0x1b7ecee7 0x87bceb18 0x5e8451eb 0x64ff0f29 0xe83dc057 0x2a4c6e3c 0x91af1038 0x3130e017 0x7ca922c3 0xf9269187 0xacf300a3 0xe24b8169 0xb3e59143 0x35adb24f 0x12e17c8a 0x4ec75921 0x526adcb7 0x778941d0 0xe36178f3 0xdb469594 0x734f03a3 0x43bfd91b 0x2e391706 0x6681c6ae 0xe5f5db99 0x779f30b9 0xf08025cc 0x9cec61d6 0xdda1ea6b 0xc7e5cb8a 0x82081731 0xff47259c 0x11aa69bc 0x16f645d8 0xafadd398 0xbe9f023f 0x2a114bd5 0xc3d28b99 0x3f8878f2 0x8d2b9d2f 0x68bcd136 0x30ef7620 0xf8f3b516 0xa8c81b0f 0x8462157e>;
            rsa,modulus = <0xe091411e 0x7da3032b 0x64dcc57c 0x10e5d214 0x08ed57b6 0xfff784cd 0x9a72d9a0 0xb27ea4ef 0x57cd7c3d 0x81cb9732 0xd927662e 0x2a49b658 0x8ad3a0a1 0xba662fe7 0xc1b67bf8 0x8700a1ae 0x44025a47 0x8c898301 0xec75c64c 0xd71e6bb2 0x828d40a1 0xb29a3d9f 0x32e88d65 0x19f294e9 0x38bc1469 0x624bc628 0xcaacbf1a 0xd175ff16 0x3315a51c 0x09b93fbc 0xbf23e386 0xe8b6a041 0xe28598a5 0xfaedc65d 0x39c81a2c 0x94ac28f7 0xde36e914 0xda33f18a 0xa956fb41 0x003bad85 0x00ad5bfb 0x7263aca7 0x6468e4b5 0xd2a89e7d 0xac1bffb3 0x40a01366 0x144451fc 0xdffd5783 0xc156aecd 0xad895633 0xffb24fa0 0x5b9debdb 0x592c6d77 0x7727cb6a 0xd1929423 0xc5bdfaff 0x709fdaa1 0x3bbe9ced 0x9b8dc276 0x54ceebad 0x68726972 0xc0f3e350 0xb9809709 0x14fb9451>;
            rsa,exponent = <0x00000000 0x00010001>;
            rsa,n0-inverse = <0x9071cb4f>;
            rsa,num-bits = <0x00000800>;
            key-name-hint = "dev";
        };
    };
};
```

uboot.dts是用于uboot使用的设备树配置，如未使用该配置可以添加以下空白节点。
使用mkimage生成itb文件时，会自动将公钥插入uboot.dtb中。

```dts
/dts-v1/;

/ {
    signature {
    };
};
```

---
**已签名配置**

尽管签名图像很有用，但它并不能完全防止多种类型的攻击。例如，可以创建一个具有相同已签名图像的FIT，但配置发生更改以选择其他配置（混合和匹配攻击）。还可以将来自旧FIT版本的已签名图像替换到新FIT中（回滚攻击）。

由于两个内核都已签名，攻击者很容易添加一个新的配置3，其中包含kernel1和fdt2：

使用已签名图像，无法防止这种攻击。它是否使攻击者获得优势仁者见仁智者见智，但它并不安全。
为了解决这个问题，我们支持有签名的配置。在这种情况下，**签名的是配置文件，而不是镜像**。每个镜像都有自己的哈希值，并且我们将哈希值包括在配置签名中。

---
**启用FIT验证**

除了启用FIT本身的选项外，还必须启用以下CONFIG：

CONFIG_FIT - 启用FIT支持
CONFIG_FIT_SIGNATURE - 启用FIT中的签名和验证
CONFIG_RSA - 启用用于验签的RSA算法

警告：在依赖带有必需签名检查的已签名FIT图像时，默认情况下通过不定义CONFIG_IMAGE_FORMAT_LEGACY来禁用传统图像格式

---
**bootm启动不同的配置**

```bash
1. bootm <addr1>
2. bootm [<addr1>]:<subimg1>
3. bootm [<addr1>]#<conf>[#<extra-conf[#...]]
4. bootm [<addr1>]:<subimg1> [<addr2>]:<subimg2>
5. bootm [<addr1>]:<subimg1> [<addr2>]:<subimg2> [<addr3>]:<subimg3>
6. bootm [<addr1>]:<subimg1> [<addr2>]:<subimg2> <addr3>
7. bootm [<addr1>]:<subimg1> -     [<addr3>]:<subimg3>
8. bootm [<addr1>]:<subimg1> -     <addr3>
```

对于有多种镜像，多套配置的itb，都是以configurations 中 default 指定的配置启动。

```bash
bootm 200000
```

也可以手动指定使用那套配置

```bash
bootm 200000#cfg@1
```

也可以手动搭配不同的镜像节点启动

```bash
bootm 200000:kernel@1 800000:ramdisk@2  
bootm 200000:kernel@1 800000:ramdisk@1 800000:fdt@1  
bootm 200000:kernel@2 200000:ramdisk@2 600000  
bootm 200000:kernel@2 - 200000:fdt@1
```

如果bootm的时候不指定地址，则会使用`CONFIG_SYS_LOAD_ADDR`配置的地址。

---
**小结**

FIT Image可以兼容于多种板子，而无需重新进行编译烧写。对于有多个kernel节点或者fdt节点等等，兼容性更强。同时，可以有多种configurations，来对kernel、fdt、ramdisk来进行组合。

#### 制作FIT Image

制作FIT Image需要用到两个工具，mkimage和的dtc。dtc要导入到环境变量$PATH中，mkimage会调用dtc。

mkimage的输入为 image source file,它定义了启动过程中image的各种属性，扩展名为.its。its只是描述了Image的属性，实际的Image data 是在uImage中，具体路径由its指定。

![[mk.att/linux 安全启动 2023-09-14 11.18.19.excalidraw]]

简要流程说明

1. 准备打包环境，包括dtc、mkimage、openssl等；
2. 根据需求配置fit文件；
3. （可选）生成公钥、私钥；
4. 打包FIT镜像；
5. 传入目标设备；
6. 使用bootm命令启动。

如下是kernel 的its文件,后面会介绍各项内容的含义。

```dts
/*  
 * Simple U-Boot uImage source file containing a single kernel  
 */  
  
/dts-v1/;  
  
/ {  
 description = "Simple image with single Linux kernel";  
 #address-cells = <1>;  
  
 images {  
  kernel@1 {  
   description = "Vanilla Linux kernel";  
   data = /incbin/("./vmlinux.bin.gz"); # Image data 具体路径  
   type = "kernel";  
   arch = "ppc";  
   os = "linux";  
   compression = "gzip";  
   load = <00000000>;  
   entry = <00000000>;  
   hash@1 {  
    algo = "crc32";  
   };  
   hash@2 {  
    algo = "sha1";  
   };  
  };  
 };  
  
 configurations {  
  default = "config@1";  
  config@1 {  
   description = "Boot Linux kernel";  
   kernel = "kernel@1";  
  };  
 };  
};
```

mkimage的输出是一个后缀为.itb的二进制文件，包含了所有需要的数据（kernel，dtb，ramdisk）。itb文件制作好之后，就可以直接加载到嵌入式设备上，通过bootm命令启动。

总结下制作FIT Image的4个必要文件：

-   mkimage
-   dtc
-   its (image source file (`*.its`))
-   image data file(s)

生成签名和证书，需要单独

```bash
openssl genpkey -algorithm RSA -out keys/dev.key  -pkeyopt rsa_keygen_bits:2048 -pkeyopt rsa_keygen_pubexp:65537

openssl req -batch -new -x509 -key keys/dev.key -out keys/dev.crt

openssl rsa -in keys/dev.key -pubout
```

```bash
mkimage -f uboot.its -r uboot.itb
mkimage -l uboot.itb
```

- 对image签名
- 对config签名

> 参考u-boot源码
> doc/uImage.FIT/sign-images.its
> doc/uImage.FIT/sign-configs.its
> doc/uImage.FIT/signature.txt

##### 无签名

```dts
/*
* Simple U-Boot uImage source file containing a single kernel and FDT blob
*/

/dts-v1/;

/ {
description = "Simple image with single Linux kernel and FDT blob";

	images {
		kernel@1 {
			description = "Vanilla Linux kernel";
			data = /incbin/("obj/KERNEL_OBJS/arch/arm/boot/zImage");
			type = "kernel";
			arch = "arm";
			os = "linux";
			compression = "none";
			load = <0x40008000>;
			entry = <0x40008000>;
			kernel-version = <1>;
			hash@1 {
				algo = "sha1";
			};
		};
		fdt@1 {
			description = "Flattened Device Tree blob";
			data = /incbin/("obj/KERNEL_OBJS/arch/arm/boot/dts/simple/demo7/demo7.dtb");
			type = "flat_dt";
			arch = "arm";
			compression = "none";
			fdt-version = <1>;
			hash@1 {
				algo = "sha1";
			};
		};
	};

	configurations {
		default = "conf@1";
		conf@1 {
			description = "Boot Linux kernel with FDT blob";
			kernel = "kernel@1";
			fdt = "fdt@1";
		};
	};
};
```

```bash
$ mkimage -f uboot.its uboot.itb
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/kernel@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/kernel@1/hash@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/fdt@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/fdt@1/hash@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /configurations/conf@1 has a unit name, but no reg property
FIT description: Simple image with single Linux kernel and FDT blob
Created:         Fri Jun 30 14:29:56 2023
 Image 0 (kernel@1)
  Description:  Vanilla Linux kernel
  Created:      Fri Jun 30 14:29:56 2023
  Type:         Kernel Image
  Compression:  uncompressed
  Data Size:    3095936 Bytes = 3023.38 KiB = 2.95 MiB
  Architecture: ARM
  OS:           Linux
  Load Address: 0x40008000
  Entry Point:  0x40008000
  Hash algo:    sha1
  Hash value:   59b8bbbfceb8bc3042a69fc1e4a480c37538e80f
 Image 1 (fdt@1)
  Description:  Flattened Device Tree blob
  Created:      Fri Jun 30 14:29:56 2023
  Type:         Flat Device Tree
  Compression:  uncompressed
  Data Size:    63048 Bytes = 61.57 KiB = 0.06 MiB
  Architecture: ARM
  Hash algo:    sha1
  Hash value:   d90566dc5416100544319c1df2cf4df751723d78
 Default Configuration: 'conf@1'
 Configuration 0 (conf@1)
  Description:  Boot Linux kernel with FDT blob
  Kernel:       kernel@1
  FDT:          fdt@1
```

##### 签名image

```dts
/*
* Simple U-Boot uImage source file containing a single kernel and FDT blob
*/

/dts-v1/;

/ {
description = "Simple image with single Linux kernel and FDT blob";

	images {
		kernel@1 {
			description = "Vanilla Linux kernel";
			data = /incbin/("obj/KERNEL_OBJS/arch/arm/boot/zImage");
			type = "kernel";
			arch = "arm";
			os = "linux";
			compression = "none";
			load = <0x40008000>;
			entry = <0x40008000>;
			kernel-version = <1>;
			signature@1 {
				algo = "sha1,rsa2048";
				key-name-hint = "dev";
			};
		};
		fdt@1 {
			description = "Flattened Device Tree blob";
			data = /incbin/("obj/KERNEL_OBJS/arch/arm/boot/dts/simple/demo7/demo7.dtb");
			type = "flat_dt";
			arch = "arm";
			compression = "none";
			fdt-version = <1>;
			signature@1 {
				algo = "sha1,rsa2048";
				key-name-hint = "dev";
			};
		};
	};

	configurations {
		default = "conf@1";
		conf@1 {
			description = "Boot Linux kernel with FDT blob";
			kernel = "kernel@1";
			fdt = "fdt@1";
		};
	};
};
```

```bash
$ mkimage -f uboot.its uboot.itb -k keys -K uboot.dtb                                                                        255 ↵
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/kernel@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/kernel@1/signature@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/fdt@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/fdt@1/signature@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /configurations/conf@1 has a unit name, but no reg property
FIT description: Simple image with single Linux kernel and FDT blob
Created:         Fri Jun 30 14:20:01 2023
 Image 0 (kernel@1)
  Description:  Vanilla Linux kernel
  Created:      Fri Jun 30 14:20:01 2023
  Type:         Kernel Image
  Compression:  uncompressed
  Data Size:    3095936 Bytes = 3023.38 KiB = 2.95 MiB
  Architecture: ARM
  OS:           Linux
  Load Address: 0x40008000
  Entry Point:  0x40008000
  Sign algo:    sha1,rsa2048:dev
  Sign value:   027f806a9b229f78d35d82cb81cfa1e93a6556371dc7ead174b07299a34ae4cc30c40227163e2cdac1d52b025df9dbcde7452bd391ade97963888094dba1bc7632be694d74c0fe94da07afde4a6d3cf73f8a769c1b25d7a90ce12cd2ecbb9b41c872aa5c0c57f9c8413ab711f226ab4342c2a6f5c56ac97beffd3e981044983ed37542935961e2bd2e70b24fa7ed1f8d8e8208f45e629a09c9aaa0cdd1ddb0d3a353d33bf4a1f96bd7648cc23f6e87c527189955145b9990786e5b58c0e890040d5653902ee25cae989b6266e027e043b709ccb005b0f24f130138aa6811941042936c015064a83986a0ff77fcb6374015f7ffa3796b30b8c6fad4843c7dbef4
  Timestamp:    Fri Jun 30 14:20:01 2023
 Image 1 (fdt@1)
  Description:  Flattened Device Tree blob
  Created:      Fri Jun 30 14:20:01 2023
  Type:         Flat Device Tree
  Compression:  uncompressed
  Data Size:    63048 Bytes = 61.57 KiB = 0.06 MiB
  Architecture: ARM
  Sign algo:    sha1,rsa2048:dev
  Sign value:   3168f11bf6ff4e2be2a3aae749a00bd8cbc83db77dbd553027d51e39015165cfecdbbd53e0f3156296f9612dd209edd8dbc0fd2083b4c76cf79b276680ed0bb4c7dccf9495228dc8dfb764fb6ce837452db077dcf3f93c7224f1c9e1165cf760714826fdcb1b0700be2b204c332dcd353609ad7e7af5d0fbbb5dac8986e58ecd170e41d7c8fb30a7d7d045f4f9171925954366bd10b1647d475858a3a9d659a685854e72cdb6f7dc2d154ecc372a5be21c7dd88ea369d2877f4fc4377b7a632147c4241beb13aca6d5ab4ba14e4ae191f39a39f04811cd10a46e31025533f2031f8cb5b93a290f50cb961c86c7a939db0e1668adb92d71d92c5a7fe43e44d241
  Timestamp:    Fri Jun 30 14:20:01 2023
 Default Configuration: 'conf@1'
 Configuration 0 (conf@1)
  Description:  Boot Linux kernel with FDT blob
  Kernel:       kernel@1
  FDT:          fdt@1
```

##### 签名config

更安全。

在secure boot中，除了对各个独立镜像签名外，也可以对FIT Image中的配置项进行签名。

有些情况下，已经签名的镜像也有可能遭到破坏。例如，也可以使用相同的签名镜像创建一个FIT image，但是，其配置已经被改变，从而可以选择不同的镜像去加载（**混合式匹配攻击**）。也有可能拿旧版本的FIT Image去替换新的FIT image（回滚式攻击）。

```dts
/*
 * Simple U-Boot uImage source file containing a single kernel and FDT blob
 */

/dts-v1/;

/ {
 description = "Simple image with single Linux kernel and FDT blob";
 #address-cells = <1>;

 images {
  kernel@1 {
   description = "Vanilla Linux kernel";
   data = /incbin/("obj/KERNEL_OBJS/arch/arm/boot/zImage");
   type = "kernel";
   arch = "arm";
   os = "linux";
   compression = "none";
   load = <0x40008000>;
   entry = <0x40008000>;
   hash@1 {
    algo = "sha1";
   };
   /*
   signature@1 {
        algo = "sha1,rsa2048";
        key-name-hit = "dev";
   };*/
  };
  fdt@1 {
   description = "Flattened Device Tree blob";
   data = /incbin/("obj/KERNEL_OBJS/arch/arm/boot/dts/simple/demo7/demo7.dtb");
   type = "flat_dt";
   arch = "arm";
   compression = "none";
   hash@1 {
    algo = "sha1";
   };
   /*
   signature@1 {
        algo = "sha1,rsa2048";
        key-name-hit = "dev";
   };*/
  };
 };

 configurations {
  default = "conf@1";
  conf@1 {
   description = "Boot Linux kernel with FDT blob";
   kernel = "kernel@1";
   fdt = "fdt@1";
   signature@1 {
    algo = "sha1,rsa2048";
    key-name-hint = "dev";
    sign-images = "fdt", "kernel";
   };
  };
 };
};
```

```bash
$ mkimage -f uboot.its uboot.itb -k keys -K uboot.dtb
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/kernel@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/kernel@1/hash@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/fdt@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /images/fdt@1/hash@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /configurations/conf@1 has a unit name, but no reg property
uboot.itb.tmp: Warning (unit_address_vs_reg): Node /configurations/conf@1/signature@1 has a unit name, but no reg property
FIT description: Simple image with single Linux kernel and FDT blob
Created:         Fri Jun 30 14:27:14 2023
 Image 0 (kernel@1)
  Description:  Vanilla Linux kernel
  Created:      Fri Jun 30 14:27:14 2023
  Type:         Kernel Image
  Compression:  uncompressed
  Data Size:    3095936 Bytes = 3023.38 KiB = 2.95 MiB
  Architecture: ARM
  OS:           Linux
  Load Address: 0x40008000
  Entry Point:  0x40008000
  Hash algo:    sha1
  Hash value:   59b8bbbfceb8bc3042a69fc1e4a480c37538e80f
 Image 1 (fdt@1)
  Description:  Flattened Device Tree blob
  Created:      Fri Jun 30 14:27:14 2023
  Type:         Flat Device Tree
  Compression:  uncompressed
  Data Size:    63048 Bytes = 61.57 KiB = 0.06 MiB
  Architecture: ARM
  Hash algo:    sha1
  Hash value:   d90566dc5416100544319c1df2cf4df751723d78
 Default Configuration: 'conf@1'
 Configuration 0 (conf@1)
  Description:  Boot Linux kernel with FDT blob
  Kernel:       kernel@1
  FDT:          fdt@1
  Sign algo:    sha1,rsa2048:dev
  Sign value:   92b576d407fbe76acb31c0261e00287546437537ce924602967ef27a7f814e8e52f64374ca4fb67a78d422a429e4e65eeeb1e4fab3bb652eaf5f293f4f26d517d1730839f8d00782dfe01d81c5c558dda1863ed4e3f26f0640df5610323dbbf179d3c7f37a70dc62a91b68f150e38f64668a28f73b14e131a43a5e3707b901db47244c92e0d1db254631a16bdb9bae34e0b7118a97d7787c5af2010a8d9da65e485048d88b244af3325224d3ae1e5651cdc3769dae8092ea98e48139cca292479927e8d3aab844e0ff1b1fe5a4c44bf871a04721fbc122f830f9149fb6b530fa4a4d19fc3ecd271f644ec6e07d6e423cf141c220f4b1785a2769506525783f71
  Timestamp:    Fri Jun 30 14:27:14 2023
```

#### 签名镜像+签名配置

在secure boot中，除了对各个独立镜像签名外，还要对FIT Image中的配置项进行签名。

有些情况下，已经签名的镜像也有可能遭到破坏。例如，也可以使用相同的签名镜像创建一个FIT image，但是，其配置已经被改变，从而可以选择不同的镜像去加载（混合式匹配攻击）。也有可能拿旧版本的FIT Image去替换新的FIT image（回滚式攻击）。

| 攻击方式                | 解决方法     |
| ----------------------- | ------------ |
| 替换攻击                | 为镜像签名   |
| 混合匹配攻击/回滚式攻击 | 为配置项签名 |

下面举个例子。

#### 启动测试

注意，需要打包zimage，而不是uimage。

```bash
simple # dev_read kernel 0 0x40800000 0x25a000
simple # bootm
## Loading kernel from FIT Image at 40800000 ...
   Using 'conf@1' configuration
   Trying 'kernel@1' kernel subimage
     Description:  Vanilla Linux kernel
     Type:         Kernel Image
     Compression:  uncompressed
     Data Start:   0x408000dc
     Data Size:         Architecture: ARM
     OS:           Linux
     Load Address: 0x40007fc0
     Entry Point:  0x40007fc0
     Hash algo:    sha1
     Hash value:   3fe19d6e06037c0f83f719def39d0a39ad2dae3a
   Verifying Hash Integrity ... sha1+ OK
## Loading fdt from FIT Image at 40800000 ...
   Using 'conf@1' configuration
   Trying 'fdt@1' fdt subimage
     Description:  Flattened Device Tree blob
     Type:         Flat Device Tree
     Compression:  uncompressed
     Data Start:   0x40a3d518
     Data Size:         Architecture: ARM
     Hash algo:    sha1
     Hash value:   31c422a2682ae585fc3bd0cfb8e50576d35a9765
   Verifying Hash Integrity ... sha1+ OK

Starting kernel ...

[    0.000000] [start_kernel] timestamp: 6100119 us 
[    0.000000] Booting Linux on physical CPU 0x0
```

### 问题

```bash
simple # dev_read kernel 0 0x40010000 0x25a000
simple # iminfo 0x40010000                    

## Checking Image at 40010000 ...
   FIT image found
   FIT description: Simple image with single Linux kernel and FDT blob
    Image 0 (kernel@1)
     Description:  Vanilla Linux kernel
     Type:         Kernel Image
     Compression:  uncompressed
     Data Start:   0x400100dc
     Data Size:         Architecture: ARM
     OS:           Linux
     Load Address: 0x40007fc0
     Entry Point:  0x40007fc0
     Hash algo:    sha1
     Hash value:   8181c41b47d348d2b3227df338fdd7fdd764c63b
    Image 1 (fdt@1)
     Description:  Flattened Device Tree blob
     Type:         Flat Device Tree
     Compression:  uncompressed
     Data Start:   0x4024d558
     Data Size:         Architecture: ARM
     Hash algo:    sha1
     Hash value:   31c422a2682ae585fc3bd0cfb8e50576d35a9765
    Default Configuration: 'conf@1'
    Configuration 0 (conf@1)
     Description:  Boot Linux kernel with FDT blob
     Kernel:       kernel@1
     FDT:          fdt@1
## Checking hash(es) for FIT Image at 40010000 ...
   Hash(es) for Image 0 (kernel@1): sha1+ 
   Hash(es) for Image 1 (fdt@1): sha1+ 
simple # 

simple # dev_read kernel 0 0x40800000 0x25a000
simple # bootm 0x40010000
## Loading kernel from FIT Image at 40010000 ...
   Using 'conf@1' configuration
   Trying 'kernel@1' kernel subimage
     Description:  Vanilla Linux kernel
     Type:         Kernel Image
     Compression:  uncompressed
     Data Start:   0x400100dc
     Data Size:         Architecture: ARM
     OS:           Linux
     Load Address: 0x40007fc0
     Entry Point:  0x40007fc0
     Hash algo:    sha1
     Hash value:   8181c41b47d348d2b3227df338fdd7fdd764c63b
   Verifying Hash Integrity ... sha1+ OK
## Loading fdt from FIT Image at 40010000 ...
   Using 'conf@1' configuration
   Trying 'fdt@1' fdt subimage
     Description:  Flattened Device Tree blob
     Type:         Flat Device Tree
     Compression:  uncompressed
     Data Start:   0x4024d558
     Data Size:         Architecture: ARM
     Hash algo:    sha1
     Hash value:   31c422a2682ae585fc3bd0cfb8e50576d35a9765
   Verifying Hash Integrity ... sha1+ OK
ERROR: new format image overwritten - must RESET the board to recover
```

地址太低被复写。

```bash
simple # dev_read kernel 0 0x40800000 0x25a000
simple # bootm
## Loading kernel from FIT Image at 40800000 ...
   Using 'conf@1' configuration
   Trying 'kernel@1' kernel subimage
     Description:  Vanilla Linux kernel
     Type:         Kernel Image
     Compression:  uncompressed
     Data Start:   0x408000dc
     Data Size:         Architecture: ARM
     OS:           Linux
     Load Address: 0x40007fc0
     Entry Point:  0x40007fc0
     Hash algo:    sha1
     Hash value:   8181c41b47d348d2b3227df338fdd7fdd764c63b
   Verifying Hash Integrity ... sha1+ OK
## Loading fdt from FIT Image at 40800000 ...
   Using 'conf@1' configuration
   Trying 'fdt@1' fdt subimage
     Description:  Flattened Device Tree blob
     Type:         Flat Device Tree
     Compression:  uncompressed
     Data Start:   0x40a3d558
     Data Size:         Architecture: ARM
     Hash algo:    sha1
     Hash value:   31c422a2682ae585fc3bd0cfb8e50576d35a9765
   Verifying Hash Integrity ... sha1+ OK

Starting kernel ...

undefined instruction
pc : [<40007fc8>]	   lr : [<47ec89d8>]
reloc pc : [<3b13ffc8>]	   lr : [<430009d8>]
sp : 474f0828  ip : 47f88655	 fp : 00000001
r10: 474f0930  r9 : 474ffef0	 r8 : 47ec8bbc
r7 : 00000000  r6 : 40007fc0	 r5 : 47f9315c  r4 : 00000000
r3 : 0000dc41  r2 : 42fef000	 r1 : 000011e0  r0 : 474fffb0
Flags: nZCv  IRQs off  FIQs off  Mode SVC_32
Resetting CPU ...

resetting ...

```

需要打包zimage，而不是uimage

### 自编rsa验签

原则上来说，任何合适的算法都可以用来签名和验签。在uboot中，目前只支持一类算法：SHA+RSA。

当然也可以在uboot中添加合适的算法，如果有其他签名算法（如DSA），可以直接替换rsa.c，并在image-sig.c中添加对应算法即可。

FIT签名流程并未添加padding。

可以自定义使用rsa-pss验签kernel及rootfs。

#### RSA-PSS签名

PSS (Probabilistic Signature Scheme)私钥签名流程的一种填充模式。目前主流的RSA签名包括RSA-PSS和RSA-PKCS#1 v1.5。相对应PKCS（Public Key Cryptography Standards）是一种能够自我从签名，而PSS无法从签名中恢恢复原来的签名。openssl-1.1.x以后默认使用更安全的PSS的RSA签名模式。

##### 填充的必要性

RSA算法比较慢，一般用于非对称加密的private key签名和public key验证。因RSA算法沒有加入乱数，当出现重复性的原始资料，攻击者会通过相同加密密文而猜测出原文，因此导入padding的机制來加強安全性。

TLS流程中的密钥材料若不进行填充而直接加密，那么显然相同的key，会得到相同的密文。这种在语义上来说，是不安全的。以下例子说明了无填充模式的安全漏洞。

-   m：明文
-   e, n：RSA参数（公钥）
-   d, n：RSA参数（私钥）
-   c：网络传输密文

加密方加密$m：c = m^e \,mod\, n$，传输c

解密方解密$c：m = c^d \,mod\, n$，还原m

-   c'：篡改密文
-   k：篡改码

由于c在网络上传输，如果网络上有人对其进行$c' = c*k^e \,mod\, n$，这样的替换

那么解密方将得到的结果是

$$
(c*k^e)^d\,mod\,n
= (c^d\,mod\,n) * (k^{ed}\,mod\,n)
= m*k
$$

即中间人有办法控制m。

##### PSS的基本要素

**使用PSS模式的RSA签名流程如下：**

![[mk.att/Pasted image 20230612164140.png]]

相比较PKCS#1 v1.5的padding简单许多：

![[mk.att/Pasted image 20230612164158.png]]

PSS的一些概念：

-   hash算法，一般使用SHA-1；
-   [[#MGF C代码实现|MGF函数]]（mask generation function)。默认是MGF1；
-   salt length，一般由hLen决定。当为0时，签名值变成了唯一确定的；
-   截断符号，一般是`0xbc`。

##### 签名示例

这节例子中所涉及到的文件说明：

/tmp/wildcard_domain.sports.qq.com.v2.key：私钥
/tmp/pub: 公钥
/tmp/data: 明文
/tmp/endata: 密文
/tmp/sign: 签名
/tmp/de_sign: 解签名

1. 准备密钥
	-   通过key文件提取出public key

	```bash
	openssl rsa -in /usr/local/services/ssl_agent/ca/wildcard_domain.sports.qq.com.v2.key -pubout -out /tmp/pub
	```

	-   原始数据：

	```bash
	echo -n "1234567890" > /tmp/data
	```

	-   这样就有一对公钥和私钥，用来测试RSA加密解密（encrypt、decrypt）和签名验证（sign，verify）
	-   RSA加密的两种算法分别是RSAES-PKCS-v1_5 and RSAES-OAEP。

1. 加密和解密（encrypt，decrypt）
	-   加密：

	```bash
	openssl rsautl -pubin -inkey /tmp/data -in /tmp/data -encrypt -out /tmp/endata
	```

	-   解密，用private key解密，得到原本的值：

	```bash
	openssl rsautl -inkey /tmp/wildcard_domain.sports.qq.com.v2.key -in /tmp/en_data -decrypt
	```

1. 签名和验证（sign, verify）

	签名过程包括hash和加密。hash函数一般使用sha1。这样输入明文，直接生成sign签名。

	如果是私钥签名所做的事就是先hash再加密，选择一种hash算法把原始消息计算后成ASN1格式，再把这个资料用private key加密后送出，资料本身不加密，这种方式主要是用來验证资料来源是否可信任的，送出時把原始资料和签名一起送出。

	-   签名：

	```bash
	openssl sha1 -sign /tmp/wildcard_domain.sports.qq.com.v2.key  /tmp/data > /tmp/data/sign
	```

	-   解开签名：

	```bash
	openssl rsautl -pubin -inkey /tmp/pub -in /tmp/sign -verify -out /tmp/de_sign 
	```

	用public key解开签名，并且保留padding

	```bash
	 openssl rsautl -pubin -inkey /tmp/pub -in /tmp/sign -encrypt -raw -hexdump
	```

	使用解开ASN1解开签名，或者签名后用ASN1工具解析

	```bash
	openssl rsautl -pubin -inkey /tmp/pub -in /tmp/sign -verify -asn1parse
	```

	或者：

	```bash
	openssl asn1parse -inform der -in /tmp/de_sign
	```

	和本地sha1对比

	```bash
	openssl sha1 /tmp/data
	```

	如果两者hash结果是一样，那么确定签名送过来是正确的。

##### PSS填充模式的特点

PSS是RSA的填充模式中的一种。

完整的RSA的填充模式包括：

```text
RSA_SSLV23_PADDING（SSLv23填充）
RSA_NO_PADDING（不填充）
RSA_PKCS1_OAEP_PADDING （RSAES-OAEP填充，强制使用SHA1，加密使用）
RSA_X931_PADDING（X9.31填充，签名使用）
RSA_PKCS1_PSS_PADDING（RSASSA-PSS填充，签名使用）
RSA_PKCS1_PADDING（RSAES-PKCS1-v1_5/RSASSA-PKCS1-v1_5填充，签名可使用）
```

其中主流的填充模式是PKCS1和PSS模式。

PSS的优缺点如下：

-   PKCS#1 v1.5比较简易实现，但是缺少security proof。
-   PSS更安全，所以新版的openssl-1.1.x优先使用PSS进行私钥签名（具体在ssl握手的server key exchange阶段）

#### MGF C代码实现

在非对称加密算法 RSA 中，如果加密模式为 RSA-OAEP 或签名方案为概率签名 RSA-PSS 时会用到一个 MGF 函数。

-   OAEP: 最优非对称加密填充 Optimal Asymmetric Encryption Padding
-   PSS: 概率签名方案, Probabilistic Signature Scheme
-   MGF: 掩码生成函数, Mask Generation Function

##### OAEP 中对 MGF 的调用

RSA 最优非对称加密填充中两次调用 MGF 分别用于生成 maskedDB 和 maskedSeed 数据:

![[mk.att/Pasted image 20230612194745.png]]

##### PSS 中对 MGF 的调用

RSA 概率签名方案中调用 MGF 来生成 maskedDB 数据:

![[mk.att/Pasted image 20230612194807.png]]

##### RFC 8017 中关于 MGF 函数的描述

MGF 函数基于哈希函数来构建，RSA 规范中使用的 MGF 函数是 MGF1，其在 [RFC 8017](https://www.rfc-editor.org/rfc/rfc8017.txt) 附录 B2 中的描述如下:

```
B.2.  Mask Generation Functions

   A mask generation function takes an octet string of variable length
   and a desired output length as input and outputs an octet string of
   the desired length.  There may be restrictions on the length of the
   input and output octet strings, but such bounds are generally very
   large.  Mask generation functions are deterministic; the octet string
   output is completely determined by the input octet string.  The
   output of a mask generation function should be pseudorandom: Given
   one part of the output but not the input, it should be infeasible to
   predict another part of the output.  The provable security of
   RSAES-OAEP and RSASSA-PSS relies on the random nature of the output
   of the mask generation function, which in turn relies on the random
   nature of the underlying hash.

   One mask generation function is given here: MGF1, which is based on a
   hash function.  MGF1 coincides with the mask generation functions
   defined in IEEE 1363 [IEEE1363] and ANSI X9.44 [ANSIX944].  Future
   versions of this document may define other mask generation functions.

B.2.1.  MGF1

   MGF1 is a mask generation function based on a hash function.

   MGF1 (mgfSeed, maskLen)

   Options:

      Hash     hash function (hLen denotes the length in octets of
               the hash function output)

   Input:

      mgfSeed  seed from which mask is generated, an octet string
      maskLen  intended length in octets of the mask, at most 2^32 hLen

   Output:

      mask     mask, an octet string of length maskLen

   Error: "mask too long"

   Steps:

   1.  If maskLen > 2^32 hLen, output "mask too long" and stop.

   2.  Let T be the empty octet string.

   3.  For counter from 0 to \ceil (maskLen / hLen) - 1, do the
       following:

       A.  Convert counter to an octet string C of length 4 octets (see
           Section 4.1):

              C = I2OSP (counter, 4) .

       B.  Concatenate the hash of the seed mgfSeed and C to the octet
           string T:

              T = T || Hash(mgfSeed || C) .

   4.  Output the leading maskLen octets of T as the octet string mask.
```

说重点, MGF1 是一个伪随机函数，其输入参数是一个任意长度的位串 mgfSeed 和需要输出的掩码位长 maskLen，基于哈希函数构造。

1.  MGF1 有两个参数 mgfSeed 和 maskLen

    -   mgfSeed 是输入的随机变量
    -   maskLen 是输出的掩码长度
2.  MGF1 内部根据需要调用一个哈希函数，默认为 SHA1，其输出哈希长度为 hLen

3.  MGF1 函数内部有一个计数器 counter，其大小为 0 ~ maskLen / hLen

4.  将 counter 转换成 4 字节的字符串 C，附加到 mgfSeed 的末尾并计算哈希

```
Hash(mgfSeed || C)
```

5.  递增计数器，计算哈希输出，并链接前一个哈希输出的末尾，直到输出位长的长度达到要求(不少于 maskLen)，然后截取前面的 maskLen 字节并返回

所以整个格式类似:

```
Hash(mgfSeed||0x00000000) || Hash(mgfSeed||0x00000001) || Hash(mgfSeed||0x00000002)... Hash(mgfSeed||n)
```

##### MGF 函数代码

###### 3.1 实现代码

整个过程的 C 语言代码如下:

-   mgf.h

```c
#ifndef __ROCKY_MGF__H
#define __ROCKY_MGF__H

int MGF1(const char *mgfSeed, unsigned int mgfSeedLen, HASH_ALG alg, unsigned int maskLen, char *mask);

#endif
```

-   mgf.c

```c
#define MGF1_BUF_SIZE 256
int MGF1(const unsigned char *mgfSeed, unsigned int mgfSeedLen, HASH_ALG alg, unsigned int maskLen, unsigned char *mask)
{
    unsigned char buf[MGF1_BUF_SIZE], *p;
    unsigned char digest[64]; /* 最长支持 SHA-512 */
    unsigned long digestLen;
    unsigned long counter, restLen;

    if (mgfSeedLen > MGF1_BUF_SIZE - 4)
    {
        printf("MGF1 buffer is not long enough!\n");
        return -1;
    }

    // copy mgfSeed to buffer
    memcpy(buf, mgfSeed, mgfSeedLen);

    // clear rest buffer to 0
    p = buf + mgfSeedLen;
    memset(p, 0, MGF1_BUF_SIZE-mgfSeedLen);

    digestLen = HASH_GetDigestSize(alg, 0);

    counter = 0;
    restLen = maskLen;

    while (restLen > 0)
    {
        p[0] = (counter >> 24) & 0xff;
        p[1] = (counter >> 16) & 0xff;
        p[2] = (counter >>  8) & 0xff;
        p[3] = counter & 0xff;

        if (restLen >= digestLen)
        {
            HASH(alg, buf, mgfSeedLen+4, (unsigned char *)mask);

            restLen -= digestLen;
            mask += digestLen;

            counter ++;
        }
        else // 剩余的不足单次哈希长度的部分
        {
            HASH(alg, buf, mgfSeedLen+4, (unsigned char *)digest);

            memcpy(mask, digest, restLen);

            restLen = 0;
        }
    }

    return 0;
}
```

其中使用到一个计算哈希函数的库 libhash.a，其头文件如下:

-   hash.h

```c
/*
 * @        file: hash.h
 * @ description: header file for hash.c
 * @      author: Gu Yongqiang
 * @        blog: https://blog.csdn.net/guyongqiangx
 */
#ifndef __ROCKY_HASH__H
#define __ROCKY_HASH__H
#include "type.h"

/* Hash Algorithm List */
typedef enum {
    HASH_ALG_MD2,
    HASH_ALG_MD4,
    HASH_ALG_MD5,
    HASH_ALG_SHA1,
    HASH_ALG_SHA224,
    HASH_ALG_SHA256,
    HASH_ALG_SHA384,
    HASH_ALG_SHA512,
    HASH_ALG_SHA512_224,
    HASH_ALG_SHA512_256,
    HASH_ALG_SHA512_T,
    HASH_ALG_SHA3_224,
    HASH_ALG_SHA3_256,
    HASH_ALG_SHA3_384,
    HASH_ALG_SHA3_512,
    HASH_ALG_SHAKE128,
    HASH_ALG_SHAKE256,
    HASH_ALG_SM3,
    HASH_ALG_MAX,
    HASH_ALG_INVALID
} HASH_ALG;

typedef struct hash_context {
    /*
     * currently we don't use below 3 stuffs,
     * just for future use, like hmac, hash_drbg, hmac_drbg and so on.
     */
    HASH_ALG alg;
    uint32_t block_size;
    uint32_t digest_size;

    void     *impl;
}HASH_CTX;

int HASH_Init(HASH_CTX *ctx, HASH_ALG alg);
int HASH_Update(HASH_CTX *ctx, const void *data, size_t len);
int HASH_Final(unsigned char *md, HASH_CTX *ctx);
unsigned char *HASH(HASH_ALG alg, const unsigned char *data, size_t n, unsigned char *md);

/*
 * For SHA-512t, SHAKE128, SHAKE256
 */
int HASH_Init_Ex(HASH_CTX *ctx, HASH_ALG alg, uint32_t ext);
unsigned char *HASH_Ex(HASH_ALG alg, const unsigned char *data, size_t n, unsigned char *md, uint32_t ext);

uint32_t HASH_GetBlockSize(HASH_ALG alg);
uint32_t HASH_GetDigestSize(HASH_ALG alg, uint32_t ext);

#endif
```

###### 3.2 测试代码

-   mgftest.c

这里的测试内容参考维基百科 MGF 词条中使用 python 测试的内容，具体参考代码中的注释段。

```c
#include <stdio.h>
#include "hash.h"
#include "mgf.h"

/*
 * From: https://en.wikipedia.org/wiki/Mask_generation_function
 *
 * Example outputs of MGF1:
 *
 * Python 2.7.6 (default, Sep  9 2014, 15:04:36) 
 * [GCC 4.2.1 Compatible Apple LLVM 6.0 (clang-600.0.39)] on darwin
 * Type "help", "copyright", "credits" or "license" for more information.
 * >>> from mgf1 import mgf1
 * >>> from binascii import hexlify
 * >>> from hashlib import sha256
 * >>> hexlify(mgf1('foo', 3))
 * '1ac907'
 * >>> hexlify(mgf1('foo', 5))
 * '1ac9075cd4'
 * >>> hexlify(mgf1('bar', 5))
 * 'bc0c655e01'
 * >>> hexlify(mgf1('bar', 50))
 * 'bc0c655e016bc2931d85a2e675181adcef7f581f76df2739da74faac41627be2f7f415c89e983fd0ce80ced9878641cb4876'
 * >>> hexlify(mgf1('bar', 50, sha256))
 * '382576a7841021cc28fc4c0948753fb8312090cea942ea4c4e735d10dc724b155f9f6069f289d61daca0cb814502ef04eae1'
 */
int main(int argc, char *argv)
{
    int i;
    char buf[1024];

    MGF1("foo", 3, HASH_ALG_SHA1, 3, buf);
    for (i=0; i<3; i++)
    {
        printf("%02x", ((unsigned char *)buf)[i]);
    }
    printf("\n");

    MGF1("foo", 3, HASH_ALG_SHA1, 5, buf);
    for (i=0; i<5; i++)
    {
        printf("%02x", ((unsigned char *)buf)[i]);
    }
    printf("\n");

    MGF1("bar", 3, HASH_ALG_SHA1, 5, buf);
    for (i=0; i<5; i++)
    {
        printf("%02x", ((unsigned char *)buf)[i]);
    }
    printf("\n");

    MGF1("bar", 3, HASH_ALG_SHA1, 50, buf);
    for (i=0; i<50; i++)
    {
        printf("%02x", ((unsigned char *)buf)[i]);
    }
    printf("\n");

    MGF1("bar", 3, HASH_ALG_SHA256, 50, buf);
    for (i=0; i<50; i++)
    {
        printf("%02x", ((unsigned char *)buf)[i]);
    }
    printf("\n");

    return 0;
}
```

###### 3.3 编译代码和并测试

使用如下命令编译:

```shell
cc mgftest.c mgf.c -o mgftest -I/public/ygu/cryptography/crypto-work.git/out/include -L/public/ygu/cryptography/crypto-work.git/out/lib -lhash
```

这里使用了 `-I` 和 `-L` 选项来指定哈希函数库的头文件和库文件位置。

执行结果如下:

```shell
/public/ygu/cryptography/crypto-work.git/mgf$ ./mgftest 
1ac907
1ac9075cd4
bc0c655e01
bc0c655e016bc2931d85a2e675181adcef7f581f76df2739da74faac41627be2f7f415c89e983fd0ce80ced9878641cb4876
382576a7841021cc28fc4c0948753fb8312090cea942ea4c4e735d10dc724b155f9f6069f289d61daca0cb814502ef04eae1
```

## kernel 安全实现

### 内核安全配置项

- CONFIG_SECURITY_DMESG_RESTRICT

- CONFIG_SECURITY_PERF_EVENTS_RESTRICT
	这将通过dmesg(8)强制对非特权用户阅读内核系统日志进行限制。
	如果不选择此选项，除非将dmesg_restrict sysctl显式设置为(1)，否则不会强制执行任何限制。
- CONFIG_SECURITY
	这允许您选择不同的安全模块配置到您的内核中。
	如果不选择此选项，将使用默认的Linux安全模块。
- CONFIG_SECURITY_WRITABLE_HOOKS
- CONFIG_SECURITYFS
	这将构建securityfs文件系统。它当前被TPM BIOS字符驱动程序和IMA（完整性提供者）使用，但不被SELinux或SMACK使用。
- CONFIG_SECURITY_NETWORK
	  这将启用套接字和网络安全钩子。
	  如果启用，安全模块可以使用这些钩子来实施套接字和网络访问控制。
- CONFIG_PAGE_TABLE_ISOLATION
	此功能通过确保大部分内核地址没有映射到用户空间来减少硬件侧信道的数量。
- CONFIG_SECURITY_INFINIBAND
	  这将启用Infiniband安全钩子。
	  如果启用，安全模块可以使用这些钩子来实施Infiniband访问控制。
- CONFIG_SECURITY_NETWORK_XFRM
	  这将启用XFRM（IPSec）网络安全钩子。
	  如果启用，安全模块可以使用这些钩子来根据IPSec策略中的标签实施每个数据包的访问控制。非IPSec通信被指定为无标签，只有被授权用于通信无标签数据的套接字可以发送而不使用IPSec。
- CONFIG_SECURITY_PATH
	  这将启用基于路径名的访问控制的安全钩子。
	  如果启用，安全模块可以使用这些钩子来实施基于路径名的访问控制。
- CONFIG_INTEL_TXT
	  此选项启用在Trusted Boot（tboot）模块下启动内核的支持。这将利用Intel(R)可信执行技术来进行内核的测量启动。如果系统不支持Intel(R) TXT，则此选项不会产生任何效果。
	  Intel TXT将提供更高的系统配置和初始状态保证，以及数据重置保护。这用于创建强大的初始内核测量和验证，有助于确保内核安全机制正常运行。这种级别的保护需要内核本身之外的根信任。
	  Intel TXT还有助于解决最终用户关于对其硬件是否运行了其配置的VMM或内核的信心的真实关注，特别是因为他们可能负责为其上运行的VM和服务提供这种保证。
	  有关Intel(R) TXT的更多信息，请参见<http://www.intel.com/technology/security/>。
	  有关tboot的更多信息，请参见<http://tboot.sourceforge.net>。
	  有关如何在内核引导中启用Intel TXT支持的说明，请参见Documentation/intel_txt.txt。
- CONFIG_LSM_MMAP_MIN_ADDR
	这是应该受到保护以防止用户空间分配的低虚拟内存部分。阻止用户对低页的写入可以帮助减少内核空指针错误的影响。
	对于大多数ia64、ppc64和x86用户，65536是一个合理的值，并且不应造成问题。
	在arm和其他架构中，它不应超过32768。
	使用vm86功能或需要映射此低地址空间的程序将需要运行LSM系统的特定权限。
- CONFIG_HAVE_HARDENED_USERCOPY_ALLOCATOR
	  堆分配器实现 `__check_heap_object()`，用于验证内存范围与堆对象大小的关系，以支持 CONFIG_HARDENED_USERCOPY。
- CONFIG_HARDENED_USERCOPY
	  当通过 copy_to_user() 和 copy_from_user() 函数将内存复制到/从内核时，此选项检查明显错误的内存区域，拒绝超过指定堆对象大小、跨多个单独分配的页面、不在进程栈上或是内核文本的内存范围。这样可以杜绝整个类别的堆溢出漏洞和类似的内核内存暴露问题。
- CONFIG_HARDENED_USERCOPY_FALLBACK
	  这是一个临时选项，允许通过 WARN() 记录到内核日志中发现缺失的用户复制白名单，而不是拒绝复制，并回退到未列入白名单的加固用户复制，该复制会检查 slab 分配的大小而不是白名单的大小。一旦似乎已经找到并修复了所有缺失的用户复制白名单，该选项将被删除。通过设置 "slab_common.usercopy_fallback=Y/N" 可以更改此设置。
- CONFIG_HARDENED_USERCOPY_PAGESPAN
	  当执行没有 `__GFP_COMP` 的多页分配时，强化的用户复制会拒绝复制它。然而，在内核中还有几种情况下存在此类分配，尚未全部删除。此配置仅用于在寻找这些使用者时使用。
- CONFIG_FORTIFY_SOURCE
	检测编译器可以确定和验证缓冲区大小的常见字符串和内存函数的缓冲区溢出。
- CONFIG_STATIC_USERMODEHELPER
	默认情况下，内核可以通过"用户模式辅助程序"内核接口调用许多不同的用户空间二进制程序。其中一些二进制程序在内核代码本身中静态定义，或作为内核配置选项。然而，其中一些是在运行时动态创建的，或者在内核启动后可以修改。为了提供额外的安全层次，将所有这些调用都路由到一个无法更改名称的单个可执行文件中。
	注意，由此单个二进制文件根据传递给它的第一个参数来调用相应的"真实"用户模式辅助程序二进制文件。如果希望，此程序可以过滤和选择调用哪些真实程序。
	如果希望禁用所有用户模式辅助程序，选择此选项，然后将 STATIC_USERMODEHELPER_PATH 设置为空字符串。
- CONFIG_STATIC_USERMODEHELPER_PATH
	内核在希望运行任何用户模式辅助程序时调用的二进制文件。"真实"应用程序的名称将作为命令行上的此程序的第一个参数传递。
	- CONFIG_DEFAULT_SECURITY_SELINUX
	- CONFIG_DEFAULT_SECURITY_SMACK
	- CONFIG_DEFAULT_SECURITY_TOMOYO
	- CONFIG_DEFAULT_SECURITY_APPARMOR
	- CONFIG_DEFAULT_SECURITY_DAC
- CONFIG_DEFAULT_SECURITY
	默认值为 "selinux" 如果 DEFAULT_SECURITY_SELINUX
	默认值为 "smack" 如果 DEFAULT_SECURITY_SMACK
	默认值为 "tomoyo" 如果 DEFAULT_SECURITY_TOMOYO
	默认值为 "apparmor" 如果 DEFAULT_SECURITY_APPARMOR
	默认值为空字符串 如果 DEFAULT_SECURITY_DAC

### 文件系统

[[dm-verity|DM-Verity]]

dm-verity与私有验签流程的对比：

| 名称/功能     | 只读属性 | 数据完整性 | 数据安全性           | 验签阶段  | 签名算法    |
| --------- | ---- | ----- | --------------- | ----- | ------- |
| 私有验签      | ✔    | ✔     | ✘内核启动后可能被恶意修改   | uboot | rsa-pss |
| dm-verity | ✔    | ✔     | ✔需加验签，可保证每bit数据 | 内核    | 灵活可配    |

### TEE框架

```bash
linux-4.19/drivers/tee
╰─$ tree
.
├── Kconfig
├── Makefile
├── optee                 # OP-TEE内核驱动
│   ├── call.c
│   ├── core.c
│   ├── Kconfig
│   ├── Makefile
│   ├── optee_msg.h
│   ├── optee_private.h
│   ├── optee_smc.h
│   ├── rpc.c
│   ├── shm_pool.c        # 共享内存池
│   ├── shm_pool.h
│   └── supp.c
├── tee_core.c            # 框架核心，包含用户空间操作
├── tee_private.h
├── tee_shm.c
└── tee_shm_pool.c
```

用户空间
include/uapi/linux/tee.h

使用文档
Documentation/tee.txt

## arm安全启动流程ATF/TF-A

ATF/TF-A以及它与UEFI的互动

ARM体系中一些基本概念普及度很低，如ATF/TF-A、Power State Coordination Interface (PSCI)、SMC、Server_Base_Boot_Requirements（SBBR）、Server_Base_System_Architecture（SBSA）、Management Mode（MM）、OPTEE等；还有很多混淆，如ATF和TZ（TrustZone）。

### ATF的由来

TF(Trusted Firmware)是ARM在Armv8引入的安全解决方案，为安全提供了整体解决方案。它包括启动和运行过程中的特权级划分，对Armv7中的TrustZone（TZ）进行了提高，补充了启动过程信任链的传导，细化了运行过程的特权级区间。TF实际有两种Profile，对ARM Profile A的CPU应用TF-A，对ARM Profile M的CPU应用TF-M。我们一般接触的都是TF-A，又因为这个概念是ARM提出的，有时候也缩写做ATF（ARM Trusted Firmware），所以本文对ATF和TF-A不再做特殊说明，ATF也是TF-A，对TF-M感兴趣的读者可以自行查询官网[1](https://zhuanlan.zhihu.com/p/391101179#ref_1)。

有些同学混淆了ATF和TZ的区别。实际上，TZ更多的是和Intel的SGX概念对应，是在CPU和内存中区隔出两个空间：Secure空间和Non-Secure空间。而ATF中有个Firmware概念，它实际上是Intel的Boot Guard、特权级和提高版的TZ的混合体。它在保有TZ的Secure空间和Non-Secure空间的同时，划分了EL0（Exception level 0）到EL3四个特权级：

![[mk.att/Pasted image 20230629105014.png]]

![[mk.att/Pasted image 20230711140540.png]]

ARMv8分为Secure World和Non-Secure World（Normal World），四种异常级别从高到低分别为EL3，EL2，EL1，EL0。

EL3具有最高管理权限，负责安全监测和Secure World和Normal World之间的切换。
EL2主要提供了对虚拟化的支持。
EL1是一个特权模式，能够执行一些特权指令，用于运行各类操作系统，在Secure World则是secure OS（如TEE）。
EL0是无特权模式，所有APP应用都在EL0。

其中EL0和EL1是ATF必须实现的，EL2和EL3是可选的。实际上，没有EL2和EL3，整个模型就基本退化成了ARMv7的TZ版本。从高EL转低EL通过ERET指令，从低EL转高EL通过exception，从而严格区分不同的特权级。其中EL0、EL1、EL2可以分成NS-ELx(None Secure ELx)和S-ELx（Secure ELx）两种，而EL3只有安全模式一种。

ATF带来最大的变化是信任链的建立（Trust Chain），整个启动过程包括从EL3到EL0的信任关系的打通，过程比较抽象。NXP的相关文档[2](https://zhuanlan.zhihu.com/p/391101179#ref_2)比较充分和公开，它的源代码也是开源的[3](https://zhuanlan.zhihu.com/p/391101179#ref_3)。我们结合它的文档和源代码来理解一下。

### ATF启动流程

![[mk.att/Pasted image 20230711135649.png]]

上图中的BL1，BL2，BL31，BL32，BL33分别对应如下功能：

#### BL1

BL1是信任链的根所在，一般是固化在芯片内部的一段代码，叫做bootrom，具有最高的执行权限EL3，在 CPU 出厂时就被写死了。

bootrom通常会被映射到它专属的一块内存地址中，但是如果你尝试向这块地址写入内容，一般都会出错。

芯片上电或复位后，bootrom的代码会从固定位置加载BL2来初始化sram，在BL2 验签通过后会跳转到BL2 ，以保证可信任执行。

#### BL2

BL2和BL1一样，也是运行在EL3特权级别的，不同的是BL2在flash中的一段可信安全启动代码，它的可信建立在BL1对它的验证，主要完成一些平台相关的初始化，比如对ddr的初始化等。

在完成初始化后寻找BL31或者BL33进行执行；如果找到了BL31则不会继续调用BL33，如果没有BL31则BL33必须有。

#### BL31

BL31作为EL3最后的安全堡垒，它不像BL1和BL2是一次性运行的。如它的runtime名字暗示的那样，它通过SMC指令为Non-Secure持续提供设计安全的服务，在Secure World和Non-Secure World之间进行切换。它的主要任务是找到BL32，验签，并运行BL32。

#### BL32

BL32是所谓的secure os，在ARM平台下是 ARM 家的 Trusted Execution Environment（TEE）实现。OP-TEE 是基于ARM TrustZone硬件架构所实现的软件Secure OS。

一般在BL32会运行OPTee OS + 安全app，它是一个可信安全的OS运行在EL1并在EL0启动可信任APP（如指纹信息，移动支付的密码等），并在Trust OS运行完成后通过SMC指令返回BL31，BL31切换到Non-Seucre World继续执行BL33。

> BL32 在不同的平台有不同的实现，Intel 的叫做 Software Guard Extensions（SGX），AMD 的叫做 Platform Security Processor（PSP）

#### BL33

到了BL33这里就是Normal Wrold了，运行的都是非安全固件，也就是我们常见的UEFI firmware或者u-boot，也可能是直接启动Linux kernel。

启动BL1，BL2，BL31，BL32则是一个完整的ATF信任链建立流程（ARM Trusted Firmware），像常见的PSCI（Power State Coordination Interface）功能则是在ATF的BL31上实现。

### 代码

ARM开源了ATF的基本功能模块，大家可以在这里下载：

```bash
git clone  https://github.com/ARM-software/arm-trusted-firmware.git
```

里面已经包含了不少平台，但这些平台的基础代码有些是缺失的，尤其是和芯片部分和与UEFI联动部分。这里我推荐它的一个分支：NXP的2160A芯片的实现。

ARM推出了System Ready计划，效果相当不错，关于它我们今后再单独讲。2020年底，ARM在OSFC推出新的一批System Ready机型[4](https://zhuanlan.zhihu.com/p/391101179#ref_4)，NXP 2160A名列其中：

![[mk.att/Pasted image 20230629105215.png]]

ATF代码下载可以用：

```bash
git clone https://source.codeaurora.org/external/qoriq/qoriq-components/atf -b LX2160_UEFI_ACPI_EAR3
```

UEFI代码下载可以用图片上的地址。我们可以把参考资料2和这些代码对照来看，加深理解。
支持ATF的ARM机器，启动过程如下

![[mk.att/Pasted image 20230629105258.png]]

注意蓝色箭头上的数字，它是启动顺序。一切起源于在EL3的BL1。

**BL1：Trusted Boot ROM**

启动最早的ROM，它可以类比Boot Guard的ACM，

https://zhuanlan.zhihu.com/p/116740555

不过它是在CPU的ROM里而不是和BIOS在一起，是一切的信任根。它的代码在这里：

![[mk.att/Pasted image 20230629105333.png]]

代码很简单（略去不重要内容）：

```text
func bl1_entrypoint
        ....
	bl	bl1_early_platform_setup
	bl	bl1_plat_arch_setup
        ....
	bl	bl1_main
        ....
	b	el3_exit
endfunc bl1_entrypoint
```

bl1_main()开始就是c程序了，那c运行依靠的堆和栈空间在哪里呢？在CPU内部的SRAM里。SRAM一启动就已经可以访问了，bl1_plat_arch_setup（）简单地在其中划分出来一块作为Trusted SRAM给c程序用，而不用像x86在cache里面扣一块出来，简单了很多。

BL1主要目的是建立Trusted SRAM、exception vector、初始化串口console等等。然后找到并验证BL2（验签CSF头），然后跳过去。

**BL2：Trusted Boot Firmware**

同样运行在EL3上的BL2和BL1一个显著的不同是它在Flash上，作为外置的一个Firmware，它的可信建立在BL1对它的验证上。它也有完整的源代码：

![[mk.att/Pasted image 20230629105400.png]]

它也会初始化一些关键安全硬件和软件框架。更主要的是，也是我希望大家下载NXP 2160A的分支的重要原因，BL2会初始化很多硬件，而这些硬件初始化在x86中是BIOS完成的（无论是在PEI中还是包在FSP/AGESA中），而在ARM的ATF体系中，很多种CPU是在BL2中完成的。2160A在Plat目录下提供了很多开源的硬件初始化代码，供ATF BL2框架代码调用。比较重要的是bl2_main()

```c
void bl2_main(void)
{
        ...
	bl2_arch_setup();
        ...
	/* initialize boot source */
	bl2_plat_preload_setup();
	/* Load the subsequent bootloader images. */
	next_bl_ep_info = bl2_load_images();
        ...
	bl2_run_next_image(next_bl_ep_info);
}
```

最重要的两步都在这个函数中完成：初始化硬件和找到BL31。

bl2_plat_preload_setup()中会初始化一堆硬件，包括读取RCW初始化Serdes等，对内存初始化感兴趣的人（比如我）也可以在里面找到初始化DDR4的代码：dram_init（），它在Plat\nxp\drivers\ddr\nxp-ddr下。比较遗憾的是DDR4 PHY的代码是个Binary，不含源码，这里对DDR4的初始化仅仅聚焦设置timing寄存器和功能寄存器，而没有内存的Training过程。

Anyway，x86带内初始化硬件的很多代码ARM ATF体系都包括在BL2中，而不在UEFI代码中，这是和x86 UEFI代码的一个显著区别。部分原因这些代码都要求是Secure的。更加糟糕的是，很多ARM平台，BL1和BL2，甚至后面的BL31都是以二进制的形式提供，让定制显得很困难。BL2能否提供足够的信息和定制化选择给固件厂商和提供足够信息给UEFI代码，考验BL2的具体设计实现。NXP在两个方面都做的不错，不但提供RCW等配置接口，还开源了大部分代码，十分方便。

BL2在初始化硬件后，开始寻找BL3的几个小兄弟：BL31，BL32和BL33。它先找到BL31，并验签它，最后转入BL31。

**BL31：EL3 Runtime Firmware**

BL31作为EL3最后的安全堡垒，它不像BL1和BL2是一次性运行的。如它的runtime名字暗示的那样，它通过SMC为Non-Secure持续提供设计安全的服务。关于SMC的调用calling convention我们今后再详细介绍，这里只需要知道它的服务主要是通过BL32。它负责找到BL32，验签，并运行BL32。

**BL32：OPTee OS + 安全app**

BL32实际上是著名的Open Portable Trusted Execution Enveiroment[5](https://zhuanlan.zhihu.com/p/391101179#ref_5) OS，它是由Linaro创立的。它是个很大的话题，我们今后再细聊。现在仅需要知道OPTee OS运行在 S-EL1，而其上的安全APP运行在S-EL0。OPTee OS运行完毕后，返回EL3的BL31，BL31找到BL33，验签它并运行。

**BL33： Non-Trusted Firmware**

BL33实际上就是UEFI firmware或者uboot，也有实现在这里直接放上Linux Kernel。2160A的实现是UEFI和uboot都支持。我们仅仅来看UEFI的路径。

第一次看到UEFI居然是Non-Trusted，我是有点伤心的。UEFI运行在NS_EL2，程序的入口点在ARM package

```
edk2/ArmPlatformPkg/PrePi/AArch64/ModuleEntryPoint.S
```

做了一些简单初始化，就跳到C语言的入口点CEntryPoint()。其中ArmPlatformInitialize()做了一些硬件初始化，调用了 `edk2-platforms/Silicon/NXP/` 的代码。重要的是PrimaryMain()。

PrimaryMain()有两个实例，2160A NXP选择的是PrePI的版本（edk2/ArmPlatformPkg/PrePi/MainUniCore.c），说明它跳过了SEC的部分，直接进入了PEI的后期阶段，在BL2已经干好了大部分硬件初始化的情况下，这个也是正常选择。PrePI的实例直接调用PrePiMain()（仅保留重要部分）。

```c
VOID
PrePiMain (
  IN  UINTN                     UefiMemoryBase,
  IN  UINTN                     StacksBase,
  IN  UINT64                    StartTimeStamp
  )
{
  ....
  ArchInitialize ();
  SerialPortInitialize ();
  InitializeDebugAgent (DEBUG_AGENT_INIT_POSTMEM_SEC, NULL, NULL);
  // Initialize MMU and Memory HOBs (Resource Descriptor HOBs)
  Status = MemoryPeim (UefiMemoryBase, FixedPcdGet32 (PcdSystemMemoryUefiRegionSize));
  BuildCpuHob (ArmGetPhysicalAddressBits (), PcdGet8 (PcdPrePiCpuIoSize));
  BuildGuidDataHob (&gEfiFirmwarePerformanceGuid, &Performance, sizeof (Performance));
  SetBootMode (ArmPlatformGetBootMode ());
  // Initialize Platform HOBs (CpuHob and FvHob)
  Status = PlatformPeim ();
  ....
  Status = DecompressFirstFv ();
  Status = LoadDxeCoreFromFv (NULL, 0);
}
```

从中我们可以看到，这里几乎就是UEFI PEI阶段DXEIPL的阶段了，后面就是直接DXE阶段。

好了，我们来梳理一下，ATF整个信任链条是逐步建立的：

![[mk.att/Pasted image 20230629105801.png]]

从作为信任根的BL1开始，一步一步验签CSF头中的签名，最后来到BL33，后面就是OS了。那BL33后面怎么就断了呢？其实后面的验签就是UEFI Secure Boot了

https://zhuanlan.zhihu.com/p/30136593

![[mk.att/Pasted image 20230629105832.png]]

### 关于 trustzone & ATF & OPTEE

TrustZone，ATF，OPTEE 这三者有什么关系呢？

TrustZone是一种架构，支持ATF的硬件。ATF是软件，包含bl2 + bl31 + bl32 + bl33，bl32=optee-os，bl33=u-boot。

#### trustzone

TrustZone是ARM针对消费电子设备设计的一种硬件架构，它对ARM的扩展，其实只是增加了一条指令,一个配置状态位(NS位),以及一个新的有别于核心态和用户态的安全态。其目的是为消费电子产品构建一个安全框架来抵御各种可能的攻击。

TrustZone在概念上将SOC的硬件和软件资源划分为安全(Secure World)和非安全(Normal World)两个世界，所有需要保密的操作在安全世界执行（如指纹识别、密码处理、数据加解密、安全认证等），其余操作在非安全世界执行（如用户操作系统、各种应用程序等），安全世界和非安全世界通过一个名为Monitor Mode的模式进行转换。

![[mk.att/Pasted image 20230711144947.png]]

处理器架构上，TrustZone将每个物理核虚拟为两个核，一个非安全核（Non-secure Core, NS Core），运行非安全世界的代码；和另一个安全核（Secure Core），运行安全世界的代码。

两个虚拟的核以基于时间片的方式运行，根据需要实时占用物理核，并通过Monitor Mode在安全世界和非安全世界之间切换，类似同一CPU下的多应用程序环境。

不同的是多应用程序环境下操作系统实现的是进程间切换，而Trustzone下的Monitor Mode实现了同一CPU上两个操作系统间的切换。

#### OPTEE

OPTEE是一个通常运行在 Secure World EL1 权限中的内核程序，比较常见的是基于开源的 ARM Trusted Firmware 进行扩展修改的，别的实现还有基于 Little Kernel 的，以及一些芯片厂家自己的实现。

它的主要作用是给 Secure World 中运行的程序提供一个基本的系统内核，实现多任务调度、虚拟内存管理、System Call 回调、硬件驱动、IPC 通讯等等。

#### ATF

TF(Trusted Firmware)是ARM在Armv8引入的安全解决方案，为安全提供了整体解决方案。它包括启动和运行过程中的特权级划分，对Armv7中的TrustZone（TZ）进行了提高，补充了启动过程信任链的传导，细化了运行过程的特权级区间。

TF实际有两种Profile，对ARM Profile A的CPU应用TF-A，对ARM Profile M的CPU应用TF-M。我们一般接触的都是TF-A，又因为这个概念是ARM提出的，有时候也缩写做ATF（ARM Trusted Firmware）。

ATF带来最大的变化是信任链的建立（Trust Chain），整个启动过程包括从EL3到EL0的信任关系的打通。

ATF的启动流程包括5个单独的启动阶段，在不同的异常级别运行，如下表所示。

![[mk.att/Pasted image 20230711145020.png]]

## 关于TEE和ARM TZ的总结

### 什么是TEE

首先，让我们首先确定“受信任”和“值得信赖”一词之间的细微区别。可信是指您依赖的人或某事不会损害您的安全，而可信意味着某人或某事不会损害您的安全。或者换句话说，您可以将“可信”视为您如何使用某些东西，而“可信赖”是关于使用某些东西是否安全。因此，您可以选择依赖可信的执行环境来执行敏感任务，当然希望它们也是值得信赖的！一般来说，TEE 可能希望实现五个安全属性：

-   隔离执行
-   安全存储
-   远程证明
-   安全配置
-   可信路径

### 现在有哪些类型的TEE可供选择？

如今，有几个TEE平台可供研究界和行业使用，包括：

-   TPM（受信任的平台模块）。TPM 是一种专用微处理器，旨在通过将加密密钥集成到设备中来保护硬件，可用于许多现代计算机。为了利用 TPM 的安全基元，应用程序通常将 TPM（硬件）和 TXT（软件）组合在一起以提供强隔离。需要指出的一件事是，TPM 真的很慢，供应商没有任何动机来保持它更快，他们只是确保它以低成本工作！
-   英特尔的 TXT（可信执行技术）或 AMD 的 SVM（安全虚拟机）。要使用 TXT，需要执行以下几个步骤：
    -   1. 挂起操作系统
    -   2. 在主 CPU 上执行少量代码（可信代码）
    -   3. 恢复操作系统。
    -   虽然这三个步骤看似简单，但实际上没有商业应用程序使用这些技术，原因如下：首先，当 TXT 打开时，即使在多核机器中也只允许一个 CPU 执行，而其他内核则挂起;其次，TXT 中没有中断和 IO 操作，并且为了保持 TCB 尽可能小，没有可用的操作系统库，这意味着您需要付出巨大的努力才能运行丰富的功能应用程序。
-   基于虚拟机管理程序的 TEE。虚拟化是实现 TEE 的简单方法，并且有大量系统使用基于虚拟机管理程序的解决方案来提供类似 TEE 的功能。
-   ARM TrustZone。ARM TrustZone 被认为是在移动设备（或 ARM 设备）中实现 TEE 的最有前途的技术。

### 什么是 ARM TrustZone？

ARM TrustZone 技术旨在建立对基于 ARM 的平台的信任。与设计为具有预定义功能集的固定功能设备的 TPM 相比，TrustZone 通过利用 CPU 作为可自由编程的可信平台模块，提供了一种更加灵活的方法。为此，ARM在常规正常模式之外引入了一种称为“安全模式”的特殊CPU模式，从而建立了“安全世界”和“正常世界”的概念。这两个世界之间的区别与用户级和内核级代码之间的正常环形保护完全正交，并且对在正常世界中运行的操作系统隐藏。

此外，它不仅限于CPU，而是通过系统总线传播到外围设备和内存控制器。这样，这样一个基于 ARM 的平台实际上变成了一种人格分裂。当安全模式处于活动状态时，CPU 上运行的软件在整个系统上的视图与在非安全模式下运行的软件不同。这样，系统功能，特别是安全功能和加密凭据，可以在正常世界中隐藏。不言而喻，这个概念比TPM芯片灵活得多，因为安全世界的功能是由系统软件定义的，而不是硬连线的。

### ARM TZ 的详细信息

TrustZone 硬件架构包括与 SoC 连接的 SoC 和外围设备。SoC 包括核心处理器、直接内存访问 （DMA）、安全 RAM、安全启动 ROM、通用中断控制器 （GIC）、信任区地址空间控制器 （TZASC）、信任区保护控制器、动态内存控制器 （DMC） 和 DRAM，它们通过 AXI 总线相互通信。SoC 使用 AXI 到 APB 桥接器与外围设备通信。

### 信任地带硬件

#### 基于分裂世界的隔离执行

支持TrustZone的物理核心处理器在两个环境中安全高效地工作：正常世界（或非安全世界）和安全世界。CPU状态存储在两个世界之间，默认情况下，安全世界可以访问正常世界的所有状态，反之则不然。在这两个世界之下，有一个称为TrustZone监控模式的更高权限模式，通常用于通过执行安全监控调用（SMC）指令或外部中断在两个世界之间切换，并负责存储CPU状态。

#### 存储器和外设保护

TrustZone 通过使用 TZASC 和 TZPC 支持两个世界之间的内存分区。TZASC可以将DRAM划分为多个内存区域，每个内存区域都可以配置为在正常世界或安全世界中使用，或者使用更动态和更复杂的访问权限控制。默认情况下，安全世界应用程序可以访问正常世界内存，反之则不然。但是，通过在 TZASC 中启用安全反转，也可以将一个正常世界内存配置为仅正常世界访问。

TZPC主要用于将外围设备配置为安全或不安全，全球敏感的AXI到APB桥接器将拒绝非法访问受保护的外围设备。除此之外，ROM或SRAM等SoC静态存储器也需要保护。这是由称为TrustZone Memory Adapter （TZMA）的SoC外设完成的，尽管TZMA没有提供直接的软件配置寄存器。通常内部ROM通过硬件设计设置为安全，SRAM中安全和非安全区域的分区通过在TZPC中设置R0SIZE寄存器来配置。TZASC 和 TZPC 只能在安全的环境中访问和配置。安全访问冲突可能会导致外部中止和陷阱到监视模式或当前 CPU 状态模式异常向量，具体取决于监视模式下中断行为的配置。

除了物理内存分区之外，信任区感知 MMU 还允许具有不同转换表和 TLB 中的标记的两个世界来标识世界，以避免在切换世界时刷新 TLB。对于直接内存访问 （DMA），有一个称为直接内存访问控制器 （DMAC） 的多通道系统控制器，用于在物理内存系统中移动数据。DMAC 对世界敏感，支持并发安全和非安全通道。正常情况下，DMA将数据传输到安全内存或从安全内存传输数据将被拒绝，从而避免安全漏洞。

#### 中断隔离

有两种类型的中断：IRQ（正常中断请求）和FIQ（快速中断请求）。具有信任区域支持的 GIC 可以将中断配置为安全或不安全。通常，IRQ 配置为普通全局源，FIQ 配置为安全全局源，因为 IRQ 是现代操作系统中最常用的中断源。在安全世界中执行时，安全中断将由安全世界中断处理程序处理;当安全世界执行期间发生非安全中断时，中断将被传输到监控模式中断处理程序，软件处理程序可以决定是丢弃中断还是切换到正常世界。与安全相关的GIC配置只能在安全世界中配置，从而防止来自正常世界的非法修改。安全配置寄存器（SCR）位于控制协处理器CP15中，只能在安全特权模式下访问，可以编程为捕获外部中止（即违反内存访问权限），IRQ或FIQ进入监视模式或在当前世界中本地处理中断。

### 是否可以使用 ARM TrustZone 来实施或取代虚拟化？

正如许多研究人员所提出的那样，可以从两个角度看待ARM TrustZone，作为虚拟化解决方案和实现类似于可信平台模块（TPM）功能的机制。当被视为虚拟化解决方案时，TrustZone 严重缺乏：

1.  虚拟机的数量限制为两个，一个在安全环境中运行，一个在非安全环境中运行。
2.  用于模拟设备的陷阱和执行模型是不可能的，因为安全违规中止始终是异步的。

因此，为了支持设备仿真，必须修改非安全操作系统的某些设备驱动程序，因此无法运行像 Windows 这样的操作系统（只有二进制文件可用）。我敢说，将TrustZone视为虚拟化机制不是一个好的选择！当将TrustZone视为TPM的替代方案时，该技术背后的动机变得更加清晰。与固定功能的TPM相比，TrustZone是一种更加通用的机制，具有无限的资源和快速的芯片。

### ARM TrustZone 可以直接用作 TPM 吗？ARM TrustZone 是否提供安全的密钥存储？

恐怕不是。问题在于缺乏安全存储，因为 TrustZone 规范没有提供任何实现安全存储的机制。但是，TrustZone 功能：仅将特定外设分配给安全全局访问是关键点，但由 SoC 供应商或 TEE 开发人员决定将哪些外设用作安全存储介质。

**此例为Trusty TEE，与使用OP-TEE基本一致**。

Trusty 是一种安全的操作系统 (OS)，可为 Android 提供可信执行环境 (TEE)。Trusty OS 与 Android OS 在同一处理器上运行，但 Trusty 通过硬件和软件与系统的其余组件隔离开来。Trusty 与 Android 彼此并行运行。Trusty 可以访问设备主处理器和内存的全部功能，但完全隔离。隔离可以保护 Trusty 免受用户安装的恶意应用以及可能在 Android 中发现的潜在漏洞的侵害。

Trusty 与 ARM 和 Intel 处理器兼容。在 ARM 系统中，Trusty 使用 ARM 的 Trustzone™ 虚拟化主处理器，并创建安全的可信执行环境。使用 Intel 虚拟化技术的 Intel x86 平台也提供类似的支持。

![[mk.att/Pasted image 20230706150958.png]]

Android 支持各种 TEE 实现，因此您并非只能使用 Trusty。每一种 TEE OS 都会通过一种独特的方式部署可信应用。对试图确保应用能够在所有 Android 设备上正常运行的可信应用开发者来说，这种不统一性可能是一个问题。 使用 Trusty 作为标准，可以帮助应用开发者轻松地创建和部署应用，而不用考虑有多个 TEE 系统的不统一性。借助 Trusty TEE，开发者和合作伙伴能够实现透明化、进行协作、检查代码并轻松地进行调试。可信应用的开发者可以集中利用各种常见的工具和 API，以降低引入安全漏洞的风险。 这些开发者可以确信自己能够开发应用并让此应用在多个设备上重复使用，而不需要进一步进行开发。

### 第三方 Trust 应用

目前，所有 Trust 应用都是由第一开发方开发的，并用 Trusty 内核映像进行封装。 整个映像都经过签名并在启动过程中由引导加载程序进行验证。 目前，Trusty 不支持第三方应用开发。尽管 Trusty 支持开发新应用，但在开发新应用时务必要万分谨慎；每个新应用都会使系统可信计算基 (TCB) 的范围增大。 可信应用可以访问设备机密数据，并且可以利用这些数据进行计算或数据转换。能够开发在 TEE 中运行的新应用为创新带来了多种可能性。不过，根据 TEE 的定义，如果这些应用没有附带某种形式的可信凭据，则无法分发。这种凭据通常采用数字签名的形式，即由应用运行时所在产品的用户信任的实体提供的数字签名。

### 用途和示例

可信执行环境正迅速成为移动设备领域的一项标准。用户的日常生活越来越依赖移动设备，对安全的需求也在不断增长。 具有 TEE 的移动设备比没有 TEE 的设备更安全。

在具有 TEE 实现的设备上，主处理器通常称为“不可信”处理器，这意味着它无法访问制造商用于存储机密数据（例如设备专用加密秘钥）的特定 RAM、硬件寄存器和一次写入 fuse 区域。 在主处理器上运行的软件会将所有需要使用机密数据的操作委派给 TEE 处理器。

在 Android 生态系统中，最广为人知的示例是受保护内容的 [DRM 框架](https://source.android.google.cn/docs/core/media/drm?hl=zh-cn)。在 TEE 处理器上运行的软件可以访问解密受保护内容所需的设备专用密钥。 主处理器只能看到已加密的内容，这样一来，就可以针对软件类攻击提供高级别的安全保障和保护。

TEE 还有许多其他用途，例如移动支付、安全银行、多重身份验证、设备重置保护、抗重放攻击的持久存储、安全 PIN 和指纹处理，甚至还能用于恶意软件检测。

## SELinux

作为 Android [安全模型](https://source.android.google.cn/security?hl=zh-cn)的一部分，Android 使用安全增强型 Linux (SELinux) 对所有进程强制执行强制访问控制 (MAC)，甚至包括以 Root/超级用户权限运行的进程（Linux 功能）。很多公司和组织都为 Android 的 [SELinux 实现](https://android.googlesource.com/platform/external/selinux/)做出了贡献。借助 SELinux，Android 可以更好地保护和限制系统服务、控制对应用数据和系统日志的访问、降低恶意软件的影响，并保护用户免遭移动设备上的代码可能存在的缺陷的影响。

SELinux 按照默认拒绝的原则运行：任何未经明确允许的行为都会被拒绝。SELinux 可按两种全局模式运行：

-   宽容模式：权限拒绝事件会被记录下来，但不会被强制执行。
-   强制模式：权限拒绝事件会被记录下来**并**强制执行。

Android 中包含 SELinux（处于强制模式）和默认适用于整个 AOSP 的相应安全政策。在强制模式下，非法操作会被阻止，并且尝试进行的所有违规行为都会被内核记录到 `dmesg` 和 `logcat`。开发时，您应该先利用这些错误信息对软件和 SELinux 政策进行优化，再对它们进行强制执行。如需了解详情，请参阅[实现 SELinux](https://source.android.google.cn/security/selinux/implement?hl=zh-cn)。

此外，SELinux 还支持基于域的宽容模式。在这种模式下，可将特定域（进程）设为宽容模式，同时使系统的其余部分处于全局强制模式。简单来说，域是安全政策中用于标识一个进程或一组进程的标签，安全政策会以相同的方式处理所有具有相同域标签的进程。借助基于域的宽容模式，可逐步将 SELinux 应用于系统中越来越多的部分，还可以为新服务制定政策（同时确保系统的其余部分处于强制模式）。

## 方法论

### ATF

学习在Linux上使用ARM Trusted Firmware（ATF）可以按照以下步骤进行：

1. 了解ARM体系结构：在学习ATF之前，建议先对ARM处理器架构有一定的了解，包括ARM处理器的基本特性、寄存器和指令集等。

2. 阅读官方文档：ARM官方提供了详细的ARM Trusted Firmware文档，包括技术参考手册、编程指南和API文档等。可以从ARM官方网站下载相关文档，并详细研读，了解ATF的功能、接口和用法。

3. 实践示例项目：ARM官方提供了一些示例项目，这些项目包含了使用ATF的典型应用场景，可以帮助你学习和理解ATF的用法。可以尝试下载这些示例项目，并根据文档进行构建、调试和运行。

4. 参考开源社区：ATF是一个开源项目，有许多开发者和社区活跃于该领域。可以查阅开源社区中的ATF相关讨论、博客文章和教程，从其他开发者的经验中学习。

5. 进行实际开发：一旦你掌握了ATF的基本概念和用法，可以尝试自己进行实际的开发。可以选择一个适合的硬件平台，根据需求进行ATF的配置、编译和集成。

需要注意的是，学习ATF需要一定的ARM体系结构和操作系统相关的知识。如果你不熟悉ARM处理器或Linux内核开发，可能需要先学习相关知识再深入学习ATF。此外，ATF的使用也与具体的硬件平台和系统配置有关，要根据实际情况进行调整和应用。

---

当学习在Linux上使用ARM Trusted Firmware（ATF）时，以下是一些推荐的学习资料：

1. ARM官方文档：ARM官方提供了详细的ARM Trusted Firmware文档，包括技术参考手册、编程指南和API文档等。你可以从ARM官方网站的文档中心获取这些资料。

2. ARM Trusted Firmware GitHub仓库：ARM Trusted Firmware作为一个开源项目，其代码托管在GitHub上。你可以访问ATF的GitHub仓库，查看源代码、文档和示例项目。地址：https://github.com/ARM-software/arm-trusted-firmware

3. Linaro网站：Linaro是一个致力于ARM平台优化的开源软件工程组织，为ARM生态系统提供了很多有用的资源和工具。他们在ARM Trusted Firmware方面也有很多投入，你可以在他们的网站上找到相关的文档和教程。地址：https://www.linaro.org/

4. 论坛和社区：在各种ARM开发者论坛和社区中，你可以找到许多关于ATF的讨论和经验分享。例如，ARM Developer Community论坛（https://community.arm.com/developer/start）和Linaro论坛（https://discuss.linaro.org/）等。在这些地方与其他开发者互动、提问和分享经验，可以加速你的学习过程。

5. 书籍和教程：有一些书籍和在线教程也可以帮助你学习ARM Trusted Firmware。例如，《ARM Trust Zone Software Design Using Open Source Technology》（https://www.amazon.com/ARM-Software-Using-Source-Technology/dp/1461493172）是一本关于ARM TrustZone技术和ATF的书籍，可以作为参考资料之一。

### OP-TEE

学习 OP-TEE（Open Portable Trusted Execution Environment）可以通过以下步骤进行：

1. 了解基础概念：首先，你可以阅读 OP-TEE 的官方文档，以了解其基本概念、架构和工作原理。这些文档通常包括用户指南、架构设计和开发者文档。

2. 设置开发环境：为了学习和使用 OP-TEE，你需要设置相应的开发环境。这包括安装必要的软件工具链、OP-TEE 的源代码和相关库文件。通常，OP-TEE 的官方文档中会提供详细的安装指南和相关资源。

3. 构建和运行示例项目：OP-TEE 提供了一些示例项目，用于演示如何使用 OP-TEE 开发安全应用和服务。你可以从 OP-TEE 的官方 GitHub 仓库中获取这些示例项目，并按照文档的指导进行构建和运行。这将帮助你对 OP-TEE 的使用和开发流程有更深入的了解。

4. 参与开源社区：OP-TEE 拥有一个活跃的开源社区，你可以通过参与社区讨论、邮件列表和论坛来学习和交流。在社区中与其他开发者互动，分享问题和经验，这对于加速学习过程非常有帮助。

5. 实践项目开发：一旦你掌握了 OP-TEE 的基本概念和技术，可以尝试自己进行实践项目的开发。选择一个合适的应用场景，使用 OP-TEE 开发安全的应用程序或服务，这将帮助你深入理解并应用所学的知识。

6. 持续学习和探索：OP-TEE 是一个不断发展和成长的项目，因此持续学习和探索是很重要的。保持关注 OP-TEE 的最新版本和更新，参与相关的研讨会、培训课程和技术活动，以保持与该领域的最新发展保持同步。

需要注意的是，学习 OP-TEE 需要一定的 ARM 架构和操作系统开发的基础知识。如果你对 ARM 处理器或 Linux 内核开发不熟悉，可能需要先学习这些基础知识再深入学习 OP-TEE。
