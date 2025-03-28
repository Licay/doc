
## 运行测试用例

运行测试用例的简要流程：

```c
kunit_run_tests
	kunit_print_subtest_start
	kunit_run_case_catch_errors
		kunit_init_test
		kunit_try_catch_init
			try_catch->test = test;
			try_catch->try = kunit_try_run_case;
			try_catch->catch = kunit_catch_run_case;
		kunit_try_catch_run
			kthread_run
			wait_for_completion_timeout
			!exit_code return /* 运行无措直接退出 */
			try_catch->catch
		kunit_print_ok_not_ok
	kunit_print_subtest_end

// 测试线程
kunit_generic_run_threadfn_adapter
	try_catch->try
	complete_and_exit

// 运行回调
kunit_try_run_case
	kunit_run_case_internal
		suite->init
		test_case->run_case
	kunit_run_case_cleanup/* 可能不会被调用 */
		suite->exit
		kunit_cleanup

// 错误回调
kunit_catch_run_case
	kunit_try_catch_get_result
	kunit_run_case_cleanup
		suite->exit
		kunit_cleanup

```

![[mk.att/KUnit 原理分析 2024-02-27 14.25.36.excalidraw]]
线程运行状态说明：

- 线程执行正常：
	- 0：用例测试成功；
	- -EFAULT：测试用例失败；
- 线程执行异常：
	- -EINTR：线程未运行；
	- 其他...


## 接口功能说明

### kunit_try_run_case

完整的测试用例运行流程，不包含错误处理分支。
**该接口应在线程中调用**，否则用例中的断言接口可能会结束当前线程，导致程序错误。

其流程如下：
1. 初始化（init）并运行测试用例；
2. 运行用例退出流程（exit）并释放相关资源；

### kunit_catch_run_case

测试用例的错误处理流程。
**该接口在测试线程结束后被调用**，用于处理用例返回值及用例退出流程。

其流程如下：
1. 获取用例的运行状态；
2. 若用例出现内部错误（timeout/Unknown），则打印并退出；
3. 若用例运行完成（成功/失败），则运行用例退出流程（exit）并释放相关资源；

### kunit_generic_run_threadfn_adapter

测试线程的主函数，用于运行测试用例回调。

其流程如下：
1. 运行try_catch->try，实际为`kunit_try_run_case`；
2. 标记测试结束，并结束线程，`complete_and_exit`；

### kunit_try_catch_run

触发一次测试线程，并处理测试返回值。

其流程如下：
1. 配置测试框架上下文；
2. 触发一个线程，运行`kunit_generic_run_threadfn_adapter`；
3. 等待测试线程运行完成；
4. 判断测试用例返回值，若测试用例成功/失败，则标记为“运行完成”；若测试用例出现内部错误，则打印错误信息；
5. 运行退出流程，调用try_catch->catch，实际为`kunit_catch_run_case`；

### kunit_run_tests

运行一次指定的测试套件，并输出信息到内核log。

其流程如下：
1. 打印开始信息；
2. 遍历测试套件中的所有用例，通过KUnit框架流程运行测试用例；
	1. 初始化测试实例（struct kunit test）；
	2. 初始化测试框架上下文（struct kunit_try_catch_context context）；
	3. 调用`kunit_try_catch_run`运行测试用例；
	4. 打印测试用例状态；
3. 打印结束信息；

### kunit_init_suite

同`kunit_debugfs_create_suite()`。
在`/sys/kernel/debug/kunit/`下为测试套件创建对应的目录和节点。

### kunit_fail

用于断言接口。标记测试失败，并打印失败信息。

### kunit_abort

用于断言接口。**需在测试线程中调用**。标记测试结束，并结束线程。





