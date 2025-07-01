#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <getopt.h>

// 颜色输出宏定义
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_RESET   "\x1b[0m"

// 测试目录名称
const char* TEST_DIR = "folder_creation_test";

// 清理测试目录
void cleanup(int keep) {
    if (!keep) {
        printf(COLOR_YELLOW "清理测试文件夹...\n" COLOR_RESET);
        char command[100];
        snprintf(command, sizeof(command), "rm -rf %s", TEST_DIR);
        system(command);
    } else {
        printf(COLOR_GREEN "测试文件夹保留在: %s\n" COLOR_RESET, TEST_DIR);
    }
}

int main(int argc, char* argv[]) {
    double duration = 1.0;  // 默认测试1秒
    int keep = 0;           // 默认清理测试文件夹

    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "d:k")) != -1) {
        switch (opt) {
            case 'd':
                duration = atof(optarg);
                if (duration <= 0) {
                    fprintf(stderr, "错误: 测试持续时间必须为正数\n");
                    return 1;
                }
                break;
            case 'k':
                keep = 1;
                break;
            default:
                fprintf(stderr, "用法: %s [-d 持续时间(秒)] [-k 保留测试文件夹]\n", argv[0]);
                return 1;
        }
    }

    // 创建测试目录
    if (mkdir(TEST_DIR, 0777) == -1) {
        perror("创建测试目录失败");
        return 1;
    }

    printf(COLOR_GREEN "开始测试文件夹创建速度，持续时间: %.1f秒...\n" COLOR_RESET, duration);

    // 记录开始时间
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // 计算结束时间
    struct timespec end = start;
    end.tv_sec += (time_t)duration;
    end.tv_nsec += (long)((duration - (time_t)duration) * 1e9);
    if (end.tv_nsec >= 1000000000) {
        end.tv_sec++;
        end.tv_nsec -= 1000000000;
    }

    // 初始化计数器
    int count = 0;
    char folder_name[100];

    // 测试文件夹创建速度
    clock_gettime(CLOCK_MONOTONIC, &now);
    while (now.tv_sec < end.tv_sec ||
           (now.tv_sec == end.tv_sec && now.tv_nsec < end.tv_nsec)) {

        // 构建文件夹名称
        snprintf(folder_name, sizeof(folder_name), "%s/test_folder_%d", TEST_DIR, count);

        // 创建文件夹
        if (mkdir(folder_name, 0777) == -1) {
            // 如果创建失败，可能是达到了系统限制，退出循环
            break;
        }

        count++;
        clock_gettime(CLOCK_MONOTONIC, &now);
    }

    // 计算耗时
    double elapsed = (now.tv_sec - start.tv_sec) +
                    (now.tv_nsec - start.tv_nsec) / 1e9;

    // 计算每秒创建的文件夹数
    double folders_per_second = count / elapsed;

    printf(COLOR_GREEN "测试完成!\n" COLOR_RESET);
    printf("总共创建了 %d 个文件夹\n", count);
    printf("耗时: %.4f 秒\n", elapsed);
    printf("平均创建速度: %.2f 个/秒\n", folders_per_second);

    // 清理测试文件
    cleanup(keep);

    return 0;
}