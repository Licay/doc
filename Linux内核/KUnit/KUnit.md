## 简介

KUnit（内核单元测试框架）为Linux内核中的单元测试提供了一个通用框架。使用KUnit，您可以定义称为测试套件的测试用例组。测试可以在内核引导时运行（如果是内置的），或者作为一个模块加载。 KUnit会自动在内核日志中标记并报告失败的测试用例。测试结果以KTAP（Kernel - Test Anything Protocol）格式呈现。它受JUnit、Python的unittest.mock和GoogleTest/GoogleMock（C++单元测试框架）的启发。

KUnit测试是内核的一部分，用C（编程）语言编写，测试内核实现的各个部分（例如：一个C语言函数）。除了构建时间外，从调用到完成，KUnit可以在不到10秒的时间内运行大约100个测试用例。KUnit可以测试任何内核组件，例如：文件系统、系统调用、内存管理、设备驱动程序等。

KUnit遵循白盒测试方法。测试可以访问内部系统功能。 KUnit在内核空间运行，不限于用户空间可见的东西。

此外，KUnit还有一个kunit_tool，这是一个脚本（tools/testing/kunit/kunit.py），它配置Linux内核，在QEMU或UML（User Mode Linux）下运行KUnit测试，解析测试结果并以用户友好的方式显示。
**（本文不涉及kunit_tool相关内容！）**

**注意：下文描述内容为最新linux版本(6.1)特性，其中使用“~~划线~~”标记的内容为linux-5.10未支持功能。**

**特点**

- 提供编写单元测试的框架。
- 在任何内核架构上运行测试。
- 在毫秒级别运行测试。

**先决条件**

任何Linux内核兼容的硬件。

对于要测试的内核，要使用Linux内核版本5.5或更高版本。

## [[KUnit 原理分析]]

## 单元测试

单元测试是在隔离环境中测试单个代码单元的测试。单元测试是最好的测试粒度，可以测试被测试代码中的所有可能代码路径。如果被测试代码很小，并且没有任何外部依赖项超出测试控制，如硬件，则可能实现这一点。

**编写单元测试**

编写良好的单元测试有一个简单而强大的模式：安排-执行-断言。这是一种很好的测试用例结构方式，定义了操作顺序。

1. 安排输入和目标：在测试开始时，安排允许函数工作的数据。例如：初始化语句或对象。
1. 针对目标行为：调用您的函数/被测试的代码。
1. 断言预期结果：验证结果（或结果状态）是否符合预期。

**单元测试优势**

- 增加测试速度并在长期开发中提高效率。
- 在初始阶段检测错误，因此与验收测试相比减少了错误修复成本。
- 提高代码质量。
- 鼓励编写可测试的代码。

## 如何在特定开发板运行测试

如果您不想使用KUnit包装器（例如：您希望进行测试的代码与其他系统集成，或者使用不同/不受支持的架构或配置），KUnit可以包含在任何内核中，并且结果可以手动读取和解析。

**注意**

在生产环境中不应启用`CONFIG_KUNIT`。启用KUnit会禁用内核地址空间布局随机化（KASLR），并且测试可能以不适合生产的方式影响内核状态。

### 配置内核

要启用KUnit本身，您需要启用`CONFIG_KUNIT` Kconfig选项（在menuconfig中的Kernel Hacking/Kernel Testing and Coverage下）。从那里，您可以启用任何KUnit测试。它们通常具有以`_KUNIT_TEST`结尾的配置选项。

KUnit和KUnit测试可以编译为模块。模块中的测试将在加载模块时运行。

- CONFIG_KUNIT：启用KUnit框架支持。
	- CONFIG_KUNIT_DEBUGFS：KUnit的debugfs节点支持。
	- CONFIG_KUNIT_TEST：用于测试KUnit的测试用例。
	- CONFIG_KUNIT_EXAMPLE_TEST：KUnit测试的简单示例。
	- CONFIG_KUNIT_ALL_TESTS：启用所有可启用的KUnit测试模块，包含debugfs节点支持。

### 运行测试

构建并运行您的内核。在内核日志中，测试输出以TAP格式打印出来。

如果KUnit/tests是内置的，在内核启动时将自动运行测试，位于“late_init”。

如果tests被编译为模块，则模块被加载时将自动运行测试。

**注意**

一些行或数据可能会在TAP输出中交错显示。

### debugfs

