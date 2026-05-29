#include "data_manager.h"
#include <stdio.h>    // FILE*, fopen, fclose, fscanf, fprintf, snprintf
#include <stdlib.h>   // qsort 备用
#include <string.h>   // strcmp, memcpy
#include <time.h>     // time_t, time(), localtime, strftime

// =====================================================================
//  内部（static）辅助函数 —— 仅在本 .cpp 文件内可见
// =====================================================================

/*
 *  SwapRecords
 *  -----------
 *  交换两个 ScoreRecord 的值（用于冒泡排序）。
 *  参数：a, b —— 指向待交换记录的指针。
 */
static void SwapRecords(ScoreRecord* a, ScoreRecord* b) {
    ScoreRecord temp = *a;
    *a = *b;
    *b = temp;
}

/*
 *  SortAndTruncate
 *  ---------------
 *  对 records 数组进行降序排序（分数高的在前），
 *  并截断只保留前 LEADERBOARD_MAX 条。
 *
 *  参数：
 *    records - 待排序的记录数组
 *    count   - 数组中有效记录的条数
 *  返回：
 *    截断后的实际记录数（≤ LEADERBOARD_MAX）
 *
 *  算法：简单冒泡排序（数据量仅 Top 5，性能无差异）
 *    每一轮遍历，相邻比较，如果前 < 后则交换。
 *    这样大的分数会"浮"到数组前端。
 */
static int SortAndTruncate(ScoreRecord* records, int count) {
    // ---- 冒泡排序（降序：高分在前） ----
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - 1 - i; j++) {
            if (records[j].score < records[j + 1].score) {
                SwapRecords(&records[j], &records[j + 1]);
            }
        }
    }

    // ---- 截断：只保留前 LEADERBOARD_MAX 条 ----
    if (count > LEADERBOARD_MAX) {
        count = LEADERBOARD_MAX;
    }
    return count;
}

/*
 *  WriteBackToFile
 *  ---------------
 *  将排序截断后的 records 数组重新写回文件（覆盖写入模式）。
 *
 *  参数：
 *    records - 已排序并截断的记录数组
 *    count   - 有效记录数
 *
 *  关键文件指针操作说明：
 *    fopen(path, "w")    —— "w" 模式：以文本写入方式打开。
 *        - 若文件不存在，则创建新文件；
 *        - 若文件已存在，则清空原有内容（覆盖写入）。
 *        - 返回 FILE* 指针，指向该文件的流。
 *    fprintf(fp, ...)    —— 向文件流 fp 写入格式化字符串。
 *    fclose(fp)          —— 关闭文件流，释放系统资源。
 *        务必在写入完成后调用，否则数据可能不完整。
 */
static void WriteBackToFile(const ScoreRecord* records, int count) {
    /* fopen 返回值：
     *   成功 → 非 NULL 的 FILE* 指针（即文件流句柄）
     *   失败 → NULL（如磁盘满、无写权限、路径非法等）
     *   必须检查返回值，防止对 NULL 指针操作导致崩溃。
     */
    FILE* fp = fopen(LEADERBOARD_FILE, "w");
    if (fp == NULL) {
        // 文件打开失败：可能是只读文件系统或权限不足
        // 这里不崩溃，仅返回，游戏仍可继续运行
        return;
    }

    // 逐条写入：格式 "分数 日期\n"
    for (int i = 0; i < count; i++) {
        /* fprintf 返回值：
         *   成功 → 写入的字符数（≥ 0）
         *   失败 → 负数
         *   这里不检查单个写入错误，保持代码简洁；
         *   对于期末项目，循环写入 Top 5 即可。
         */
        fprintf(fp, "%d %s\n", records[i].score, records[i].date);
    }

    /* fclose 的作用：
     *   ① 将缓冲区中尚未写入磁盘的数据刷新（flush）到文件
     *   ② 释放 FILE 结构体占用的内存
     *   ③ 释放该文件在操作系统中的句柄/锁
     *   忘记 fclose 会导致：数据丢失 + 资源泄漏 + 文件被锁定
     */
    fclose(fp);
}

/*
 *  GetAllLines
 *  -----------
 *  从 leaderboard.txt 中读取所有分数记录到 records 数组，
 *  返回读取到的总条数。
 *
 *  参数：
 *    records - 输出数组，调用者需保证足够大
 *
 *  关键文件指针操作说明：
 *    fopen(path, "r")    —— "r" 模式：以只读文本方式打开。
 *        - 若文件不存在，返回 NULL。
 *        - 若文件存在，文件位置指示器指向文件开头。
 *    fscanf(fp, "%d %s", ...)
 *        —— 从文件流 fp 中按格式读取数据。
 *        - "%d" 读取整数（跳过前导空白符）
 *        - "%s" 读取字符串（以空白符分隔，读到空格/换行停止）
 *        - 返回值：成功匹配并赋值的项数。
 *          返回值 < 2 时说明该行格式不合法或到达文件末尾。
 *    feof(fp)            —— 检测是否已到达文件末尾。
 *        - 注意：feof 在尝试读取越过文件尾后才返回 true，
 *          因此放在 while 循环条件中，配合 fscanf 返回值使用。
 */
