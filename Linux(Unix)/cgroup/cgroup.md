## cgroup组成

### **cgroups功能及核心概念**

cgroups（全称：control groups）是 Linux 内核的一个功能，它可以实现限制进程或者进程组的资源（如 CPU、内存、磁盘 IO 等）。

> 在 2006 年，Google 的工程师（ Rohit Seth 和 Paul Menage 为主要发起人） 发起了这个项目，起初项目名称并不是cgroups，而被称为进程容器（process containers）。在 2007 年cgroups代码计划合入Linux 内核，但是当时在 Linux 内核中，容器（container）这个词被广泛使用，并且拥有不同的含义。为了避免命名混乱和歧义，进程容器被重名为cgroups，并在 2008 年成功合入 Linux 2.6.24 版本中。cgroups目前已经成为 systemd、Docker、Linux Containers（LXC） 等技术的基础。

**cgroups 主要提供了如下功能**：

-   资源限制：限制资源的使用量，例如我们可以通过限制某个业务的内存上限，从而保护主机其他业务的安全运行。
-   优先级控制：不同的组可以有不同的资源（ CPU 、磁盘 IO 等）使用优先级。
-   审计：计算控制组的资源使用情况。
-   控制：控制进程的挂起或恢复。

**cgroups功能的实现依赖于三个核心概念：子系统、控制组、层级树。**

-   子系统（subsystem）：是一个内核的组件，一个子系统代表一类资源调度控制器。例如内存子系统可以限制内存的使用量，CPU 子系统可以限制 CPU 的使用时间。
-   控制组（cgroup）：表示一组进程和一组带有参数的子系统的关联关系。例如，一个进程使用了 CPU 子系统来限制 CPU 的使用时间，则这个进程和 CPU 子系统的关联关系称为控制组。
-   层级树（hierarchy）：是由一系列的控制组按照树状结构排列组成的。这种排列方式可以使得控制组拥有父子关系，子控制组默认拥有父控制组的属性，也就是子控制组会继承于父控制组。比如，系统中定义了一个控制组 c1，限制了 CPU 可以使用 1 核，然后另外一个控制组 c2 想实现既限制 CPU 使用 1 核，同时限制内存使用 2G，那么 c2 就可以直接继承 c1，无须重复定义 CPU 限制。

cgroups 的三个核心概念中，子系统是最核心的概念，因为子系统是真正实现某类资源的限制的基础。

### **Subsystem(子系统)**

cgroups 中的子系统就是一个资源调度控制器(又叫 controllers)。比如 CPU 子系统可以控制 CPU 的时间分配，内存子系统可以限制内存的使用量。

**subsystem** 如下( cat /proc/cgroups)：

**blkio** 对块设备的 IO 进行限制。

**cpu** 限制 CPU 时间片的分配，与 cpuacct 挂载在同一目录。

**cpuacct** 生成 cgroup 中的任务占用 CPU 资源的报告，与 cpu 挂载在同一目录。

**cpuset** 给 cgroup 中的任务分配独立的 CPU(多处理器系统) 和内存节点。

**devices** 允许或禁止 cgroup 中的任务访问设备。

**freezer** 暂停/恢复 cgroup 中的任务。

**hugetlb** 限制使用的内存页数量。

**memory** 对 cgroup 中的任务的可用内存进行限制，并自动生成资源占用报告。

**net_cls** 使用等级识别符（classid）标记网络数据包，这让 Linux 流量控制器（tc 指令）可以识别来自特定 cgroup 任务的数据包，并进行网络限制。

**net_prio** 允许基于 cgroup 设置网络流量(netowork traffic)的优先级。

**perf_event** 允许使用 perf 工具来监控 cgroup。

**pids** 限制任务的数量。

### **Hierarchy(层级)** 

层级有一系列 cgroup 以一个树状结构排列而成，每个层级通过绑定对应的子系统进行资源控制。层级中的 cgroup 节点可以包含零个或多个子节点，子节点继承父节点挂载的子系统。一个操作系统中可以有多个层级。

## 内核配置

```
# CONFIG_BLK_CGROUP is not set
CONFIG_CGROUP_SCHED=y
CONFIG_FAIR_GROUP_SCHED=y
CONFIG_CFS_BANDWIDTH=y
CONFIG_RT_GROUP_SCHED=y
CONFIG_CGROUP_PIDS=y
# CONFIG_CGROUP_RDMA is not set
# CONFIG_CGROUP_FREEZER is not set
# CONFIG_CGROUP_DEVICE is not set
# CONFIG_CGROUP_CPUACCT is not set
# CONFIG_CGROUP_DEBUG is not set
```