目录位于`/sys/kernel/debug/kunit/`，使用如下。

```c
## 加载测试模块
# insmod ./kunit-example-test.ko 
[263545.488992]     # Subtest: example
[263545.489013]     1..1
[263545.492874]     # example_simple_test: initializing
[263545.500181]     ok 1 - example_simple_test
[263545.500198] ok 1 - example
#
## 检查模块节点
# cd /sys/kernel/debug/kunit
# ls
example
## 检查测试套件的运行结果
# cd example/
# ls
results
# cat results 
    # Subtest: example
    1..1
    # example_simple_test: initializing

    ok 1 - example_simple_test
ok 1 - example
#
```

## 数据结构


### ~~kunit_status~~

```c
enum kunit_status
```

用例\套件的测试结果类型。

**Constants**

`KUNIT_SUCCESS`

Denotes the test suite has not failed nor been skipped.

`KUNIT_FAILURE`

Denotes the test has failed.

`KUNIT_SKIPPED`

Denotes the test has been skipped.

### kunit_case

标识一个独立的测试用例。

**Definition**:

```c
struct kunit_case {
	void (*run_case)(struct kunit *test);
	const char *name;

	/* private: internal use only. */
	bool success;
	char *log;
};
```

~~**new**~~
```c
struct kunit_case {
    void (*run_case)(struct kunit *test);
    const char *name;
    const void* (*generate_params)(const void *prev, char *desc);
    struct kunit_attributes attr;
};
```

**Members**

`run_case`

the function representing the actual test case.

`name`

the name of the test case.

~~`generate_params`~~

the generator function for parameterized tests.

~~`attr`~~

the attributes associated with the test

### kunit_suite

定义一个测试套件，描述了`struct kunit_case`的相关合集。

**Definition**:

```c
struct kunit_suite {
	const char name[256];
	int (*init)(struct kunit *test);
	void (*exit)(struct kunit *test);
	struct kunit_case *test_cases;

	/* private: internal use only */
	struct dentry *debugfs;
	char *log;
};
```

~~**new**~~
```c
struct kunit_suite {
    const char name[256];
    int (*suite_init)(struct kunit_suite *suite);
    void (*suite_exit)(struct kunit_suite *suite);
    int (*init)(struct kunit *test);
    void (*exit)(struct kunit *test);
    struct kunit_case *test_cases;
    struct kunit_attributes attr;
};
```

**Members**

`name`

the name of the test. Purely informational.

~~`suite_init`~~

called once per test suite before the test cases.

~~`suite_exit`~~

called once per test suite after all test cases.

`init`

called before every test case.

`exit`

called after every test case.

`test_cases`

a null terminated array of test cases.

~~`attr`~~

the attributes associated with the test suite

**Description**

A kunit_suite is a collection of related `struct kunit_case`, such that **init** is called before every test case and **exit** is called after every test case, similar to the notion of a _test fixture_ or a _test class_ in other unit testing frameworks like JUnit or Googletest.

Note that **exit** and **suite_exit** will run even if **init** or **suite_init** fail: make sure they can handle any inconsistent state which may result.

Every `struct kunit_case` must be associated with a kunit_suite for KUnit to run it.

### kunit

表示运行的测试实例。

**Definition**:

```c
struct kunit {
    void *priv;
};
```

**Members**

`priv`

for user to store arbitrary data. Commonly used to pass data created in the init function (see `struct kunit_suite`).

**Description**

Used to store information about the current context under which the test is running. Most of this data is private and should only be accessed indirectly via public functions; the one exception is **priv** which can be used by the test writer to store arbitrary data.

### kunit_resource

描述一个由test管理的资源。

**Definition**:

```c
struct kunit_resource {
    void *data;
    const char *name;
    kunit_resource_free_t free;
};
```

**Members**

`data`

for the user to store arbitrary data.

`name`

optional name

`free`

a user supplied function to free the resource.

**Description**

