#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

/*
 *  data_manager.h —— 排行榜数据持久化模块
 *  ===========================================
 *  职责：纯 C 风格文件 IO，与游戏逻辑完全解耦。
 *  提供"新增分数 → 自动排序 → 保留 Top 5"的完整 CRUD 流程。
 *
 *  文件格式（leaderboard.txt）：
 *    每行一条记录：<分数> <日期(YYYY-MM-DD)>
 *    例如：42 2026-05-29
 */

#define LEADERBOARD_FILE "leaderboard.txt"   // 排行榜持久化文件名
#define LEADERBOARD_MAX  5                   // 只保留前 N 名

// ----- 分数记录结构体（纯 C 风格） -----
typedef struct {
    int  score;          // 玩家得分
    char date[11];       // 记录日期，格式 YYYY-MM-DD（含末尾 '\0'）
} ScoreRecord;

/*
 *  LoadLeaderboard
 *  ---------------
 *  从 leaderboard.txt 中读取所有历史记录，按分数降序排序，
 *  截断只保留 Top 5，并将结果存入传入的 records 数组。
 *
 *  参数：
 *    records  - 输出数组，调用者需保证至少有 LEADERBOARD_MAX 个元素
 *  返回值：
 *    实际读取到的有效记录数（0 ~ LEADERBOARD_MAX）
 *
 *  内部流程：
 *    ① 用 fopen(path, "r") 打开文件
 *    ② 逐行 fscanf，解析"分数 日期"格式
 *    ③ 按分数从高到低冒泡排序
 *    ④ 只保留前 LEADERBOARD_MAX 条
 *    ⑤ fclose 关闭文件
 *    ⑥ 将排序后的 Top 数据重新写回文件（覆盖写入，实现 Update）
 */
int LoadLeaderboard(ScoreRecord* records);

/*
 *  SaveScore
 *  ---------
 *  接收一个新分数，获取当前日期，作为新纪录追加写入文件，
 *  然后调用 LoadLeaderboard 的内部逻辑完成排序 + Top 5 截断。
 *
 *  参数：
 *    score - 本次游戏得分
 *
 *  内部流程：
 *    ① 获取当前系统日期（YYYY-MM-DD）
 *    ② 用 fopen(path, "a") 打开文件（追加模式）
 *    ③ fprintf 写入 "分数 日期\n"
 *    ④ fclose 关闭文件
 *    ⑤ 调用内部函数重新排序并截断文件
 */
void SaveScore(int score);

#endif // DATA_MANAGER_H