- CONFIG_CFS_BANDWIDTH
	- 允许用户通过CFS调度器为运行的进程定义极限cpu使用率。未被设置极限的group将不会被限制。

## cgroup使用说明

### **blkio - BLOCK IO 资源控制**

限额类限额类是主要有两种策略，一种是基于完全公平队列调度（CFQ：Completely Fair Queuing ）的按权重分配各个 cgroup 所能占用总体资源的百分比，好处是当资源空闲时可以充分利用，但只能用于最底层节点 cgroup 的配置；另一种则是设定资源使用上限，这种限额在各个层次的 cgroup 都可以配置，但这种限制较为生硬，并且容器之间依然会出现资源的竞争。

**按比例分配块设备 IO 资源**

-   **blkio.weight**：填写 100-1000 的一个整数值，作为相对权重比率，作为通用的设备分配比。
-   **blkio.weight_device**： 针对特定设备的权重比，写入格式为device_types:node_numbers weight，空格前的参数段指定设备，weight参数与blkio.weight相同并覆盖原有的通用分配比。查看一个设备的device_types:node_numbers可以使用：ls -l /dev/DEV，看到的用逗号分隔的两个数字就是。有的文章也称之为major_number:minor_number。

**控制 IO 读写速度上限**

-   **blkio.throttle.read_bps_device**：按每秒读取块设备的数据量设定上限，格式device_types:node_numbers bytes_per_second。
-   **blkio.throttle.write_bps_device**：按每秒写入块设备的数据量设定上限，格式device_types:node_numbers bytes_per_second。 **blkio.throttle.read_iops_device**：按每秒读操作次数设定上限，格式device_types:node_numbers operations_per_second。 
- **blkio.throttle.write_iops_device**：按每秒写操作次数设定上限，格式device_types:node_numbers operations_per_second
-   针对特定操作 (read, write, sync, 或 async) 设定读写速度上限 **blkio.throttle.io_serviced**：针对特定操作按每秒操作次数设定上限，格式device_types:node_numbers operation operations_per_second 
- **blkio.throttle.io_service_bytes**：针对特定操作按每秒数据量设定上限，格式device_types:node_numbers operation bytes_per_second

**统计与监控 以下内容都是只读的状态报告，通过这些统计项更好地统计、监控进程的 io 情况。**

**blkio.reset_stats**：重置统计信息，写入一个 int 值即可。

**blkio.time**：统计 cgroup 对设备的访问时间，按格式device_types:node_numbers milliseconds读取信息即可，以下类似。

**blkio.io_serviced**：统计 cgroup 对特定设备的 IO 操作（包括 read、write、sync 及 async）次数，格式device_types:node_numbers operation number

**blkio.sectors**：统计 cgroup 对设备扇区访问次数，格式 device_types:node_numbers sector_count

**blkio.io_service_bytes**：统计 cgroup 对特定设备 IO 操作（包括 read、write、sync 及 async）的数据量，格式device_types:node_numbers operation bytes

**blkio.io_queued**：统计 cgroup 的队列中对 IO 操作（包括 read、write、sync 及 async）的请求次数，格式number operation

**blkio.io_service_time**：统计 cgroup 对特定设备的 IO 操作（包括 read、write、sync 及 async）时间 (单位为 ns)，格式device_types:node_numbers operation time

**blkio.io_merged**：统计 cgroup 将 BIOS 请求合并到 IO 操作（包括 read、write、sync 及 async）请求的次数，格式number operation

**blkio.io_wait_time**：统计 cgroup 在各设备中各类型IO 操作（包括 read、write、sync 及 async）在队列中的等待时间(单位 ns)，格式device_types:node_numbers operation time

**blkio.recursive_***：各类型的统计都有一个递归版本，Docker 中使用的都是这个版本。获取的数据与非递归版本是一样的，但是包括 cgroup 所有层级的监控数据。

### **cpu - CPU 资源控制**

CPU 资源的控制也有两种策略，一种是完全公平调度 （CFS：Completely Fair Scheduler）策略，提供了限额和按比例分配两种方式进行资源控制；另一种是实时调度（Real-Time Scheduler）策略，针对实时进程按周期分配固定的运行时间。配置时间都以微秒（µs）为单位，文件名中用us表示。

