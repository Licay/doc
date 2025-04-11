## 背景

ARMv8支持多核系统，比如说一个Cortex-A57MPCore 或者 Cortex-A53MPCore的处理器可以包含一个或者四个core。ARM多核技术允许每个core并发执行，而且在空闲时允许独立睡眠。多核系统会共享很多资源比如电源、时钟、cahce，那它们共享方式以及同步方式是怎样的？

## Multi-processing systems(多处理器系统)

-   包含一个核的处理器。
-   包含多核的处理器。比如多个Cortex-A53 core组成一个cluster。
-   包含多cluster的处理器。

### Symmetric multi-processing(SMP对称多处理器)

对称多处理(SMP)是一种软件架构，它动态地确定各个核心的角色。集群中的每个核心都有相同的内存视图和共享的硬件。任何应用程序、进程或任务都可以在任何核心上运行，操作系统调度器可以动态地在核心之间迁移任务，以实现最佳的系统负载。一个多线程应用程序可以同时运行在几个核心上。

application跑在OS上称为一个`process`。application会向kernel发起系统调度，每个process拥有对应的资源包括堆栈、常量、优先级。process对应为kernel视角中的一个`task`。`thread`是在相同的进程空间中执行的独立任务，它使应用程序的不同部分能够在不同的核心上并行执行。

通常，**一个进程中的所有线程共享多个全局资源**(包括相同的内存映射和对任何打开文件和资源句柄的访问)。线程也有它们自己的本地资源，包括它们自己的栈和寄存器使用情况，这些使用情况由内核在上下文切换时保存和恢复。然而，这些资源是本地的这一事实并不意味着任何线程的本地资源就能保证免受其他线程的错误访问。线程是单独调度的，在同一个进程中的线程也可以有不同的优先级。