Represents a _test managed resource_, a resource which will automatically be cleaned up at the end of a test case. This cleanup is performed by the 'free' function. The `struct kunit_resource` itself is freed automatically with `kfree()` if it was allocated by KUnit (e.g., by `kunit_alloc_resource()`, but must be freed by the user otherwise.

Resources are reference counted so if a resource is retrieved via `kunit_alloc_and_get_resource()` or `kunit_find_resource()`, we need to call `kunit_put_resource()` to reduce the resource reference count when finished with it. Note that `kunit_alloc_resource()` does not require a kunit_resource_put() because it does not retrieve the resource itself.

## 接口说明

详细参考以下文件：

- include/kunit/test.h

### KUNIT_CASE

`KUNIT_CASE (test_name)`

> 辅助工具，用于创建一个`struct kunit_case`。

**Parameters**

`test_name`

a reference to a test case function.

**Description**

Takes a symbol for a function representing a test case and creates a `struct kunit_case` object from it. See the documentation for `struct kunit_case` for an example on how to use it.

### ~~KUNIT_CASE_ATTR~~

`KUNIT_CASE_ATTR (test_name, attributes)`

> 辅助工具，用于创建一个`struct kunit_case` ，并附带attributes参数。

**Parameters**

`test_name`

a reference to a test case function.

`attributes`

a reference to a struct kunit_attributes object containing test attributes

### ~~KUNIT_CASE_SLOW~~

`KUNIT_CASE_SLOW (test_name)`

> 辅助工具，用于创建一个`struct kunit_case` ，并标记为“慢用例”（用例运行时间超过`KUNIT_SPEED_SLOW_THRESHOLD_S`=1s）。

**Parameters**

`test_name`

a reference to a test case function.

### ~~KUNIT_CASE_PARAM~~

`KUNIT_CASE_PARAM (test_name, gen_params)`

> 辅助工具，用于创建一个参数化`struct kunit_case` ，并附带gen_params参数。

**Parameters**

`test_name`

a reference to a test case function.

`gen_params`

a reference to a parameter generator function.

**Description**

The generator function:

`const void* gen_params(const void *prev, char *desc)`

is used to lazily generate a series of arbitrarily typed values that fit into a void*. The argument **prev** is the previously returned value, which should be used to derive the next value; **prev** is set to NULL on the initial generator call. When no more values are available, the generator must return NULL. Optionally write a string into **desc** (size of KUNIT_PARAM_DESC_SIZE) describing the parameter.

### ~~KUNIT_CASE_PARAM_ATTR~~

`KUNIT_CASE_PARAM_ATTR (test_name, gen_params, attributes)`

> 辅助工具，用于创建一个参数化`struct kunit_case` ，并附带gen_params、attributes参数。

**Parameters**

`test_name`

a reference to a test case function.

`gen_params`

a reference to a parameter generator function.

`attributes`

a reference to a struct kunit_attributes object containing test attributes

### 内存接口

```c
void *kunit_kmalloc_array(struct kunit *test, size_t n, size_t size, gfp_t gfp)
void *kunit_kmalloc(struct kunit *test, size_t size, gfp_t gfp)
void kunit_kfree(struct kunit *test, const void *ptr)
void *kunit_kzalloc(struct kunit *test, size_t size, gfp_t gfp)
void *kunit_kcalloc(struct kunit *test, size_t n, size_t size, gfp_t gfp)
```

### 打印接口

```c
kunit_log(lvl, test_or_suite, fmt, ...)
kunit_printk(lvl, test, fmt, ...)
kunit_info (test, fmt, ...)
kunit_warn (test, fmt, ...)
kunit_err (test, fmt, ...)
```

### 状态接口

```c
KUNIT_SUCCEED(test)						// do nothing
KUNIT_FAIL(test, fmt, ...)				// it is an expectation that always fails
```

### 断言接口

以下接口EXPECT/ASSERT字段可以相互替换。不同之处在于ASSERT断言失败将立即退出测试。

```c
KUNIT_EXPECT_TRUE(test, condition)
KUNIT_EXPECT_TRUE_MSG(test, condition, fmt, ...)
KUNIT_EXPECT_FALSE(test, condition)
KUNIT_EXPECT_FALSE_MSG(test, condition, fmt, ...)
KUNIT_EXPECT_EQ(test, left, right)
KUNIT_EXPECT_PTR_EQ(test, left, right)
KUNIT_EXPECT_NE(test, left, right)
KUNIT_EXPECT_PTR_NE(test, left, right)
KUNIT_EXPECT_LT(test, left, right)
KUNIT_EXPECT_LE(test, left, right)
KUNIT_EXPECT_GT(test, left, right)
KUNIT_EXPECT_GE(test, left, right)
KUNIT_EXPECT_STREQ(test, left, right)
KUNIT_EXPECT_STRNEQ(test, left, right)
KUNIT_EXPECT_NOT_ERR_OR_NULL(test, ptr)
... ...
```

如需自定义断言，可使用以下宏：

```c
#define KUNIT_UNARY_ASSERTION(test,
			      assert_type,
			      condition,
			      expected_true,
			      fmt,
			      ...)
```

### kunit_test_suites

`kunit_test_suites (__suites...)`

> 用于注册一个或多个 `struct kunit_suite` 到KUnit框架。

**Parameters**

`__suites...`

a statically allocated list of `struct kunit_suite`.

**Description**

Registers **suites** with the test framework. This is done by placing the array of `struct kunit_suite` in the .kunit_test_suites ELF section.

When builtin, KUnit tests are all run via the executor at boot, and when built as a module, they run on module load.

### ~~kunit_test_init_section_suites~~

`kunit_test_init_section_suites (__suites...)`

> 用于注册一个或多个 `struct kunit_suite` 到KUnit框架，包含init函数或init数据。

**Parameters**

`__suites...`

a statically allocated list of `struct kunit_suite`.

**Description**

This functions similar to `kunit_test_suites()` except that it compiles the list of suites during init phase.

This macro also suffixes the array and suite declarations it makes with _probe; so that modpost suppresses warnings about referencing init data for symbols named in this manner.

Also, do not mark the suite or test case structs with __initdata because they will be used after the init phase with debugfs.

**Note**

these init tests are not able to be run after boot so there is no "run" debugfs file generated for these tests.

### kunit_suite_has_succeeded

`bool kunit_suite_has_succeeded(struct kunit_suite *suite);`

> 获取测试套件的最终结果。

**Parameters**

`suite`

需要查询的测试套件指针。

**Description**

套件测试成功，返回true，否则返回false。

### kunit_alloc_resource

```c
static inline void *kunit_alloc_resource(struct kunit *test,
					 kunit_resource_init_t init,
					 kunit_resource_free_t free,
					 gfp_t internal_gfp,
					 void *context)
```

Allocates a _test managed resource_.

**Parameters**

`struct kunit *test`

The test context object.

`kunit_resource_init_t init`

a user supplied function to initialize the resource.

`kunit_resource_free_t free`

a user supplied function to free the resource (if needed).

`gfp_t internal_gfp`

gfp to use for internal allocations, if unsure, use GFP_KERNEL

`void *context`

for the user to pass in arbitrary data to the init function.

**Description**

Allocates a _test managed resource_, a resource which will automatically be cleaned up at the end of a test case. 

**Note**

KUnit needs to allocate memory for a kunit_resource object. You must specify an **internal_gfp** that is compatible with the use context of your resource.

### kunit_alloc_and_get_resource

```c
/*
 * Like kunit_alloc_resource() below, but returns the struct kunit_resource
 * object that contains the allocation. This is mostly for testing purposes.
 */
struct kunit_resource *kunit_alloc_and_get_resource(struct kunit *test,
						    kunit_resource_init_t init,
						    kunit_resource_free_t free,
						    gfp_t internal_gfp,
						    void *context);
```

### kunit_destroy_resource

```c
int kunit_destroy_resource(struct kunit *test,
			   kunit_resource_match_t match,
			   void *match_data);
```

Find a kunit_resource and destroy it.

**Parameters**

`struct kunit *test`

Test case to which the resource belongs.

`kunit_resource_match_t match`

Match function. Returns whether a given resource matches **match_data**.

`void *match_data`

Data passed into **match**.

**Return**

0 if kunit_resource is found and freed, -ENOENT if not found.

### ~~action~~

```c
KUNIT_DEFINE_ACTION_WRAPPER (wrapper, orig, arg_type)
int kunit_add_action(struct kunit *test, kunit_action_t *action, void *ctx);
int kunit_add_action_or_reset(struct kunit *test, kunit_action_t *action, void *ctx);
void kunit_remove_action(struct kunit *test, kunit_action_t *action, void *ctx);
void kunit_release_action(struct kunit *test, kunit_action_t *action, void *ctx);
```

### ~~Managed Devices~~

Functions for using KUnit-managed struct device and struct device_driver. Include 'kunit/device.h' to use these.

```c
struct device_driver *kunit_driver_create(struct kunit *test, const char *name);
struct device *kunit_device_register(struct kunit *test, const char *name);
struct device *kunit_device_register_with_driver(struct kunit *test,
												 const char *name,
												 const struct device_driver *drv);
void kunit_device_unregister(struct kunit *test, struct device *dev);
```


## 编写说明

### 编写用例

KUnit 中每个单元测试使用一个`void (*)(struct kunit *test)`类型的函数标识，该函数将在运行测试时被调用。以下是函数示例：

```c
	void example_test_success(struct kunit *test)
	{
	}

	void example_test_failure(struct kunit *test)
	{
		KUNIT_FAIL(test, "This test never passes.");
	}
```

示例中`example_test_success`测试总是成功，因为它未运行任何代码；`example_test_failure`测试总是失败，因为他调用`KUNIT_FAIL`，该接口将报告测试失败并附带一段log。

### 测试预期（Expectations）

```c
static void add_test_basic(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 1, add(1, 0));
	KUNIT_EXPECT_EQ(test, 2, add(1, 1));
}
```

使用`KUNIT_EXPECT_`接口进行断言，若断言失败，测试将报告失败，但**测试仍将运行**。
只有所有断言都成功，测试才成功。

### 测试断言（Assertions）

```c
static void kmalloc_oob_right(struct kunit *test)
{
	char *ptr;
	size_t size = 123;

	ptr = kmalloc(size, GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ptr);

	KUNIT_EXPECT_KASAN_FAIL(test, ptr[size + OOB_TAG_OFF] = 'x');
	kfree(ptr);
}
```

使用`KUNIT_ASSERT_`接口进行断言，若断言失败，测试将报告失败，并**立即退出当前测试**。
只有所有断言都成功，测试才成功。

### 测试套件

我们需要许多测试用例来覆盖所有单元的行为。通常会有许多类似的测试。为了减少这些紧密相关测试中的重复，大多数单元测试框架（包括KUnit）提供了测试套件的概念。测试套件是一组针对代码单元的测试用例，其中包含可选的init和exit函数，在~~整个测试套件和（linux-5.10未支持）~~每个测试用例之前/之后运行。

**注意**

仅当测试用例与测试套件相关联时，才会运行测试用例。

```c
static struct kunit_case example_test_cases[] = {
	KUNIT_CASE(example_test_foo),
	KUNIT_CASE(example_test_bar),
	KUNIT_CASE(example_test_baz),
	{}
};

static struct kunit_suite example_test_suite = {
	.name = "example",
	.init = example_test_init,
	.exit = example_test_exit,
	.test_cases = example_test_cases,
};
kunit_test_suite(example_test_suite);
```

在上面的示例中，测试套件`example_test_suite`~~首先会运行example_suite_init（linux-5.10未支持），然后~~运行测试用例example_test_foo、example_test_bar和example_test_baz。每个测试用例在其之前立即调用`example_test_init`，在其之后立即调用`example_test_exit`。~~最后，在所有其他操作之后，将调用`example_suite_exit`。~~`kunit_test_suite(example_test_suite)`将测试套件注册到KUnit测试框架中。

**注意**

即使init ~~或suite_init~~失败，exit ~~和suite_exit~~函数也将运行。需要确保它们可以处理由于init ~~或suite_init~~遇到错误或提前退出而导致的任何不一致状态。

`kunit_test_suite(...)`是一个宏，告诉链接器将指定的测试套件放置在一个特殊的链接器部分中，以便KUnit可以在late_init之后或测试模块加载时（如果测试被构建为模块）运行它。


### 分配内存

在可能使用 `kzalloc` 时，可以改用 `kunit_kzalloc`，因为 KUnit 将确保一旦测试完成，内存就会被释放。

这很有用，因为它允许我们使用宏从测试中提前退出，而不必担心记得调用 `kfree`。例如：`KUNIT_ASSERT_EQ`。

```c
void example_test_allocation(struct kunit *test)
{
        char *buffer = kunit_kzalloc(test, 16, GFP_KERNEL);
        /* Ensure allocation succeeded. */
        KUNIT_ASSERT_NOT_ERR_OR_NULL(test, buffer);

        KUNIT_ASSERT_STREQ(test, buffer, "");
}
```

### 自定义资源

当测试用例结束后（测试失败退出）kunit将自动调用`kunit_cleanup()`清理所有申请的资源。**因此用户使用基于kunit resource的相关接口时，可以免于显式释放已有的资源。**

以下示例定义了基于kunit resource的内存申请、释放接口。用户可参照此示例编写用于特定测试用例的资源管理接口。

```c
/* 定义资源参数结构体，用于传递申请资源所需的参数 */
struct kunit_kmalloc_params {
	size_t size;
	gfp_t gfp;
};

/* 定义资源初始化函数，用于申请并初始化资源 */
static int kunit_kmalloc_init(struct kunit_resource *res, void *context)
{
	struct kunit_kmalloc_params *params = context;

	res->data = kmalloc(params->size, params->gfp);
	if (!res->data)
		return -ENOMEM;

	// 获取资源成功则返回0，实际的资源指针保存在res->data
	return 0;
}

/* 定义资源释放函数，用于释放资源 */
static void kunit_kmalloc_free(struct kunit_resource *res)
{
	kfree(res->data);
}

/* 定义资源获取函数 */
void *kunit_kmalloc(struct kunit *test, size_t size, gfp_t gfp)
{
	// 配置资源参数
	struct kunit_kmalloc_params params = {
		.size = size,
		.gfp = gfp
	};

	// 通过kunit resource接口申请资源
	// 需要定义资源参数结构体、初始化函数、释放函数
	return kunit_alloc_resource(test,
				    kunit_kmalloc_init,
				    kunit_kmalloc_free,
				    gfp,
				    &params);
}

/* 定义资源释放函数 */
void kunit_kfree(struct kunit *test, const void *ptr)
{
	// 通过kunit resource接口释放资源
	kunit_destroy_resource(test,
				      kunit_resource_instance_match,
				      ptr);
}

```

### ~~参数化 test case~~

表驱动的测试模式非常常见，因此 KUnit 也支持它。

通过重用参数数组，我们可以将测试编写为 “参数化测试”，如下所示。

```c
// This is copy-pasted from above.
struct sha1_test_case {
        const char *str;
        const char *sha1;
};
const struct sha1_test_case cases[] = {
        {
                .str = "hello world",
                .sha1 = "2aae6c35c94fcfb415dbe95f408b9ce91ee846ed",
        },
        {
                .str = "hello world!",
                .sha1 = "430ce34d020724ed75a196dfc2ad67c77772d169",
        },
};

// Creates `sha1_gen_params()` to iterate over `cases` while using
// the struct member `str` for the case description.
KUNIT_ARRAY_PARAM_DESC(sha1, cases, str);

// Looks no different from a normal test.
static void sha1_test(struct kunit *test)
{
        // This function can just contain the body of the for-loop.
        // The former `cases[i]` is accessible under test->param_value.
        char out[40];
        struct sha1_test_case *test_param = (struct sha1_test_case *)(test->param_value);

        sha1sum(test_param->str, out);
        KUNIT_EXPECT_STREQ_MSG(test, out, test_param->sha1,
                              "sha1sum(%s)", test_param->str);
}

// Instead of KUNIT_CASE, we use KUNIT_CASE_PARAM and pass in the
// function declared by KUNIT_ARRAY_PARAM or KUNIT_ARRAY_PARAM_DESC.
static struct kunit_case sha1_test_cases[] = {
        KUNIT_CASE_PARAM(sha1_test, sha1_gen_params),
        {}
};
```

### ~~注册clean操作~~

如果您需要执行一些超出简单使用`kunit_kzalloc`之外的清理操作，您可以注册自定义的“延迟操作”，这是一个在测试退出时运行的清理函数（无论是干净地退出还是通过失败的断言）。

操作必须是简单的函数，没有返回值，带有一个上下文参数。

这些对于从全局列表中注销内容、关闭文件或其他资源或释放资源非常有用。

例如：

```c
static void cleanup_device(void *ctx)
{
        struct device *dev = (struct device *)ctx;

        device_unregister(dev);
}

void example_device_test(struct kunit *test)
{
        struct my_device dev;

        device_register(&dev);

        kunit_add_action(test, &cleanup_device, &dev);
}
```

请注意，对于像 `device_unregister` 这样只接受一个指针大小参数的函数，可以使用宏自动生成包装器，例如：`KUNIT_DEFINE_ACTION_WRAPPER()`

```c
KUNIT_DEFINE_ACTION_WRAPPER(device_unregister, device_unregister_wrapper, struct device *);
kunit_add_action(test, &device_unregister_wrapper, &dev);
```

你应该优先使用这种方法，而不是手动转换类型，因为转换函数指针会破坏控制流完整性（CFI）。

如果 `kunit_add_action` 失败，例如系统内存不足，可以使用 `kunit_add_action_or_reset` 来立即运行操作，而不是推迟。

如果需要控制清理函数何时被调用，可以使用 `kunit_release_action` 提前触发，或者使用 `kunit_remove_action` 完全取消。


### ~~访问当前test~~

在某些情况下，我们需要从测试文件外部调用仅限于测试的代码。例如，当提供函数的虚拟实现或在错误处理程序内部失败当前测试时，这非常有用。我们可以通过 kunit_test 结构体中的字段来实现这一点，然后可以使用 kunit/test-bug.h 中的`kunit_get_current_test() `函数来访问。

即使 KUnit 未启用，也可以安全地调用 `kunit_get_current_test()`。如果 KUnit 未启用，或者当前任务中没有运行测试，它将返回 NULL。这将编译成一个无操作或静态密钥检查，因此在没有运行测试时将具有可忽略的性能影响。

下面的示例使用这个特性来实现函数 "foo" 的 "fake" 实现：

```c
#include <kunit/test-bug.h> /* for kunit_get_current_test */

struct test_data {
        int foo_result;
        int want_foo_called_with;
};

static int fake_foo(int arg)
{
        struct kunit *test = kunit_get_current_test();
        struct test_data *test_data = test->priv;

        KUNIT_EXPECT_EQ(test, test_data->want_foo_called_with, arg);
        return test_data->foo_result;
}

static void example_simple_test(struct kunit *test)
{
        /* Assume priv (private, a member used to pass test data from
         * the init function) is allocated in the suite's .init */
        struct test_data *test_data = test->priv;

        test_data->foo_result = 42;
        test_data->want_foo_called_with = 1;

        /* In a real test, we'd probably pass a pointer to fake_foo somewhere
         * like an ops struct, etc. instead of calling it directly. */
        KUNIT_EXPECT_EQ(test, fake_foo(1), 42);
}
```

在这个例子中，我们使用 kunit_test 结构的 .priv 成员作为从 init 函数向测试传递数据的方式。通常，.priv 是一个指针，可以用于任何用户数据。相比静态变量，这种方法更可取，因为它避免了并发问题。

如果我们需要更灵活的方式，可以使用命名的 kunit_resource。每个测试可以拥有多个资源，每个资源都有一个字符串名称，提供与 .priv 成员相同的灵活性，同时还可以允许辅助函数创建资源而不会相互冲突。还可以为每个资源定义清理函数，以便轻松避免资源泄漏。

### ~~管理虚拟设备和驱动程序~~

在测试驱动程序或与驱动程序交互的代码时，许多函数将需要一个 `struct device` 或 `struct device_driver`。在许多情况下，为了测试任何给定函数并不需要设置真实设备，因此可以使用虚拟设备代替。

KUnit 提供了辅助函数来创建和管理这些虚拟设备，这些设备在内部是 `struct kunit_device` 类型，并附加到一个特殊的 `struct kunit_bus` 上。这些设备支持受控设备资源（devres）。

要创建一个由 KUnit 管理的 `struct device_driver`，请使用 `kunit_driver_create()` 函数，在给定的 `struct kunit_bus` 上创建一个带有指定名称的驱动程序。当相应的测试结束时，此驱动程序将自动销毁，但也可以使用 `driver_unregister()` 函数手动销毁。

要创建一个虚拟设备，请使用 `kunit_device_register()` 函数，它将使用使用 `kunit_driver_create()` 创建的新的 KUnit 管理的驱动程序创建并注册一个设备。如果要提供一个特定的、非由 KUnit 管理的驱动程序，则可以使用 `kunit_device_register_with_driver()` 函数。与受控驱动程序一样，当测试结束时，KUnit 管理的虚拟设备会自动清理，但也可以使用 `kunit_device_unregister()` 函数提前手动清理。

在设备不是平台设备的情况下，应优先使用 KUnit 设备，而不是 `platform_device_register()`。

例如：

```c
#include <kunit/device.h>

static void test_my_device(struct kunit *test)
{
        struct device *fake_device;
        const char *dev_managed_string;

        // Create a fake device.
        fake_device = kunit_device_register(test, "my_device");
        KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fake_device)

        // Pass it to functions which need a device.
        dev_managed_string = devm_kstrdup(fake_device, "Hello, World!");

        // Everything is cleaned up automatically when the test ends.
}
```