static int GetAllLines(ScoreRecord* records) {
    /* ---- 打开文件（只读模式） ----
     * 若 leaderboard.txt 不存在（首次运行），fopen 返回 NULL。
     * 这是正常情况，直接返回 0 条记录即可。
     */
    FILE* fp = fopen(LEADERBOARD_FILE, "r");
    if (fp == NULL) {
        return 0;   // 文件不存在 = 排行榜为空
    }

    int count = 0;

    /* ---- 逐行读取 ----
     * while 循环条件说明：
     *   fscanf 返回值为成功匹配的数量。
     *   当格式串为 "%d %s" 时，期望返回 2（成功读到分数和日期）。
     *   如果返回 != 2，说明该行格式错或文件已读完，终止循环。
     */
    while (fscanf(fp, "%d %s", &records[count].score,
                               records[count].date) == 2) {
        count++;
        // 安全边界：防止恶意文件或异常数据导致数组越界
        if (count >= 1000) break;
    }

    /* ---- 关闭文件 ----
     * 读取完毕后必须 fclose，释放文件句柄。
     */
    fclose(fp);
    return count;
}

// =====================================================================
//  对外接口（在 data_manager.h 中声明）
// =====================================================================

/*
 *  LoadLeaderboard
 *  ---------------
 *  完整的"读取 → 排序 → 截断 → 写回"流程。
 *
 *  流程：
 *    ① GetAllLines——从文件读出所有历史记录
 *    ② SortAndTruncate——降序排序 + 取 Top 5
 *    ③ WriteBackToFile——将 Top 5 覆盖写回文件
 *    ④ 返回实际条数
 *
 *  注：步骤③写回是为了确保下次读取时文件已经是干净的 Top 5，
 *      相当于每次读取都是一次"自动清理"。
 */
int LoadLeaderboard(ScoreRecord* records) {
    // ---- 临时大数组：容纳文件中的所有行（最多 1000 条） ----
    ScoreRecord allRecords[1000];
    int total = GetAllLines(allRecords);

    if (total == 0) {
        return 0;   // 文件为空或不存在，返回空排行榜
    }

    // ---- 排序 + 截断 ----
    int topCount = SortAndTruncate(allRecords, total);

    // ---- 将 Top 数据写回文件（保证文件和内存一致） ----
    WriteBackToFile(allRecords, topCount);

    // ---- 拷贝 Top N 到调用者提供的输出数组 ----
    for (int i = 0; i < topCount; i++) {
        records[i] = allRecords[i];
    }

    return topCount;
}

/*
 *  SaveScore
 *  ---------
 *  接收新分数，获取当前日期，追加写入文件，然后重新排序截断。
 *
 *  流程：
 *    ① 获取当前系统日期，格式 YYYY-MM-DD
 *    ② fopen(path, "a") 以追加模式打开文件
 *    ③ fprintf 写入新记录
 *    ④ fclose 关闭文件
 *    ⑤ 调用 LoadLeaderboard 内部逻辑，重新排序截断（保证文件始终是 Top 5）
 *
 *  关键文件指针操作说明：
 *    fopen(path, "a")    —— "a" 模式（append，追加）：
 *        - 若文件不存在，创建新文件（与 "w" 相同）；
 *        - 若文件存在，文件位置指示器指向文件末尾，不会覆盖已有内容；
 *        - 所有写入操作都追加在文件尾部。
 *    vs "w" 模式的区别：
 *        - "w" 会清空原有内容再写入（覆盖）
 *        - "a" 保留原有内容，新数据加在末尾（追加）
 */
void SaveScore(int score) {
    // ---- ① 获取当前日期 ----
    /* time(NULL)   —— 获取当前 Unix 时间戳（自 1970-01-01 的秒数）
     * localtime()  —— 将时间戳转换为本地时间的 struct tm
     * strftime()   —— 将 struct tm 格式化为自定义字符串
     *   %Y - 四位年份
     *   %m - 两位月份（01-12）
     *   %d - 两位日期（01-31）
     */
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    char today[11];  // YYYY-MM-DD + '\0' = 11 字节
    strftime(today, sizeof(today), "%Y-%m-%d", local);

    // ---- ② 以追加模式打开文件 ----
    FILE* fp = fopen(LEADERBOARD_FILE, "a");
    if (fp == NULL) {
        return;  // 文件无法打开（权限不足等），静默失败
    }

    // ---- ③ 写入新记录 ----
    fprintf(fp, "%d %s\n", score, today);

    // ---- ④ 关闭文件（确保数据写入磁盘） ----
    fclose(fp);

    // ---- ⑤ 重新排序并截断文件 ----
    // 因为新增了一行，文件可能超过 5 行，需要重新整理为 Top 5
    ScoreRecord allRecords[1000];
    int total = GetAllLines(allRecords);
    if (total > 0) {
        int topCount = SortAndTruncate(allRecords, total);
        WriteBackToFile(allRecords, topCount);
    }
}