**设定CPU使用的周期和使用的时间上限**
**cpu.cfs_period_us**：设定周期时间，必须与cfs_quota_us配合使用。
**cpu.cfs_quota_us**：设定周期内最多可使用的时间。这里的配置指 task 对单个 cpu 的使用上限，若cfs_quota_us是cfs_period_us的两倍，就表示在两个核上完全使用。数值范围为 1000 - 1000,000（微秒）。
**cpu.stat**：统计信息，包含nr_periods（表示经历了几个cfs_period_us周期）、nr_throttled（表示 task 被限制的次数）及throttled_time（表示 task 被限制的总时长）。

**按权重比例设定 CPU 的分配**

-   **cpu.shares**：设定一个整数（必须大于等于 2）表示相对权重，最后除以权重总和算出相对比例，按比例分配 CPU 时间。
    例如 ：cgroup A 设置 100，cgroup B 设置 300，那么 cgroup A 中的 task 运行 25% 的 CPU 时间。对于一个 4 核 CPU 的系统来说，cgroup A 中的 task 可以 100% 占有某一个 CPU，这个比例是相对整体的一个值。

**RT 调度策略下的配置 实时调度策略与公平调度策略中的按周期分配时间的方法类似，也是在周期内分配一个固定的运行时间。**

-   **cpu.rt_period_us**：设定周期时间。
-   **cpu.rt_runtime_us**：设定周期中的运行时间。

### **cpuacct - CPU 资源报告**

这个子系统的配置是cpu子系统的补充，提供 CPU 资源用量的统计，时间单位都是纳秒。

-   **cpuacct.usage**：统计 cgroup 中所有 task 的 cpu 使用时长
-   **cpuacct.stat**：统计 cgroup 中所有 task 的用户态和内核态分别使用 cpu 的时长
-   **cpuacct.usage_percpu**：统计 cgroup 中所有 task 使用每个 cpu 的时长

### **cpuset - CPU 绑定**

为 task 分配独立 CPU 资源的子系统，参数较多，这里只选讲两个必须配置的参数，同时 Docker 中目前也只用到这两个。

-   **cpuset.cpus**：在这个文件中填写 cgroup 可使用的 CPU 编号，如0-2,16代表 0、1、2 和 16 这 4 个 CPU。
-   **cpuset.mems**：与 CPU 类似，表示 cgroup 可使用的memory node，格式同上

### **device - 限制 task 对 device 的使用**

设备黑 / 白名单过滤

-   **devices.allow**：允许名单，语法type device_types:node_numbers access type ；type有三种类型：b（块设备）、c（字符设备）、a（全部设备）；access也有三种方式：r（读）、w（写）、m（创建）。
-   **devices.deny**：禁止名单，语法格式同上。 统计报告
-   **devices.list**：报告为这个 cgroup 中的task 设定访问控制的设备

### **freezer - 暂停 / 恢复 cgroup 中的 task**

只有一个属性，表示进程的状态，把 task 放到 freezer 所在的 cgroup，再把 state 改为 FROZEN，就可以暂停进程。不允许在 cgroup 处于 FROZEN 状态时加入进程。

-   **freezer.state** ，包括如下三种状态：
	- FROZEN 停止
	- FREEZING 正在停止，这个是只读状态，不能写入这个值。
	- THAWED 恢复

### **memory - 内存资源管理**

**限额类**

-   memory.limit_bytes：强制限制最大内存使用量，单位有k、m、g三种，填-1则代表无限制。
-   memory.soft_limit_bytes：软限制，只有比强制限制设置的值小时才有意义。填写格式同上。当整体内存紧张的情况下，task 获取的内存就被限制在软限制额度之内，以保证不会有太多进程因内存挨饿。可以看到，加入了内存的资源限制并不代表没有资源竞争。
-   memory.memsw.limit_bytes：设定最大内存与 swap 区内存之和的用量限制。填写格式同上。

**报警与自动控制**

-   memory.oom_control：改参数填 0 或 1， 0表示开启，当 cgroup 中的进程使用资源超过界限时立即杀死进程，1表示不启用。默认情况下，包含 memory 子系统的 cgroup 都启用。当oom_control不启用时，实际使用内存超过界限时进程会被暂停直到有空闲的内存资源。