SMP系统的kernel调度器会对任务做负载均衡。SMP会考虑这样的事情，在更多的核上以更低的频率运行任务，在更少的核上以更高的频率运行任务，哪种会是最佳的功耗和性能平衡的场景。中断处理也可以在各个核心之间实现负载均衡。这可以帮助提高性能或节省能源。在核心之间平衡中断或为特定类型的中断保留核心可以减少中断延迟。**kernel不支持自动的中断负载均衡**，但是kernel提供了绑定中断的机制。有一些开源项目，如`irqbalance` ([https://github.com/Irqbalance/irqbalance](https://github.com/Irqbalance/irqbalance))，它们使用绑定中断机制安排中断的分布。Irqbalance知道系统属性，比如共享缓存层次结构(哪些核心具有公共缓存)和电源域布局(哪些核心可以独立关闭)。

### Timer(时钟)

支持SMP的操作系统内核通常有一个任务调度器，负责在多个任务之间对核心上进行时间分片。它动态地确定各个任务的优先级，并决定在每个核心上下一步运行哪个任务。为了周期性地中断每个核心上的活动任务，通常需要一个计时器，让调度器有机会选择不同的任务。

当所有核心都竞争相同的关键资源时，可能会出现问题。每个核心运行调度器来决定它应该执行哪个任务，这以固定的间隔发生。内核调度器代码需要使用一些共享数据，比如任务列表，可以通过排除(由互斥锁提供)对其进行保护，防止并发访问。

系统计时器体系结构描述了一个通用的系统计数器，它为每个核心提供了最多四个计时器通道。这个系统计数器时钟频率是固定的。有安全物理计时器和非安全物理计时器以及两个用于虚拟化目的的计时器。每个通道都有一个比较器，它比较系统范围内的64位计数，从0开始计数。你可以配置计时器，以便当计数大于大于或等于编程的比较器值时产生中断。虽然系统计时器必须有固定的频率(通常是MHz)，但允许变化的更新粒度。这意味着，不是在每个时钟滴答上增加1的计数，而是可以在每10或100个周期中相应地以减少的速率增加计时器更大的数量，如10或100。**这提供了同样有效的频率，但减少了更新的粒度**。这对于实现低功耗状态很有用。

**CNTFRQ_EL0寄存器报告系统定时器的频率**。一个常见的误解是CNTFRQ_EL0由所有核心共享，其实这个寄存器是per core的。所有软件看到这个寄存器已经初始化为正确值在所有核上。计数器频率是全局的，对所有核心都是固定的。**CNTFRQ_EL0为bootrom或固件提供了一种方便的方式来告诉其他软件全局计数器频率是多少，但是不控制硬件行为的任何方面**。

CNTPCT_EL0寄存器报告当前的计数值。CNTKCTL_EL1控制EL0是否可以访问系统定时器。配置定时器的步骤如下:

1. 将比较器值写入64位寄存器CNTP_CVAL_EL0。
1. 在CNTP_CTL_EL0中使能计数器和中断生成。
1. 轮询CTP_CTL_EL0以报告EL0定时器中断的原始状态。

您可以使用系统计时器作为倒计时计时器。在本例中，所需的计数被写入到32位的CNTP_TVAL_EL0寄存器。硬件为您计算正确的CNTP_CVAL_EL0值。

### Synchronization(同步)

ARM架构提供了三条与独占访问相关的指令(读、写、清)，以及这些指令的变体，它们操作字节、半字、字或双字大小的数据。指令依赖于核心或内存系统的能力来标记特定地址，以便由该核心使用独占访问监视器进行独占访问监视。

指令依赖于core或内存系统的能力，以便该core可以独占访问标记的特定地址。这些指令的使用在多核系统中很常见，但在单核系统中也可以找到，用于实现运行在同一核心上的线程之间的同步操作。

LDXR执行读取内存，但也标记物理地址，以便由该核心进行独占访问。STXR执行一个条件写内存，只有当目标位置被标记为被该核心监控的独占访问时才成功。如果存储不成功，这个指令在通用寄存器Ws中返回非零，如果存储成功，则的值为0。在汇编程序语法中，它总是被指定为W寄存器，也就是说，不是X寄存器。此外，STXR清除独占标记。

```csharp
1. Load Exclusive (LDXR): LDXR W|Xt, [Xn]
2. Store Exclusive (STXR): STXR Ws, W|Xt, [Xn] where Ws indicates whether the storecompleted successfully. 0 = success.
3. Clear Exclusive access monitor (CLREX) This is used to clear the state of the Local Exclusive Monitor.
```

Load exclusive和store exclusive操作只保证在普通内存中起作用，以下所有属性的普通内存均可：

- Inner or Outer Shareable.
- Inner Write-Back.
- Outer Write-Back.
- Read and Write allocate hints.
- Not transient.

**每个核心只能标记一个地址。独占监视器并不阻止其他核心或线程读写所监视的位置，而是简单地监视自LDXR以来该位置是否已被写入**。

### Asymmetric multi-processing(AMP非对称多处理器)

**在多核处理器上实现AMP系统的原因可能包括安全性、实时性，或者因为单个核专用于执行特定的任务**。

非对称多处理 (AMP) 系统使您能够为cluster内的每个核心静态分配角色，这样实际上，每个核心执行单独任务。这被称为功能分布软件架构，通常**意味着在各个核心上运行单独的操作系统**。该系统可以认为是一个为某些关键系统配备专用加速器的单核系统服务。 AMP不是指任务或中断绑定特定的核心。在 AMP 系统中，每个任务可以有不同的内存视图，不能再AMP之间进行负载均衡。此类系统的硬件缓存一致性没有要求。

有一些系统同时具有SMP和AMP特性。这意味着有两个或更多的核心运行一个SMP OS，其他核心运行独立的OS。

> 独立的核心需要一些同步机制，可以通过消息通信协议Multicore Communications Association API (MCAPI)，基本上就是门铃机制+共享内存来实现消息收发。

### Heterogeneous multi-processing(HMP异构多处理器)

ARM用HMP来表示一个由应用处理器集群组成的系统，这些处理器在指令集架构上100%相同，但在微架构上却非常不同。所有处理器都是完全缓存一致的，并且是同一个一致性域的一部分。**ARM的big.LITTLE架构就是HMP的实现**。在一个big.LITTLE系统，节能的LITTLE核心与高性能大核心相结合，形成一个系统。

### Exclusive monitor system location

典型的多核系统可能包括多个独占监视器。**每个核心都有自己的本地监视器，并且有一个或多个全局监视器**。翻译表项的可共享和可缓存属性与用于独占指令的位置相关，从而决定了使用哪一个独占监视器。

在硬件中，核心包括一个名为本地监视器的设备。这个监视器观察核心。当核心执行独占加载访问时，它将在本地监视器中记录这一事件。当它执行独占存储时，它检查之前的独占加载是否执行，如果没有执行，则独占存储失败。核心一次只能标记一个物理地址。

**本地独占监视器在每次异常返回时(即在ERET指令执行时)被清除**。在Linux内核中，多个任务在EL1的内核上下文中运行，并且可以进行上下文切换。只有当我们返回到与内核任务相关的用户空间线程时，才会执行异常返回。这与ARMv7架构不同，内核任务调度器必须显式地清除每个任务开关上的独占访问监控器。

**本地监视器使用场景**：当用于独占访问的位置标记为不可共享时，即仅在同一个核心上运行的不同线程，将使用本地监视器。例如线程A发起独占load后，中断导致同一核心上的另一线程B也发起了独占load，这时就会使用本地监视器。本地监视器还可以处理将访问标记为内部可共享的情况，例如，保护可共享域中任何核心上运行的SMP线程之间共享资源的互斥锁。

**全局监视器使用场景**：对于运行在不同的、非一致的核心上的线程，互斥锁的位置被标记为正常的、不可缓存的，并且需要在系统中使用全局访问监视器。

**系统可能不包含全局监视器，或者全局监视器可能只对某些地址区域可用**。如果执行独占访问的位置在系统中没有合适的监视器，会发生什么情况，这是由IMPLEMENTATION定义的。一些允许的选项：指令产生External Abort，MMU fault，NOP，排他指令被当做简单的读写指令。

**独占监视器的粒度Exclusives Reservation Granule (ERG)是IMPLEMENTAION DEFINED**，但通常是一条缓存行cache line。它给出监视器区分它们的最小地址间距。在单个ERG中放置两个互斥量可能会导致错误，对其中一个互斥量执行STXR指令会清除这两个互斥量的独占标记。这个ERG限制可能会降低效率。特定核心上独占监视器的ERG大小可以从缓存类型寄存器`CTR_EL0`中读取。

## Cache coherency

Cortex-A53和Cortex-A57处理器支持集群中不同核心之间的一致性管理。这需要用正确标记地址区域的可共享属性。允许构建包含多cluster的系统，这样的系统级一致性需要一个缓存相干互连，例如ARM CCI-400，它实现了AMBA 4 ACE总线规范。例如下图的一致性总线。

![[mk.att/Pasted image 20240530145415.png]]

**系统中的一致性支持取决于硬件设计决策和存在许多可能的配置**。例如，一致性只能在单个cluster中得到支持。一个big.LITTLE系统可能内部域包括两个集群的核心，或者是一个多cluster系统，其中内部域包括一部分cluster，外部域包括其他cluster。

除了维护缓存之间数据一致性的硬件之外，**你必须能够在一个核心上广播缓存一致性操作到系统其他部分**。 有的硬件配置信号，在复位时采样，它控制是广播内部还是外部或两个缓存维护操作，以及是否广播系统屏障指令。AMBA 4 ACE协议允许向其他master发送屏障信号，以便维护和一致性操作的顺序得到维护。Interconnect逻辑可能需要通过引导代码进行初始化。

对于普通可缓存区域，这意味着将可共享属性设置为不可共享、内部可共享或外部可共享中的一个。`对于不可缓存的区域，可共享的属性将被忽略。`缓存操作可能需要广播到互连。这意味着一个核心上的软件可以向一个地址发出缓存清除或无效操作，该地址目前可能存储在持有该地址的另一个核心的数据缓存中。**当一个维护操作通过广播时，特定共享性域中的所有内核执行该操作**。SMP系统依赖这些操作进行同步。

![[mk.att/Pasted image 20240530145431.png]]

## Multi-core cache coherency within a cluster(多核cache一致性)

一致性意味着确保系统中所有的处理器或总线master具有共享内存的相同视角。这意味着对一个核心缓存中保存的数据进行修改对于其他核心是可见的，这使得核心不可能看到过时的或旧的数据副本。

-   软件管理的一致性
    软件管理的一致性是处理数据共享的一种更常见的方法。一般，设备驱动程序必须清除缓存中的脏数据或使旧数据失效。这增加了耗时以及软件的复杂性，并且在共享率很高的情况下会降低性能。
-   硬件管理的一致性
    硬件维护cluster内level 1数据缓存之间的一致性。核心在上电后自动参与一致性方案，启用D-cache和MMU，并将一个地址标记为相干的。然而，这种缓存一致性逻辑并不维护数据和指令缓存之间的一致性。在ARMv8-A体系结构和相关实现中，一般都使用硬件管理的一致方案。这为互连和集群增加了一些硬件复杂性，但极大地简化了软件。

缓存一致性方案可以通过多种标准方式进行操作。ARMv8处理器使用`MOESI协议`。ARMv8处理器也可以连接到AMBA 5 CHI互连，其缓存一致性协议类似(但不完全相同)MOESI。根据使用的协议，`SCU`用以下属性之一标记缓存中的每一行：M(修改的)、O(拥有的)、E(独占的)、S(共享的)或I(无效的)。其他类似协议还有MSI、MOSI、MESI等，[https://en.wikipedia.org/wiki/MOESI_protocol](https://en.wikipedia.org/wiki/MOESI_protocol)

![[mk.att/Pasted image 20240530145453.png]]

### SCU

处理器cluster包含一个Snoop Control Unit (SCU)，它包含存储在各个L1数据缓存标记的副本。

-   维护L1数据缓存之间的一致性。
-   仲裁对L2接口的访问，包括指令和数据。
-   包括tag RAM以跟踪每个核心分配的数据。

SCU是通过位于私有内存区域中的控制寄存器启用的。每个核心可以单独配置，以参与或不参与数据缓存一致性管理方案。处理器内部的SCU自动维护cluster内部level 1的缓存一致性。`SCU只能在单个cluster内保持一致性。`如果系统中有额外的处理器或其他总线master，就需要显式的软件同步。

### Accelerator coherency port

SCU拥有与AMBA 4 AXI兼容的从接口为主机提供了一个连接点，

-   该接口支持所有标准的读写事务，而不需要额外的一致性要求。但是，任何到内存的一致区域的读事务都会与SCU交互，以测试信息是否已经存储在L1缓存中。
-   SCU在执行写到系统内存的强制一致性，可能分配到L2缓存，消除直接写到片外内存的功率和性能影响。

### Cache coherency between clusters

系统还可以包含PMU硬件来维护集群之间的一致性。集群可以动态地从一致性管理中添加或删除，例如，当整个集群(包括L2缓存)关闭电源时。操作系统可以通过内置的性能监视单元(Performance Monitoring Units，pmu)监视上的活动。

### Domain

在ARMv8-A体系结构中，术语`domain`用于指一组总线上的master。domain决定snoop哪些master，以便进行一致性处理。snooping是对master的缓存进行检查，以查看请求的位置是否存储在那里。有四个定义的domain类型:

-   Non-shareable.
-   Inner Shareable.
-   Outer Shareable.
-   System.

典型的系统使用情况是，在相同操作系统下运行的master位于相同的Inner Shareable域中。不那么紧密地耦合的master在同一个Outer Shareable域中。内存访问的domain选择是通过页面表中的项来控制的。

![[mk.att/Pasted image 20240530145509.png]]

## Bus protocol and the Cache Coherent Interconnect(总线协议和cache一致性互联)

将硬件一致性扩展到多集群系统需要一个一致性的总线协议。AMBA 4 ACE规范包括AXI一致性扩展(ACE)。完整的ACE接口支持集群之间的硬件一致性，并使SMP操作系统能够在多个核上运行。如果你有多个集群，那么在一个集群中对内存的共享访问都可以窥探其他集群的缓存，以查看数据是否在那里，或者是否必须从外部内存加载数据。`AMBA 4 ACE-Lite接口是全接口的一个子集`，专为单向IO相干系统master设计，如DMA引擎、网络接口和gpu。这些设备可能没有自己的缓存，但可以从ACE处理器读取共享数据。用于non-core master的缓存通常不与core的缓存保持一致。例如，在许多系统中，core无法窥探GPU的缓存内部。但反过来是可以的。

ACE-Lite允许其他主机窥探其他集群的缓存内部。这意味着，对于可共享位置，必要时将从一致缓存中完成读取，而可共享的写将从一致缓存线中合并为强制清理并使其失效。ACE规范允许TLB和I-Cache维护操作广播到所有能够接收它们的设备。数据屏障被发送到从接口，以确保它们编程上是完整的。

CoreLink CCI-400缓存相干接口是AMBA 4 ACE的第一个实现IP，支持最多两个ACE集群，使最多8个核能够看到相同的内存视图，并运行SMP操作系统，例如，一个big.LITTLE系统，Cortex-A57处理器和Cortex-A53处理器。

### 定义cpu

内核是如何识别到芯片中有几个CPU核的呢？CPU0又是如何唤醒CPU1的呢？首先，为了描述当前系统中各个CPU核心的工作状态，内核在kernel/cpu.c中定义四个cpumask类型的结构体变量，

cpumask类型的结构体变量(位于文件kernel/cpu.c)

```c
struct cpumask __cpu_possible_mask __read_mostly;
EXPORT_SYMBOL(__cpu_possible_mask);

struct cpumask __cpu_online_mask __read_mostly;
EXPORT_SYMBOL(__cpu_online_mask);

struct cpumask __cpu_present_mask __read_mostly;
EXPORT_SYMBOL(__cpu_present_mask);

struct cpumask __cpu_active_mask __read_mostly;
EXPORT_SYMBOL(__cpu_active_mask);
```

cpumask类型的结构体只有一个成员变量——数据类型为unsigned long的一维数组，一个CPU核心用数组元素的一个位表示， 宏定义BITS_TO_LONGS(bits)负责计算数组的长度，假设当前有33个CPU（NR_CPUS=33），经过BITS_TO_LONGS转换之后，可知需要的数组长度为2个。

struct cpumask结构体（位于文件include/linux/cpumask.h）

```c
/* Don't assign or return these: may not be this big! */
typedef struct cpumask { DECLARE_BITMAP(bits, NR_CPUS); } cpumask_t;

#define DECLARE_BITMAP(name,bits) \
unsigned long name[BITS_TO_LONGS(bits)]
```

这四个cpumask类型的变量作用如下：

-   `__cpu_possible_mask`：记录物理存在且可能被激活的CPU核心对应的编号，由设备树解析CPU节点获得；

-   `__cpu_online_mask`：记录当前系统正在运行的CPU核心的编号；

-   `__cpu_present_mask`：动态地记录了当前系统中所有CPU核心的编号，如果内核配置了CONFIG_HOTPLUG_CPU，那么这些CPU核心不一定全部处于运行状态，因为有的CPU核心可能被热插拔了；

-   `__cpu_active_mask`：用于记录当前系统哪些CPU核心可用于任务调度；

在/sys/devices/system/cpu目录下，记录了系统中所有的CPU核以及上述各变量的内容，例如文件present，对应于__cpu_present_mask变量，执行以下命令，可以查看当前系统中所有的CPU核编号。

```bash
cat /sys/devices/system/cpu/present
```

此外，我们可以通过文件/sys/devices/system/cpu/cpu1/online在用户空间控制一个CPU核运行与否。

```bash
# 关闭CPU1
echo 0 > /sys/devices/system/cpu/cpu1/online
# 打开CPU1
echo 1 > /sys/devices/system/cpu/cpu1/online
```

接下来，看看内核是如何建立CPU之间的关系的。在设备树根节点下有个/cpus的子节点，其内容如下

```c
	cpus {
		#address-cells = <1>;
		#size-cells = <0>;

		cpu0: cpu@0 {
			device_type = "cpu";
			compatible = "arm,armv7";
			reg = <0x0>;
			clocks = <&cpu_pll>, <&cpu_clk>;
			#cooling-cells = <2>;
			// operating-points-v2 = <&cpu_opp_table>;
			// dynamic-power-coefficient = <311>;
			// pwms = <&pwm 0 100000 1>;
			// pinctrl-names = "default", "suspend";
			// pinctrl-0 = <&pinctrl_pwm0>;
			// pinctrl-1 = <&pinctrl_pwm0_suspend>;
		};

		cpu1: cpu@1 {
			device_type = "cpu";
			compatible = "arm,armv7";
			reg = <0x1>;
			clocks = <&cpu_pll>, <&cpu_clk>;
			#cooling-cells = <2>;
			// operating-points-v2 = <&cpu_opp_table>;
			// dynamic-power-coefficient = <311>;
			// pwms = <&pwm 0 100000 1>;
			// pinctrl-names = "default", "suspend";
			// pinctrl-0 = <&pinctrl_pwm0>;
			// pinctrl-1 = <&pinctrl_pwm0_suspend>;
		};
	};
```

该节点描述了当前硬件上存在两个CPU，分别是CPU0和CPU1，内核代码通过解析该节点，便可以获得当前系统的CPU核心个数，并且我们可以看到该节点还包含了“operating-points-v2”属性，指向了cpu0_opp_table节点， 该节点是用于配置CPU核心支持的频率。“pwms”属性用于为cpu调压。

```c
	cpu_opp_table: cpu_opp_table {
		compatible = "operating-points-v2";
		opp-shared;
		reg = <0x04020000 0x400>;
		/* mix_voltage: set lower limit voltage(mv) */
		min_voltage = <850>;
		/* fixed_voltage(mv) */
		/* fixed_voltage = <850>; */
		/* the npu share cpu voltage flag */
		/* share_volt_flag = <1>; */

		opp00 {
			opp-hz = /bits/ 64 <1000000000>;
			opp-microvolt = <800000>;
			clock-latency-ns = <300000>;
		};

		... ...
	};
```

OPP驱动会根据芯片内部的版本号，来设置CPU核心的工作电压和工作频率。这部分的内核代码，最终实现构建CPU的拓扑关系，并调用函数set_cpu_possible在possible_mask相应的CPU编号位置上置1， 表明当前系统存在这个CPU核。

解析/cpus节点（位于文件arch/arm/kernel/devtree.c）

```c
void __init arm_dt_init_cpu_maps(void)
{
        /* 省略部分代码 */

        cpus = of_find_node_by_path("/cpus");

        if (!cpus)
                return;

        /* 省略部分代码 */

        for (i = 0; i < cpuidx; i++) {
                set_cpu_possible(i, true);
                cpu_logical_map(i) = tmp_map[i];
                pr_debug("cpu logical map 0x%x\n", cpu_logical_map(i));
        }
}
```

内核已经掌握了当前系统的CPU相关信息，接下来就应该让其他CPU纳入内核的管理，开始卖力干活了。在SMP初始化之前，内核需要初始化present_mask，之后便根据present_mask中的内容来打开对应的CPU了。 具体实现方式是将possible_mask的值复制到present_mask中。

初始化present_mask（位于文件arch/arm/kernel/smp.c）

```c
void __init smp_prepare_cpus(unsigned int max_cpus)
{
        unsigned int ncores = num_possible_cpus();

        if (ncores > 1 && max_cpus) {
                init_cpu_present(cpu_possible_mask);
        }
}

void init_cpu_present(const struct cpumask *src)
{
        cpumask_copy(&__cpu_present_mask, src);
}
```

函数smp_init（位于文件kernel/smp.c）

```c
/* Called by boot processor to activate the rest. */
void __init smp_init(void)
{
        /* 省略部分代码 */
        for_each_present_cpu(cpu) {
                if (!cpu_online(cpu))
                        cpu_up(cpu);
        }
}
```

smp_init()函数会遍历present_mask中的cpu，如果cpu没有online，那么调用cpu_up()函数。该函数是SMP启动过程最关键的一环。SMP系统在启动的过程中，即刚上电时，只能用一个CPU来执行内核初始化， 这个CPU称为“引导处理器”，即BP，其余的处理器处于暂停状态，称为“应用处理器”，即AP。代码的注释中列出了BP和AP之间初始化过程的大致状态，左侧是CPU上电过程，需要经历OFFLINE->BRINGUP_CPU->AP_OFFLINE-> AP_ONLNE->AP_ACTIVE的过程。

CPU状态值枚举（位于文件include/linux/cpuhotplug.h）

```c
/*
* CPU-up                                        CPU-down
*
* BP            AP                              BP              AP
*
* OFFLINE                                       OFFLINE
*   |                                             ^
*   v                                             |
* BRINGUP_CPU->AP_OFFLINE       BRINGUP_CPU  <- AP_IDLE_DEAD (idle thread/play_dead)
*                       |                                                       AP_OFFLINE
*                       v (IRQ-off)      ,---------------^
*                   AP_ONLNE             | (stop_machine)
*                       |                        TEARDOWN_CPU <-        AP_ONLINE_IDLE
*                       |                           ^
*                       v                               |
*            AP_ACTIVE       AP_ACTIVE
*/

enum cpuhp_state {
        CPUHP_INVALID = -1,
        CPUHP_OFFLINE = 0,
        /* 省略部分代码 */
        CPUHP_AP_ONLINE_DYN_END         = CPUHP_AP_ONLINE_DYN + 30,
        CPUHP_AP_X86_HPET_ONLINE,
        CPUHP_AP_X86_KVM_CLK_ONLINE,
        CPUHP_AP_ACTIVE,
        CPUHP_ONLINE,
};
```

内核在kernel/cpu.c里提供了一个cpuhp_step类型的数组：cpuhp_hp_states，在数组中内置了一些初始化的回调函数，这些回调函数对应初始化过程中的各个状态。

cpuhp_hp_states数组（位于文件kernel/cpu.c）

```c
static struct cpuhp_step cpuhp_hp_states[] = {
        [CPUHP_OFFLINE] = {
                .name                   = "offline",
                .startup.single         = NULL,
                .teardown.single        = NULL,
        },
#ifdef CONFIG_SMP

        [CPUHP_BRINGUP_CPU] = {
                .name                   = "cpu:bringup",
                .startup.single         = bringup_cpu,
                .teardown.single        = NULL,
                .cant_stop              = true,
        },

        [CPUHP_ONLINE] = {
                .name                   = "online",
                .startup.single         = NULL,
                .teardown.single        = NULL,
        },
};
```

下面我们看一下OFFLINE->BRINGUP_CPU的这个过程，cpu_up函数最终会调用_cpu_up函数，传入的实参target为CPUHP_ONLINE（cpuhp_state中的最大值），第四行代码比较CPUHP_ONLINE和CPUHP_BRINGUP_CPU的大小，最终返回较小值，即CPUHP_BRINGUP_CPU。

`_cpu_up`函数（位于文件kernel/smp.c）

```c
static int _cpu_up(unsigned int cpu, int tasks_frozen, enum cpuhp_state target)
{
        /* 省略部分代码 */
        target = min((int)target, CPUHP_BRINGUP_CPU);
        ret = cpuhp_up_callbacks(cpu, st, target);
out:
        cpus_write_unlock();
        arch_smt_update();
        return ret;
}
```

cpuhp_up_callbacks函数的作用，就如函数名称一样，是用来调用cpuhp_hp_states数组中的提供的初始化函数。

`_cpu_up`函数（位于文件kernel/smp.c）

```c
static int cpuhp_up_callbacks(unsigned int cpu, struct cpuhp_cpu_state *st,
                                enum cpuhp_state target)
{
        enum cpuhp_state prev_state = st->state;
        int ret = 0;

        while (st->state < target) {
                st->state++;
                ret = cpuhp_invoke_callback(cpu, st->state, true, NULL, NULL);
                if (ret) {
                        if (can_rollback_cpu(st)) {
                                st->target = prev_state;
                                undo_cpu_up(cpu, st);
                        }
                        break;
                }
        }
        return ret;
}
```

st->state记录了当前AP核的状态，默认上电后，AP是处于CPUHP_OFFLINE的状态，因此，cpuhp_up_callbacks函数便会执行cpuhp_hp_states数组中提供的（CPUHP_OFFLINE+1）至CPUHP_BRINGUP_CPU所有阶段的回调函数， 来启动当前的AP核，经过BRINGUP_CPU的状态之后，当前的AP核就会运行空闲任务，与此同时，BP核唤醒cpuhp/0进程，完成CPUHP_AP_ONLINE_IDLE->CPUHP_ONLINE的过程，具体的实现过程：

cpuhp_thread_fun函数（位于文件kernel/smp.c）

```c
static void cpuhp_thread_fun(unsigned int cpu)
{
        if (cpuhp_is_atomic_state(state)) {
                local_irq_disable();
                st->result = cpuhp_invoke_callback(cpu, state, bringup, st->node, &st->last);
                local_irq_enable();
                WARN_ON_ONCE(st->result);
        } else {
                st->result = cpuhp_invoke_callback(cpu, state, bringup, st->node, &st->last);
        }

}
```

我们可以看到在这个进程又调用了前面的提到的函数cpuhp_invoke_callback，最终AP核达到CPUHP_ONLINE的状态，由内核进行调度，和BP核一起承担工作负载。

上述的过程只是分析了单个AP核的启动过程，假设现在系统中有多个AP核，那么内核会为每个AP核执行相同的操作，直到所有的AP核启动成功。

### SMP启动

SMP结构中的CPU都是平等的，没有主次之分，这是基于系统中有多个进程的前提下说的。

在同一时间，一个进程只能由一个CPU执行。

**系统启动**对于SMP结构来说是一个特例，因为在这个阶段系统里只有一个CPU，也就是说在刚刚上电或者复位时只有一个CPU来执行系统引导和初始化。

简易流程

![[mk.att/linux multi-core processors 2024-07-01 15.56.55.excalidraw]]

这个CPU被称为“引导处理器”，即BP，其余的处理器处于暂停状态，称为“应用处理器”，即AP。

![[mk.att/linux multi-core processors 2024-06-28 17.06.49.excalidraw]]

"引导处理器"完成整个系统的引导和初始化，并创建起多个进程，从而可以由多个处理器同时参与处理时，才启动所有的"应用处理器"，让他们完成自身的初始化以后，投入运行。

1、BP先完成自身初始化，然后从start_kernel()调用smp_init()进行SMP结构初始化。

2、smp_init()的主体是smp_boot_cpus()，依次调用do_boot_cpu()启动各个AP。

3、AP通过执行trampoline.S的一段跳板程序进入startup_32()完成一些基本初始化。

4、AP进入start_secondary()做进一步初始化工作，进入自旋（全局变量smp_commenced是否变为1），等待一个统一的“起跑”命令。

5、BP完成所有AP启动后，调用smp_commence()发出该起跑命令。

6、每个CPU进入cpu_idle()，等待调度。

3.4、SMP拓扑关系构建

系统启动时开始构建CPU拓扑关系。

```c
--> start_kernel()
	--> reset_init()
		--> kernel_init()
			--> kernel_init_freeable()
				--> sched_init_smp()
					--> init_sched_domains()
```

```bash
[    0.120008] smp: Bringing up secondary CPUs ...
[    0.200147] [platsmp] simple_boot_secondary:298 enter
[    0.200156] CPU: 0 PID: 1 Comm: swapper/0 Not tainted 4.19.73+ #10
[    0.206241] Hardware name: Simple-X (Flattened Device Tree)
[    0.212135] Backtrace: 
[    0.214572] [<c010ba04>] (dump_backtrace) from [<c010bcec>] (show_stack+0x18/0x1c)
[    0.222104]  r7:dc051540 r6:60000013 r5:00000000 r4:c0b36c8c
[    0.227740] [<c010bcd4>] (show_stack) from [<c07c9aa4>] (dump_stack+0x90/0xa4)
[    0.234933] [<c07c9a14>] (dump_stack) from [<c01156c0>] (simple_boot_secondary+0x30/0x16c)
[    0.243076]  r7:dc051540 r6:00000001 r5:00000001 r4:c0b37af0
[    0.248711] [<c0115690>] (simple_boot_secondary) from [<c010dbf0>] (__cpu_up+0x88/0x128)
[    0.256685]  r9:1c18a000 r8:00000000 r7:dc051540 r6:c09a8048 r5:00000001 r4:c0b37af0
[    0.264398] [<c010db68>] (__cpu_up) from [<c011e554>] (bringup_cpu+0x28/0xc4)
[    0.271503]  r7:c0a3620c r6:00000055 r5:00000001 r4:dc051540
[    0.277138] [<c011e52c>] (bringup_cpu) from [<c011e100>] (cpuhp_invoke_callback+0xa0/0x1c0)
[    0.285456]  r7:c0a3620c r6:00000055 r5:00000001 r4:c0b0f854
[    0.291092] [<c011e060>] (cpuhp_invoke_callback) from [<c011f524>] (_cpu_up+0xe8/0x1cc)
[    0.299064]  r8:00000000 r7:c0a3620c r6:00000055 r5:00000001 r4:dcbc020c
[    0.305737] [<c011f43c>] (_cpu_up) from [<c011f680>] (do_cpu_up+0x78/0xa0)
[    0.312584]  r10:00000000 r9:00000000 r8:c0b0a46c r7:c0b0a53c r6:c0b145a8 r5:000000c7
[    0.320381]  r4:00000001
[    0.322897] [<c011f608>] (do_cpu_up) from [<c011f6bc>] (cpu_up+0x14/0x18)
[    0.329655]  r5:c0b0a468 r4:00000001
[    0.333211] [<c011f6a8>] (cpu_up) from [<c0a0aab4>] (smp_init+0xa8/0xf8)
[    0.339886] [<c0a0aa0c>] (smp_init) from [<c0a00dec>] (kernel_init_freeable+0x90/0x204)
[    0.347857]  r8:00000000 r7:00000000 r6:00000000 r5:c0a35710 r4:c0a35710
[    0.354533] [<c0a00d5c>] (kernel_init_freeable) from [<c07dcd68>] (kernel_init+0x10/0x114)
[    0.362764]  r10:00000000 r9:00000000 r8:00000000 r7:00000000 r6:00000000 r5:c07dcd58
[    0.370561]  r4:00000000
[    0.373079] [<c07dcd58>] (kernel_init) from [<c01010f0>] (ret_from_fork+0x14/0x24)
[    0.380615] Exception stack(0xdc059fb0 to 0xdc059ff8)
[    0.385643] 9fa0:                                     00000000 00000000 00000000 00000000
[    0.393790] 9fc0: 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
[    0.401936] 9fe0: 00000000 00000000 00000000 00000000 00000013 00000000
[    0.408522]  r5:c07dcd58 r4:00000000
[    0.412125] [platsmp] simple_secondary_init:256 enter
[    0.412132] CPU: 1 PID: 0 Comm: swapper/1 Not tainted 4.19.73+ #10
[    0.412134] Hardware name: Simple-X (Flattened Device Tree)
[    0.412136] Backtrace: 
[    0.412144] [<c010ba04>] (dump_backtrace) from [<c010bcec>] (show_stack+0x18/0x1c)
[    0.412148]  r7:c0b37b00 r6:200000d3 r5:00000000 r4:c0b36c8c
[    0.412154] [<c010bcd4>] (show_stack) from [<c07c9aa4>] (dump_stack+0x90/0xa4)
[    0.412160] [<c07c9a14>] (dump_stack) from [<c0115620>] (simple_secondary_init+0x34/0xa4)
[    0.412164]  r7:c0b37b00 r6:c080069c r5:dc094000 r4:c0925d5c
[    0.412170] [<c01155ec>] (simple_secondary_init) from [<c010dfb0>] (secondary_start_kernel+0xf8/0x178)
[    0.412174]  r7:c0b37b00 r6:00000001 r5:dc094000 r4:c0b15020
[    0.412179] [<c010deb8>] (secondary_start_kernel) from [<40102518>] (0x40102518)
[    0.412184]  r7:c0b37b00 r6:10c0387d r5:00000051 r4:5c06806a
[    0.412188] [platsmp] simple_boot_secondary:344 end
[    0.412189] [platsmp] simple_secondary_init:280 end
[    0.412221] CPU1: thread -1, cpu 1, socket 0, mpidr 80000001
[    0.418373] smp: Brought up 1 node, 2 CPUs
```

平台定义的smp操作接口。

```c
struct smp_operations {
#ifdef CONFIG_SMP
	/*
	 * Setup the set of possible CPUs (via set_cpu_possible)
	 */
	void (*smp_init_cpus)(void);
	/*
	 * Initialize cpu_possible map, and enable coherency
	 */
	void (*smp_prepare_cpus)(unsigned int max_cpus);

	/*
	 * Perform platform specific initialisation of the specified CPU.
	 */
	void (*smp_secondary_init)(unsigned int cpu);
	/*
	 * Boot a secondary CPU, and assign it the specified idle task.
	 * This also gives us the initial stack to use for this CPU.
	 */
	int  (*smp_boot_secondary)(unsigned int cpu, struct task_struct *idle);
#ifdef CONFIG_HOTPLUG_CPU
	int  (*cpu_kill)(unsigned int cpu);
	void (*cpu_die)(unsigned int cpu);
	bool  (*cpu_can_disable)(unsigned int cpu);
	int  (*cpu_disable)(unsigned int cpu);
#endif
#endif
};
```

- smp_init_cpus： 通过set_cpu_possible()设置可用的cpu map；
- smp_prepare_cpus：准备所有可用的cpu；
- smp_secondary_init：用于ap cpu的初始化；
- smp_boot_secondary：用于启动ap cpu，并判断是否成功；

下面是cpu热插拔的部分
- cpu_kill：用于下电其他cpu；
- cpu_die：用于关闭当前cpu；
- cpu_can_disable：用于判断某个cpu释放可以下电；
- cpu_disable：用于下电当前cpu，但是通常是无用的；

### SMP调度

SMP调度机制分析

1、首先，load_balance()调用find_busiest_queue()来选出最忙的运行队列，在这个队列中具有最多的进程数。这个最忙的运行队列应该至少比当前的队列多出25％的进程数量。如果不存在具备这样条件的队列，find_busiest_queue()函数NULL，同时load_balance()也返回。如果存在，那么将返回这个最忙的运行时队列。

2、然后，load_balance()函数从这个最忙的运行时队列中选出将要进行负载平衡的优先级数组(priority array)。选取优先级数组原则是，首先考虑过期数组(expired array)，因为这个数组中的进程相对来说已经很长时间没有运行了，所以它们极有可能不在处理器缓冲中。如果过期数组(expired priority array)为空，那就只能选择活跃数组(active array)。

3、下一步，load_balance()找出具有最高优先级(最小的数字)链表，因为把高优先级的进程分发出去比分发低优先级的更重要。

4、为了能够找出一个没有运行,可以迁移并且没有被缓冲的进程,函数将分析每一个该队列中的进程。如果有一个进程符合标准，pull_task()函数将把这个进程从最忙的运行时队列迁移到目前正在运行的队列。

5、只要这个运行时队列还处于不平衡的状态,函数将重复执行3和4,直到将多余进程从最忙的队列中迁移至目前正在运行的队列。最后，系统又处于平衡状态，当前运行队列解锁。load_balance()返回。

3.6、SMP调度时机

1、scheduler_tick

2、try_to_wake_up（优先选择在唤醒的CPU上运行）。

3、exec系统调用启动一个新进程时。

3.6.1、scheduler_tick

系统的软中断触发会周期性的调度scheduler_tick函数,每个cpu都有一个时钟中断，都会被周期性的调度到scheduler_tick函数。

### SMP的优势与缺陷

优点 ：避免了结构障碍 , 其最大的特点是 所有的资源共享。

缺点：SMP 架构的系统 , 扩展能力有限 , 有瓶颈限制。

如 : 内存瓶颈限制，每个 CPU 处理器必须通过 相同的总线访问相同的内存资源，如果 CPU 数量不断增加，使用同一条总线，就会导致内存访问冲突; 这样就降低了 CPU 的性能 ;

通过实践证明，SMP 架构的系统，使用2 ~ 4个 CPU，可以达到利用率最高，如果 CPU 再多，其利用率就会降低，浪费处理器的性能 。

### SMP IRQ 亲和性

/proc/irq/IRQ#/smp_affinity和/proc/irq/IRQ#/smp_affinity_list指定了哪些CPU能够关联到一个给定的IRQ源，这两个文件包含了这些指定cpu的cpu位掩码(smp_affinity)和cpu列表(smp_affinity_list)。它不允许关闭所有CPU， 同时如果IRQ控制器不支持中断请求亲和 (IRQ affinity)，那么所有cpu的默认值将保持不变(即关联到所有CPU).

/proc/irq/default_smp_affinity指明了适用于所有非激活IRQ的默认亲和性掩码。一旦IRQ被分配/激活，它的亲和位掩码将被设置为默认掩码。然后可以如上所述改变它。默认掩码是0xffffffff。

下面是一个先将IRQ44(eth1)限制在CPU0-3上，然后限制在CPU4-7上的例子(这是一个8CPU的SMP box)

```bash
[root@moon 44]# cd /proc/irq/44
[root@moon 44]# cat smp_affinity
ffffffff

[root@moon 44]# echo 0f > smp_affinity
[root@moon 44]# cat smp_affinity
0000000f
[root@moon 44]# ping -f h
PING hell (195.4.7.3): 56 data bytes
...
--- hell ping statistics ---
6029 packets transmitted, 6027 packets received, 0% packet loss
round-trip min/avg/max = 0.1/0.1/0.4 ms
[root@moon 44]# cat /proc/interrupts | grep 'CPU\|44:'
        CPU0       CPU1       CPU2       CPU3      CPU4       CPU5        CPU6       CPU7
44:       1068       1785       1785       1783         0          0           0         0    IO-APIC-level  eth1
```

从上面一行可以看出，IRQ44只传递给前四个处理器（0-3）。 现在让我们把这个IRQ限制在CPU(4-7)。

```bash
[root@moon 44]# echo f0 > smp_affinity
[root@moon 44]# cat smp_affinity
000000f0
[root@moon 44]# ping -f h
PING hell (195.4.7.3): 56 data bytes
..
--- hell ping statistics ---
2779 packets transmitted, 2777 packets received, 0% packet loss
round-trip min/avg/max = 0.1/0.5/585.4 ms
[root@moon 44]# cat /proc/interrupts |  'CPU\|44:'
        CPU0       CPU1       CPU2       CPU3      CPU4       CPU5        CPU6       CPU7
44:       1068       1785       1785       1783      1784       1069        1070       1069   IO-APIC-level  eth1
```

这次IRQ44只传递给最后四个处理器。 即CPU0-3的计数器没有变化。

下面是一个将相同的irq(44)限制在cpus 1024到1031的例子

```bash
[root@moon 44]# echo 1024-1031 > smp_affinity_list
[root@moon 44]# cat smp_affinity_list
1024-1031
```

需要注意的是，如果要用位掩码来做这件事，就需要32个为0的位掩码来追踪其相关的一个。

查看每个CPU核心数

```bash
# cat /proc/cpuinfo 
processor	: 0
model name	: ARMv7 Processor rev 5 (v7l)
BogoMIPS	: 48.00
Features	: half thumb fastmult vfp edsp thumbee neon vfpv3 tls vfpv4 idiva idivt vfpd32 lpae 
CPU implementer	: 0x41
CPU architecture: 7
CPU variant	: 0x0
CPU part	: 0xc07
CPU revision	: 5

Hardware	: Simple-X (Flattened Device Tree)
Revision	: 0000
Serial		: 0000000000000000

```

查看每个CPU核心数

`cat /proc/cpuinfo |grep "cores"`

### lscpu

-   Architecture：表示处理器所使用的架构，常见的有x86、MIPS、PowerPC、ARM等等，对于MP157来说，它属于ARMv7架构；

-   Byte Order：表示处理器是大端模式还是小端模式；

-   CPU(s)：表示处理器拥有多少个核心，前面说过157A系列是有两个A7核心，这里对应了的值为2，其编号分别对应0和1；

-   On-line CPU(s) list：当前正常运行的CPU编号，可以看到，当前系统中两个A7核心都处于正常运行的状态；

-   Socket(s)：插槽，可以理解为当前板子上有多少个MP157芯片；

-   Core(s) per socket：芯片厂商会把多个Core集成在一个Socket上，这里表示每块157芯片上面有2个核；

-   Thread(s) per core：进程是程序的运行实例，它依赖于各个线程的分工合作。为此，英特尔研发了超线程技术，通过此技术，英特尔实现在一个实体CPU中，提供两个逻辑线程，让单个处理器就也能使用线程级的并行计算。

-   Vendor ID：芯片厂商ID，比如GenuineIntel、ARM、AMD等；

-   Model name：CPU的型号名称，这里对应的是Cortex-A7；

-   Stepping：用来标识一系列CPU的设计或生产制造版本数据，步进的版本会随着这一系列CPU生产工艺的改进、BUG的解决或特性的增加而改变；

-   CPU min MHz，CPU max MHz：CPU所支持的最小、最大的频率，系统运行过程会根据实际情况，来调整CPU的工作频率，但不会超过最大支持频率；

-   BogoMIPS：MIPS是millions of instructions per second(百万条指令每秒)的缩写，该值是通过函数计算出来的，只能用来粗略计算处理器的性能，并不是十分精确。

-   Flags：用来表示 CPU 特征的参数。比如参数thumb、thumbee表示MP157支持Thumb和Thumb-2EE这两种指令模式；vfp表示支持浮点运算。

### 实验

#### 进程迁移实验

**目的**：理解Linux如何通过进程迁移来平衡不同处理器之间的负载。

**步骤**：

1. **设置环境**：在一个SMP系统上运行Linux，并准备一些可以并发执行的程序或脚本。
2. **使用工具**：利用`top`、`htop`或`mpstat`等工具观察系统的CPU使用情况。
3. **创建进程**：启动多个进程，使它们在不同的CPU上运行。
4. **迁移进程**：使用Linux提供的API（如`sched_setaffinity`）或工具（如`taskset`）来迁移进程，从一个CPU到另一个CPU。
5. **观察结果**：观察迁移过程中CPU使用情况的变化，以及进程执行效率的变化。

```
#修改pid的cpu affinity mask
taskset -p mask pid
#查看pid的cpu affinity mask
taskset -p pid
```

```bash
# ps | grep app
 1243 root      2:50 ./app_arm
 1265 root      0:00 grep app
# taskset -p 1243
pid 1243's current affinity mask: 3
# mpstat -uA
Linux 4.19.73+ (product)	01/02/70	_armv7l_	(2 CPU)

01:50:37     CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle
01:50:37     all   29.88    0.00    9.32    0.02    0.00    0.21    0.00    0.00   60.58
01:50:37       0   56.36    0.00    7.60    0.02    0.00    0.41    0.00    0.00   35.60
01:50:37       1    3.41    0.00   11.03    0.01    0.00    0.00    0.00    0.00   85.55

01:50:37     CPU    intr/s
01:50:37     all    562.30
01:50:37       0    443.05
01:50:37       1    119.19
# mpstat -A
Linux 4.19.73+ (product)	01/02/70	_armv7l_	(2 CPU)

01:52:34     CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle
01:52:34     all   37.82    0.00    5.59    0.01    0.00    0.26    0.00    0.00   56.32
01:52:34       0   73.60    0.00    4.55    0.01    0.00    0.53    0.00    0.00   21.31
01:52:34       1    2.05    0.00    6.63    0.01    0.00    0.00    0.00    0.00   91.31

01:52:34     CPU    intr/s
01:52:34     all    414.20
01:52:34       0    337.46
01:52:34       1     76.72
# taskset -p 0x2 1243
pid 1243's current affinity mask: 3
pid 1243's new affinity mask: 2
# pidstat  -p 1243
Linux 4.19.73+ (product) 	01/02/70 	_armv7l_	(2 CPU)

02:03:15      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:03:15        0      1243   90.86    0.21    0.00    0.00   91.07     1  app_arm
# taskset   -p 0x1 1243
pid 1243's current affinity mask: 2
pid 1243's new affinity mask: 1
# 
# pidstat  -p 1243
Linux 4.19.73+ (product) 	01/02/70 	_armv7l_	(2 CPU)

02:06:54      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:06:54        0      1243   92.59    0.17    0.00    0.00   92.76     0  app_arm

```

可见，修改进程的affinity mask后进程在CPU-0和CPU-1之间迁移，并且相应的负载情况也被迁移到新的CPU上。

#### 调度器策略

**目的**：了解Linux调度器如何在SMP系统上选择进程进行执行。

**步骤**：

1. **设置环境**：同上，准备SMP系统和并发执行的程序。
2. **修改调度策略**：通过修改内核参数或使用特定的调度器（如CFS、BFS等），改变Linux的调度策略。
3. **运行实验**：启动多个进程，并观察它们在不同调度策略下的执行情况。
4. **分析数据**：使用`pidstat`、`strace`等工具收集数据，分析不同调度策略对进程执行效率、系统响应时间等方面的影响。

```bash
# chrt -h
chrt: invalid option -- 'h'
BusyBox v1.31.1 (2024-07-05 16:18:38 CST) multi-call binary.

Usage: chrt -m | -p [PRIO] PID | [-rfobi] PRIO PROG [ARGS]

Change scheduling priority and class for a process

        -m      Show min/max priorities
        -p      Operate on PID
        -r      Set SCHED_RR class
        -f      Set SCHED_FIFO class
        -o      Set SCHED_OTHER class
        -b      Set SCHED_BATCH class
        -i      Set SCHED_IDLE class
# chrt -p 1886
pid 1886's current scheduling policy: SCHED_OTHER
pid 1886's current scheduling priority: 0
# chrt -p 1887
pid 1887's current scheduling policy: SCHED_OTHER
pid 1887's current scheduling priority: 0
# 
# pidstat -u 1
Linux 4.19.73+ (product)  01/07/70        _armv7l_        (2 CPU)

07:07:20      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:07:21        0        80    0.00    0.98    0.00    0.00    0.98     0  kworker/0:1-events
07:07:21        0      1886  100.00    0.00    0.00    0.00  100.00     0  app0
07:07:21        0      1887   98.04    0.00    0.00    0.00   98.04     1  app1
07:07:21        0      1890    0.00    0.98    0.00    0.00    0.98     1  pidstat

... ...

07:07:27      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:07:28        0      1886  100.00    0.00    0.00    0.00  100.00     0  app0
07:07:28        0      1887   99.00    0.00    0.00    0.00   99.00     1  app1
07:07:28        0      1890    0.00    1.00    0.00    0.00    1.00     1  pidstat

07:07:28      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:07:29        0      1886  100.00    0.00    0.00    0.00  100.00     0  app0
07:07:29        0      1887   99.00    0.00    0.00    0.00   99.00     1  app1
07:07:29        0      1890    1.00    0.00    0.00    0.00    1.00     1  pidstat

07:07:29      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:07:30        0      1886  100.00    0.00    0.00    0.00  100.00     0  app0
07:07:30        0      1887   99.00    0.00    0.00    0.00   99.00     1  app1
07:07:30        0      1890    0.00    1.00    0.00    0.00    1.00     1  pidstat
^C

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0        80    0.00    0.10    0.00    0.00    0.10     -  kworker/0:1-events
Average:        0      1886   99.90    0.00    0.00    0.00   99.90     -  app0
Average:        0      1887   99.00    0.00    0.00    0.00   99.00     -  app1
Average:        0      1890    0.20    0.60    0.00    0.00    0.80     -  pidstat
#
```

可见，默认运行的进程将使用SCHED_OTHER调度类，并且优先级为0。在该情况下，进程使用时间片轮转方式调度，当系统空闲时当前进程的时间片为无限长。

##### 2x **SCHED_FIFO** + 1x **SCHED_OTHER**

```bash
# chrt -p 1908
pid 1908's current scheduling policy: SCHED_OTHER
pid 1908's current scheduling priority: 0
#
# chrt -f -p 1 1886
pid 1886's current scheduling policy: SCHED_OTHER
pid 1886's current scheduling priority: 0
pid 1886's new scheduling policy: SCHED_FIFO
pid 1886's new scheduling priority: 1
# chrt -f -p 2 1887
pid 1887's current scheduling policy: SCHED_OTHER
pid 1887's current scheduling priority: 0
pid 1887's new scheduling policy: SCHED_FIFO
pid 1887's new scheduling priority: 2
# [90668.098256] sched: RT throttling activated
# pidstat -u 1
Linux 4.19.73+ (product)  01/07/70        _armv7l_        (2 CPU)

07:18:59      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:19:00        0      1886   95.05    0.00    0.00    0.00   95.05     0  app0
07:19:00        0      1887   94.06    0.00    0.00    0.00   94.06     1  app1
07:19:00        0      1908    5.94    0.00    0.00    0.00    5.94     1  app
07:19:00        0      1912    0.00    0.99    0.00    0.00    0.99     0  pidstat

... ...

07:19:03      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:19:04        0      1886   95.00    0.00    0.00    0.00   95.00     0  app0
07:19:04        0      1887   96.00    0.00    0.00    0.00   96.00     1  app1
07:19:04        0      1894    1.00    0.00    0.00    0.00    1.00     0  kworker/0:0-events
07:19:04        0      1908    5.00    0.00    0.00    0.00    5.00     0  app
^C

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0      1886   95.21    0.00    0.00    0.00   95.21     -  app0
Average:        0      1887   95.21    0.00    0.00    0.00   95.21     -  app1
Average:        0      1894    0.20    0.00    0.00    0.00    0.20     -  kworker/0:0-events
Average:        0      1908    5.19    0.00    0.00    0.00    5.19     -  app
Average:        0      1912    0.20    0.60    0.00    0.00    0.80     -  pidstat

```

在启用SCHED_FIFO调度后系统响应明显变得迟缓，这是由于开启了RT throttling，这使得app0\app1以实时的方式运行，这要求更高的优先级。从而导致了app等其他用户进程处于低优先级的状态，表现为响应迟缓。

```bash
# chrt -f -p 2 1908
pid 1908's current scheduling policy: SCHED_FIFO
pid 1908's current scheduling priority: 1
pid 1908's new scheduling policy: SCHED_FIFO
pid 1908's new scheduling priority: 2
# 
# pidstat -u 1
Linux 4.19.73+ (product)  01/07/70        _armv7l_        (2 CPU)

07:24:40      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:24:41        0      1887   95.05    0.00    0.00    0.00   95.05     1  app1
07:24:41        0      1908   93.07    0.00    0.00    0.00   93.07     0  app
07:24:41        0      1918    0.99    0.00    0.00    0.00    0.99     0  pidstat

... ...

07:24:47      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:24:48        0      1887   96.00    0.00    0.00    0.00   96.00     1  app1
07:24:48        0      1908   96.00    0.00    0.00    0.00   96.00     0  app
07:24:48        0      1918    0.00    1.00    0.00    0.00    1.00     0  pidstat
^C

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0        80    0.00    0.12    0.00    0.00    0.12     -  kworker/0:1-events_freezable
Average:        0      1887   95.63    0.00    0.00    0.00   95.63     -  app1
Average:        0      1908   95.01    0.00    0.00    0.00   95.01     -  app
Average:        0      1918    0.37    0.50    0.00    0.00    0.87     -  pidstat
```

修改app进程优先级为2后，直接将app0进程（优先级为1）抢占，并且在该调度下若**同优先级进程**未主动放弃调度则其他进程无法抢占。

```bash
# chrt -f -p 1 1908
pid 1908's current scheduling policy: SCHED_RR
pid 1908's current scheduling priority: 1
pid 1908's new scheduling policy: SCHED_FIFO
pid 1908's new scheduling priority: 1
# chrt -f -p 1 1886
pid 1886's current scheduling policy: SCHED_RR
pid 1886's current scheduling priority: 1
pid 1886's new scheduling policy: SCHED_FIFO
pid 1886's new scheduling priority: 1
# chrt -f -p 1 1887
pid 1887's current scheduling policy: SCHED_RR
pid 1887's current scheduling priority: 1
pid 1887's new scheduling policy: SCHED_FIFO
pid 1887's new scheduling priority: 1
# pidstat -u 1
Linux 4.19.73+ (product)  01/07/70        _armv7l_        (2 CPU)
... ...

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0        80    0.00    0.12    0.00    0.00    0.12     -  kworker/0:1+events_freezable
Average:        0      1886   95.01    0.00    0.00    0.00   95.01     -  app0
Average:        0      1908   95.51    0.00    0.00    0.00   95.51     -  app
Average:        0      1972    0.12    0.75    0.00    0.00    0.87     -  pidstat
#
```

全部设置为SCHED_FIFO并且优先级为1，则只有app\app0被执行，并且app1无法抢占。

##### 2x **SCHED_RR** + 1x **SCHED_OTHER**

```bash
# chrt -p 1908
pid 1908's current scheduling policy: SCHED_OTHER
pid 1908's current scheduling priority: 0
# 
# chrt -r -p 1 1886
pid 1886's current scheduling policy: SCHED_IDLE
pid 1886's current scheduling priority: 0
pid 1886's new scheduling policy: SCHED_RR
pid 1886's new scheduling priority: 1
# 
# chrt -r -p 1 1887
pid 1887's current scheduling policy: SCHED_IDLE
pid 1887's current scheduling priority: 0

pid 1887's new scheduling policy: SCHED_RR
pid 1887's new scheduling priority: 1
# 
# pidstat -u 1
Linux 4.19.73+ (product)  01/07/70        _armv7l_        (2 CPU)

08:51:53      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
08:51:54        0      1886   94.06    0.00    0.00    0.00   94.06     0  app0
08:51:54        0      1887   94.06    0.00    0.00    0.00   94.06     1  app1
08:51:54        0      1908    5.94    0.00    0.00    0.00    5.94     0  app
08:51:54        0      1963    0.00    0.99    0.00    0.00    0.99     1  pidstat

... ...

08:51:57      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
08:51:58        0      1886   95.00    0.00    0.00    0.00   95.00     0  app0
08:51:58        0      1887   95.00    0.00    0.00    0.00   95.00     1  app1
08:51:58        0      1908    5.00    0.00    0.00    0.00    5.00     0  app
08:51:58        0      1963    0.00    1.00    0.00    0.00    1.00     1  pidstat
^C

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0      1886   95.01    0.00    0.00    0.00   95.01     -  app0
Average:        0      1887   95.21    0.00    0.00    0.00   95.21     -  app1
Average:        0      1908    5.19    0.00    0.00    0.00    5.19     -  app
Average:        0      1963    0.20    0.80    0.00    0.00    1.00     -  pidstat
#
# chrt -r -p 1 1908
pid 1908's current scheduling policy: SCHED_OTHER
pid 1908's current scheduling priority: 0
pid 1908's new scheduling policy: SCHED_RR
pid 1908's new scheduling priority: 1
# pidstat -u 1
Linux 4.19.73+ (product)  01/07/70        _armv7l_        (2 CPU)
... ...

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0      1886   95.51    0.00    0.00    0.00   95.51     -  app0
Average:        0      1887   46.59    0.00    0.00    0.00   46.59     -  app1
Average:        0      1908   48.09    0.00    0.00    0.00   48.09     -  app
Average:        0      1968    0.33    0.67    0.00    0.00    1.00     -  pidstat
#
```

**SCHED_RR**也是实时调度策略． RR 是轮询的缩写，与 SCHED_FIFO 不同的是．它具有时间片。

##### 2x **SCHED_BATCH** + 1x **SCHED_OTHER**

```bash
# chrt -o -p 0 1908
pid 1908's current scheduling policy: SCHED_FIFO
pid 1908's current scheduling priority: 2
pid 1908's new scheduling policy: SCHED_OTHER
pid 1908's new scheduling priority: 0
# 
# chrt -b -p 0 1887
pid 1887's current scheduling policy: SCHED_FIFO
pid 1887's current scheduling priority: 2
pid 1887's new scheduling policy: SCHED_BATCH
pid 1887's new scheduling priority: 0
# chrt -b -p 0 1886
pid 1886's current scheduling policy: SCHED_FIFO
pid 1886's current scheduling priority: 1
pid 1886's new scheduling policy: SCHED_BATCH
pid 1886's new scheduling priority: 0
#
# pidstat -u 1
Linux 4.19.73+ (product)  01/07/70        _armv7l_        (2 CPU)

07:30:59      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:31:00        0      1886   73.53    0.00    0.00    0.00   73.53     0  app0
07:31:00        0      1887   68.63    0.00    0.00    0.00   68.63     0  app1
07:31:00        0      1908   54.90    0.00    0.00    0.00   54.90     1  app
07:31:00        0      1924    0.98    0.00    0.00    0.00    0.98     1  pidstat

... ...

07:31:05      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:31:06        0      1886   62.00    0.00    0.00    0.00   62.00     1  app0
07:31:06        0      1887   65.00    0.00    0.00    0.00   65.00     0  app1
07:31:06        0      1908   72.00    0.00    0.00    0.00   72.00     0  app
07:31:06        0      1924    0.00    1.00    0.00    0.00    1.00     1  pidstat
^C

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0      1886   65.95    0.00    0.00    0.00   65.95     -  app0
Average:        0      1887   66.95    0.00    0.00    0.00   66.95     -  app1
Average:        0      1908   65.95    0.00    0.00    0.00   65.95     -  app
Average:        0      1924    0.28    0.57    0.00    0.00    0.85     -  pidstat
#
```

SCHED_BATCH指定这个调度策略的进程不是会话型，不会根据休眠时间更改优先级。
在 Linux 2.6.23 导入的 CFS 中，对进行补丁处理的进程改变了处理的方法，优先级不会因休眠时间而发生变化。在导入 CFS 的 RHEL6 中． SCHEO_BATCH 和 SCHEO_OTHER 几乎没有区别．因此可以不使用。

从上面示例也可见该调度类型控制的进程几乎与SCHEO_OTHER调度类一致。

##### 2x **SCHED_IDLE** + 1x **SCHEO_OTHER**

```bash
# chrt -i -p 0 1886
pid 1886's current scheduling policy: SCHED_BATCH
pid 1886's current scheduling priority: 0
pid 1886's new scheduling policy: SCHED_IDLE
pid 1886's new scheduling priority: 0
# chrt -i -p 0 1887
pid 1887's current scheduling policy: SCHED_BATCH
pid 1887's current scheduling priority: 0
pid 1887's new scheduling policy: SCHED_IDLE
pid 1887's new scheduling priority: 0
# pidstat -u 1
Linux 4.19.73+ (product)  01/07/70        _armv7l_        (2 CPU)

07:48:22      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:48:23        0      1886   50.50    0.00    0.00    0.00   50.50     1  app0
07:48:23        0      1887   49.50    0.00    0.00    0.00   49.50     1  app1
07:48:23        0      1908  100.99    0.00    0.00    0.00  100.99     0  app
07:48:23        0      1934    0.99    0.00    0.00    0.00    0.99     1  pidstat

... ...

07:48:27      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
07:48:28        0        80    0.00    1.00    0.00    0.00    1.00     0  kworker/0:1-events_freezable
07:48:28        0      1886   50.00    0.00    0.00    0.00   50.00     0  app0
07:48:28        0      1887   49.00    0.00    0.00    0.00   49.00     0  app1
07:48:28        0      1908   99.00    0.00    0.00    0.00   99.00     1  app
^C

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0        80    0.00    0.17    0.00    0.00    0.17     -  kworker/0:1-events_freezable
Average:        0      1886   50.08    0.00    0.00    0.00   50.08     -  app0
Average:        0      1887   49.42    0.00    0.00    0.00   49.42     -  app1
Average:        0      1908   99.67    0.00    0.00    0.00   99.67     -  app
Average:        0      1934    0.33    0.50    0.00    0.00    0.83     -  pidstat
#
```

可见由SCHED_IDLE调度的进程在系统中的优先级最低，仅在cpu空闲时获得执行权。
app进程独占一个CPU 100%，而app0、app1只有在另一个CPU空闲时被执行，由于优先级相同故使用率基本一致。

#### 负载均衡

**目的**：理解Linux如何在SMP系统上实现负载均衡。

**步骤**：

1. **设置环境**：准备CPU利用率不同的SMP系统或模拟这样的环境。
2. **启动负载**：在系统上启动不同数量的进程，模拟不同的负载情况。
3. **观察负载均衡**：使用`mpstat`、`vmstat`等工具观察系统在不同负载下的CPU使用情况，分析负载均衡的效果。

启动1个密集型进程，可见在单一密集型进程情况下，进程未在迁移。

```bash
# taskset -p 2416
pid 2416's current affinity mask: 3
# pidstat -u 1
Linux 4.19.73+ (product)  01/08/70        _armv7l_        (2 CPU)

02:08:08      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:09        0      2270    0.00    0.99    0.00    0.00    0.99     0  kworker/0:0-events_power_efficient
02:08:09        0      2401    0.99    0.00    0.00    0.00    0.99     1  kworker/1:0-events_power_efficient
02:08:09        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0
02:08:09        0      2417    0.00    0.99    0.00    0.00    0.99     0  pidstat

02:08:09      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:10        0      2416   99.00    0.00    0.00    0.00   99.00     1  app0
02:08:10        0      2417    0.00    1.00    0.00    0.00    1.00     0  pidstat

02:08:10      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:11        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0

02:08:11      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:12        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0
02:08:12        0      2417    1.00    0.00    0.00    0.00    1.00     0  pidstat

02:08:12      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:13        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0

02:08:13      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:14        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0
02:08:14        0      2417    0.00    1.00    0.00    0.00    1.00     0  pidstat

02:08:14      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:15        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0
02:08:15        0      2417    0.00    1.00    0.00    0.00    1.00     0  pidstat
^C

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0      2270    0.00    0.14    0.00    0.00    0.14     -  kworker/0:0-events_freezable
Average:        0      2401    0.14    0.00    0.00    0.00    0.14     -  kworker/1:0-events_power_efficient
Average:        0      2416   99.86    0.00    0.00    0.00   99.86     -  app0
Average:        0      2417    0.14    0.57    0.00    0.00    0.71     -  pidstat
#
```

启动两个密集型进程，可见新进程自动被分配到空闲CPU上。并且两个CPU已经都是满载状态。

```bash
# ./app1 &
# pidstat -u 1
Linux 4.19.73+ (product)  01/08/70        _armv7l_        (2 CPU)

02:08:54      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:55        0      2270    0.00    0.98    0.00    0.00    0.98     0  kworker/0:0-events_power_efficient
02:08:55        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0
02:08:55        0      2418   98.04    0.00    0.00    0.00   98.04     0  app1
02:08:55        0      2419    0.00    0.98    0.00    0.00    0.98     0  pidstat

02:08:55      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:56        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0
02:08:56        0      2418   99.00    0.00    0.00    0.00   99.00     0  app1
02:08:56        0      2419    0.00    1.00    0.00    0.00    1.00     0  pidstat

02:08:56      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:57        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0
02:08:57        0      2418   99.00    0.00    0.00    0.00   99.00     0  app1
02:08:57        0      2419    0.00    1.00    0.00    0.00    1.00     0  pidstat

02:08:57      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:58        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0
02:08:58        0      2418   99.00    0.00    0.00    0.00   99.00     0  app1

02:08:58      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:08:59        0      2416  100.00    0.00    0.00    0.00  100.00     1  app0
02:08:59        0      2418   99.00    0.00    0.00    0.00   99.00     0  app1
02:08:59        0      2419    0.00    1.00    0.00    0.00    1.00     0  pidstat

02:08:59      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:09:00        0      2416  101.00    0.00    0.00    0.00  101.00     1  app0
02:09:00        0      2418  100.00    0.00    0.00    0.00  100.00     0  app1
^C

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0      2270    0.00    0.17    0.00    0.00    0.17     -  kworker/0:0-events_power_efficient
Average:        0      2416  100.17    0.00    0.00    0.00  100.17     -  app0
Average:        0      2418   99.00    0.00    0.00    0.00   99.00     -  app1
Average:        0      2419    0.00    0.66    0.00    0.00    0.66     -  pidstat
# mpstat -P ALL 1
Linux 4.19.73+ (product)  01/07/70        _armv7l_        (2 CPU)

02:09:56     CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle
02:09:57     all  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:09:57       0  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:09:57       1  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00

02:09:57     CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle
02:09:58     all  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:09:58       0  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:09:58       1  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
... ...
```

启动3个密集型进程，可见app0、1、2自动在CPU0和CPU1之间迁移，并且所有CPU基本保持满载状态。

```bash
# ./app2 &
# pidstat -u 1
Linux 4.19.73+ (product)  01/08/70        _armv7l_        (2 CPU)

02:10:12      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:10:14        0      2270    0.00    0.98    0.00    0.00    0.98     0  kworker/0:0-events_power_efficient
02:10:14        0      2416   72.55    0.00    0.00    0.00   72.55     1  app0
02:10:14        0      2418   62.75    0.00    0.00    0.00   62.75     0  app1
02:10:14        0      2423   62.75    0.00    0.00    0.00   62.75     0  app2
02:10:14        0      2424    0.00    0.98    0.00    0.00    0.98     1  pidstat

02:10:14      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:10:15        0      2416   66.00    0.00    0.00    0.00   66.00     0  app0
02:10:15        0      2418   69.00    0.00    0.00    0.00   69.00     1  app1
02:10:15        0      2423   64.00    0.00    0.00    0.00   64.00     0  app2
02:10:15        0      2424    0.00    1.00    0.00    0.00    1.00     1  pidstat

02:10:15      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:10:16        0      2416   62.00    0.00    0.00    0.00   62.00     0  app0
02:10:16        0      2418   66.00    0.00    0.00    0.00   66.00     1  app1
02:10:16        0      2423   71.00    0.00    0.00    0.00   71.00     1  app2

02:10:16      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:10:17        0      2416   78.00    0.00    0.00    0.00   78.00     0  app0
02:10:17        0      2418   60.00    0.00    0.00    0.00   60.00     1  app1
02:10:17        0      2423   61.00    0.00    0.00    0.00   61.00     0  app2
02:10:17        0      2424    1.00    1.00    0.00    0.00    2.00     1  pidstat

02:10:17      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:10:18        0        10    1.00    0.00    0.00    0.00    1.00     1  rcu_preempt
02:10:18        0      2416   67.00    0.00    0.00    0.00   67.00     0  app0
02:10:18        0      2418   63.00    0.00    0.00    0.00   63.00     1  app1
02:10:18        0      2423   69.00    0.00    0.00    0.00   69.00     0  app2
02:10:18        0      2424    0.00    1.00    0.00    0.00    1.00     1  pidstat

02:10:18      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:10:19        0      2416   76.00    0.00    0.00    0.00   76.00     0  app0
02:10:19        0      2418   62.00    0.00    0.00    0.00   62.00     1  app1
02:10:19        0      2423   61.00    0.00    0.00    0.00   61.00     0  app2

02:10:19      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
02:10:20        0      2416   79.00    0.00    0.00    0.00   79.00     0  app0
02:10:20        0      2418   70.00    0.00    0.00    0.00   70.00     1  app1
02:10:20        0      2423   49.00    0.00    0.00    0.00   49.00     0  app2
02:10:20        0      2424    1.00    0.00    0.00    0.00    1.00     1  pidstat
^C

Average:      UID       PID    %usr %system  %guest   %wait    %CPU   CPU  Command
Average:        0        10    0.14    0.00    0.00    0.00    0.14     -  rcu_preempt
Average:        0      2270    0.00    0.14    0.00    0.00    0.14     -  kworker/0:0-events_power_efficient
Average:        0      2416   71.51    0.00    0.00    0.00   71.51     -  app0
Average:        0      2418   64.67    0.00    0.00    0.00   64.67     -  app1
Average:        0      2423   62.54    0.00    0.00    0.00   62.54     -  app2
Average:        0      2424    0.28    0.57    0.00    0.00    0.85     -  pidstat
# mpstat -P ALL 1
Linux 4.19.73+ (product)  01/08/70        _armv7l_        (2 CPU)

02:13:25     CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle
02:13:26     all  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:13:26       0  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:13:26       1  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00

02:13:26     CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle
02:13:27     all  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:13:27       0  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:13:27       1  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00

02:13:27     CPU    %usr   %nice    %sys %iowait    %irq   %soft  %steal  %guest   %idle
02:13:28     all  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:13:28       0  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
02:13:28       1  100.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00    0.00
```