**统计与监控类**

-   memory.usage_bytes：报告该 cgroup 中进程使用的当前总内存用量（以字节为单位）
-   memory.max_usage_bytes：报告该 cgroup 中进程使用的最大内存用量
-   memory.failcnt：报告内存达到在 memory.limit_in_bytes设定的限制值的次数
-   memory.stat：包含大量的内存统计数据。
-   cache：页缓存，包括 tmpfs（shmem），单位为字节。
-   rss：匿名和 swap 缓存，不包括 tmpfs（shmem），单位为字节。
-   mapped_file：memory-mapped 映射的文件大小，包括 tmpfs（shmem），单位为字节
-   pgpgin：存入内存中的页数
-   pgpgout：从内存中读出的页数
-   swap：swap 用量，单位为字节
-   active_anon：在活跃的最近最少使用（least-recently-used，LRU）列表中的匿名和 swap 缓存，包括 tmpfs（shmem），单位为字节
-   inactive_anon：不活跃的 LRU 列表中的匿名和 swap 缓存，包括 tmpfs（shmem），单位为字节
-   active_file：活跃 LRU 列表中的 file-backed 内存，以字节为单位
-   inactive_file：不活跃 LRU 列表中的 file-backed 内存，以字节为单位
-   unevictable：无法再生的内存，以字节为单位
-   hierarchical_memory_limit：包含 memory cgroup 的层级的内存限制，单位为字节
-   hierarchical_memsw_limit：包含 memory cgroup 的层级的内存加 swap 限制，单位为字节

### 一般使用的挂载节点

```bash
root@cr7-ubuntu:~# mount -t cgroup
cgroup on /sys/fs/cgroup/systemd type cgroup (rw,nosuid,nodev,noexec,relatime,xattr,name=systemd)
cgroup on /sys/fs/cgroup/cpu,cpuacct type cgroup (rw,nosuid,nodev,noexec,relatime,cpu,cpuacct)
cgroup on /sys/fs/cgroup/net_cls,net_prio type cgroup (rw,nosuid,nodev,noexec,relatime,net_cls,net_prio)
cgroup on /sys/fs/cgroup/perf_event type cgroup (rw,nosuid,nodev,noexec,relatime,perf_event)
cgroup on /sys/fs/cgroup/devices type cgroup (rw,nosuid,nodev,noexec,relatime,devices)
cgroup on /sys/fs/cgroup/freezer type cgroup (rw,nosuid,nodev,noexec,relatime,freezer)
cgroup on /sys/fs/cgroup/hugetlb type cgroup (rw,nosuid,nodev,noexec,relatime,hugetlb)
cgroup on /sys/fs/cgroup/cpuset type cgroup (rw,nosuid,nodev,noexec,relatime,cpuset)
cgroup on /sys/fs/cgroup/pids type cgroup (rw,nosuid,nodev,noexec,relatime,pids)
cgroup on /sys/fs/cgroup/blkio type cgroup (rw,nosuid,nodev,noexec,relatime,blkio)
cgroup on /sys/fs/cgroup/rdma type cgroup (rw,nosuid,nodev,noexec,relatime,rdma)
cgroup on /sys/fs/cgroup/memory type cgroup (rw,nosuid,nodev,noexec,relatime,memory)
```

在小型设备中往往不会挂载相关的目录，此处是示例的挂载方法。

```bash
	umount /data/cgrp > /dev/null
	rm -rf /data/cgrp /data/cgrp
	mkdir -p /data/cgrp
	# 挂载一个和所有子系统关联的cgrp树
	mount -t cgroup cgroup /data/cgrp
	# 创建一个控制组
	mkdir /data/cgrp/cgx
	cd /data/cgrp/cgx

	# 挂载一个和cpu\pids子系统关联的cgrp树
	# mount -t cgroup -o cpu,pids cg1 /data/cgrp/cg1
```

## 示例

### 操作节点

```bash
# cd /data/cgrp/
# 创建一个控制组
# mkdir /data/cgrp/cgx
# ls
cgroup.clone_children  cpu.cfs_period_us      notify_on_release
cgroup.procs           cpu.cfs_quota_us       release_agent
cgroup.sane_behavior   cpu.shares             tasks
cgx                    cpu.stat

# ls cgx
cgroup.clone_children  cpu.shares             pids.events
cgroup.procs           cpu.stat               pids.max
cpu.cfs_period_us      notify_on_release      tasks
cpu.cfs_quota_us       pids.current
# 配置新的cgrp
cd cgx
cat cpu.cfs_period_us
# 设置grp的cpu极限 %=cpu.cfs_quota_us/(cpu.cfs_period_us * cpu_num)
echo 20000 > cpu.cfs_quota_us
# 设置grp的限制进程
echo 1765 > tasks

```

- tasks：当前控制组所包含的进程PID，一个PID只能被一个cgroup控制。

### 限制进程 cpu 使用率

使用cpu.cfs_quota_us设置pid-1765极限cpu带宽为20%。

![[mk.att/Pasted image 20240320145005.png]]

使用cpulimit设置pid-1765极限cpu带宽为20%。

```bash
./cpulimit -p 1765 -i -l 20 &
top -d 0.5
```

![[mk.att/Pasted image 20240320145548.png]]

**由实验可知，在app限制使用相同cpu的情况下，通过cgroup的方式对系统负载的影响更小！**

### **限制进程使用memory**

1.  限制进程可用的最大内存，在 /sys/fs/cgroup/memory 下创建目录test_memory：

```js
$ cd  /sys/fs/cgroup/memory
$ sudo mkdir test_memory
```

2. 下面的设置把进程的可用内存限制在最大 300M，并且不使用 swap：

```js
# 物理内存 + SWAP <= 300 MB；1024*1024*300 = 314572800
$ sudo su
$ echo 314572800 > test_memory/memory.limit_in_bytes
$ echo 0 > test_memory/memory.swappiness
```

3. 然后创建一个不断分配内存的程序，它分五次分配内存，每次申请 100M：

```js
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
​
#define CHUNK_SIZE 1024 * 1024 * 100
​
void main()
{
    char *p;
    int i;
​
    for(i = 0; i < 5; i ++)
    {
        p = malloc(sizeof(char) * CHUNK_SIZE);
        if(p == NULL)
        {
            printf("fail to malloc!");
            return ;
        }
        // memset() 函数用来将指定内存的前 n 个字节设置为特定的值
        memset(p, 0, CHUNK_SIZE);
        printf("malloc memory %d MB\n", (i + 1) * 100);
    }
}
```

4. 把上面的代码保存为 mem.c 文件，然后编译：

```js
gcc mem.c -o mem
# 执行生成的 mem 程序,发现并没有什么限制性的动作
./mem
malloc memory 100 MB
malloc memory 200 MB
malloc memory 300 MB
malloc memory 400 MB
​
# 然后加上刚才的约束执行,发现在申请超过300M 的空间的时候直接挂了
cgexec -g memory:test_memory ./mem
malloc memory 100 MB
malloc memory 200 MB
killed
​
# 使用stress 压测
cgexec -g memory:test_memory stress --vm 1 --vm-bytes 500000000 --vm-keep --verbose
stree: info: [44728] dispatching hogs: 0 cpu, 0 io, 1vm, 0 hdd
stree: dbug: [44729] allocating 500000000 bytes ...
...
...
stress: FAIL: [44728] (415) <-- worker 44729 got signal 9
​
```

5. stress 程序能够提供比较详细的信息，进程被杀掉的方式是收到了 SIGKILL(signal 9) 信号。

## 使用差异

| 工具             | 限制类型                                                                                                                                | 对系统影响 |
| ---------------- | --------------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| cpulimit         | cpu使用极限（限制小组吃蛋糕的占比，吃得快的多吃，但不能超过限制）                                                                       | 一般       |
| cpu.cfs_quota_us | 同上                                                                                                                                    | 较小       |
| cpu.shares       | 根据每个cgrp的数值分配对应的cpu使用极限占比（所有cgrp的cpu使用极限之和为cpu使用上限）（看小组积分分蛋糕，只有一个小组则可以吃全部蛋糕） | 较小       |
| nice             | 进程优先级（保证某些进程的快速响应）                                                                                                    | 较小       |

## 注意

- 使用cgroup时，避免过度限制资源，否则可能导致进程性能下降或无法正常运行；
- 删除控制组之前，确保组内所有进程已经退出，否则可能导致资源泄漏；
- 使用cgroup进行资源监控时，可以定期读取状态文件，以便及时发现和处理潜在问题；
- 使用cgroup进行优先级调整时，注意权衡各个控制组之间的资源分配，避免出现资源竞争等情况。
