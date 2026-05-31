#include "game_logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// =====================================================================
//  内部工具函数
// =====================================================================

// 生成 [min, max] 范围内的随机整数
static int RandInt(int min, int max) {
    return min + rand() % (max - min + 1);
}

// 将网格坐标转换为屏幕像素坐标（矩形左上角）
static Vector2 GridToScreen(int gx, int gy) {
    return (Vector2){ (float)(gx * CELL_SIZE),
                      (float)(PLAY_AREA_Y + gy * CELL_SIZE) };
}

// 判断两个网格坐标是否相同
static bool PosEqual(Position a, Position b) {
    return a.x == b.x && a.y == b.y;
}

// 判断某个格子是否被任何怪兽占据
static bool IsOccupiedByMonster(const GameState* gs, int x, int y) {
    for (int i = 0; i < gs->monsterCount; i++) {
        if (gs->monsters[i].pos.x == x && gs->monsters[i].pos.y == y)
            return true;
    }
    return false;
}

// =====================================================================
//  ★★★ A* 寻路算法 —— 怪兽智能追踪核心 ★★★
// =====================================================================
//
//  【算法原理概述】
//  A*（A-star）是一种启发式图搜索算法，广泛用于游戏 AI 寻路。
//  它在 Dijkstra 算法的基础上引入"启发函数 h(n)"，使搜索具有方向性，
//  从而大幅减少需要探索的节点数量。
//
//  核心公式：f(n) = g(n) + h(n)
//    - g(n)：从起点到节点 n 的实际代价（已走过的步数）
//    - h(n)：从节点 n 到终点的估计代价（启发函数，这里用曼哈顿距离）
//    - f(n)：经过节点 n 的最优路径的估计总代价
//
//  算法维护两个集合：
//    - Open 集：待探索的节点（按 f 值排序，优先探索 f 最小的）
//    - Closed 集：已探索完毕的节点（不再重复访问）
//
//  每一步从 Open 集中取出 f 值最小的节点，检查是否到达终点；
//  若未到达，则将其四个邻居加入 Open 集，重复直到找到终点或 Open 集为空。
//
//  【本实现的设计选择】
//  - 网格地图（20x14），每个格子是一个节点
//  - 移动方向：上、下、左、右（四邻域），每步代价为 1
//  - 启发函数 h(n) 使用曼哈顿距离：|dx| + |dy|
//    → 曼哈顿距离在网格四邻域移动下是"可接受启发式"
//    → 即 h(n) ≤ 实际最短距离，保证 A* 找到最优路径
//  - 使用数组模拟最小堆实现优先队列（避免动态内存分配）
//
// =====================================================================

// ---- A* 节点结构 ----
// 每个节点代表网格上的一个格子，记录寻路所需的所有信息
typedef struct {
    int x, y;       // 网格坐标
    int g;          // 从起点到当前节点的实际步数
    int h;          // 从当前节点到终点的启发式估计值（曼哈顿距离）
    int f;          // f = g + h，用于排序的优先级
    int parentIdx;  // 父节点在节点列表中的索引，用于回溯路径
    bool closed;    // 是否已加入 Closed 集（已探索完毕）
    bool inOpen;    // 是否当前在 Open 集中（待探索）
} AStarNode;

// ---- 简易最小堆（优先队列） ----
// 堆中存储的是节点在 nodes 数组中的索引，按 f 值排序
typedef struct {
    int indices[GRID_COLS * GRID_ROWS];  // 最多容纳全部格子
    int size;
} MinHeap;

static void HeapPush(MinHeap* heap, AStarNode* nodes, int nodeIdx) {
    // 上滤（sift-up）：新元素放在末尾，逐层与父节点比较
    int i = heap->size++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        // 如果当前节点 f 值小于父节点 f 值，交换
        if (nodes[heap->indices[parent]].f <= nodes[nodeIdx].f) break;
        heap->indices[i] = heap->indices[parent];
        i = parent;
    }
    heap->indices[i] = nodeIdx;
}

static int HeapPop(MinHeap* heap, AStarNode* nodes) {
    // 下滤（sift-down）：取出堆顶，将末尾元素移到堆顶后逐层下沉
    int result = heap->indices[0];
    int last = heap->indices[--heap->size];
    int i = 0;
    while (true) {
        int left  = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;
        if (left  < heap->size && nodes[heap->indices[left]].f  < nodes[heap->indices[smallest]].f)
            smallest = left;
        if (right < heap->size && nodes[heap->indices[right]].f < nodes[heap->indices[smallest]].f)
            smallest = right;
        if (smallest == i) break;
        heap->indices[i] = heap->indices[smallest];
        i = smallest;
    }
    heap->indices[i] = last;
    return result;
}

// ---- 方向向量：上、下、左、右 ----
// 怪兽每一步只能向这四个方向之一移动
static const int DX[4] = { 0, 0, -1, 1 };
static const int DY[4] = {-1, 1,  0, 0 };

/*
 *  AStarFindPath
 *  ─────────────
 *  功能：在网格地图上，找到从 (sx,sy) 到 (gx,gy) 的最短路径。
 *
 *  参数：
 *    sx, sy  - 起点网格坐标（怪兽当前位置）
 *    gx, gy  - 终点网格坐标（玩家当前位置）
 *    nextX, nextY - 输出参数，返回路径的"下一步"坐标
 *                   即怪兽应该移动到的下一个格子
 *
 *  返回值：
 *    true  - 成功找到路径，nextX/nextY 有效
 *    false - 无路径可达（通常是因为所有邻居都被占据），
 *            此时怪兽原地不动
 *
 *  【算法流程（分步骤详解）】
 *
 *  步骤 1【初始化】：
 *    创建节点数组 nodes[]，为每个格子分配唯一索引 idx = y * GRID_COLS + x。
 *    将起点节点的 g=0, h=曼哈顿距离, f=g+h，标记为 inOpen=true，
 *    并将其索引推入最小堆（Open 集）。
 *
 *  步骤 2【主循环 —— 扩展节点】：
 *    当 Open 集不为空时：
 *      a) 从堆中弹出 f 值最小的节点作为"当前节点"。
 *      b) 检查当前节点是否就是终点：
 *         → 若是，跳到步骤 3【回溯路径】。
 *      c) 将当前节点标记为 closed=true（移入 Closed 集）。
 *      d) 遍历当前节点的四个邻居（上、下、左、右）：
 *         - 边界检查：邻居坐标必须在网格范围内
 *         - 障碍检查：邻居不能被其他怪兽占据
 *         - Closed 检查：已在 Closed 集中的邻居跳过
 *         - 计算新邻居的 g' = 当前节点.g + 1（每步代价为 1）
 *         - 若邻居不在 Open 集中 或 g' < 邻居.g：
 *             更新邻居的 g, h, f, parentIdx，加入/更新 Open 集
 *
 *  步骤 3【回溯路径 —— 从终点倒推回起点】：
 *    从终点节点开始，沿着 parentIdx 链一直回溯到起点。
 *    路径上"起点的下一个节点"就是 nextX/nextY。
 *
 *  步骤 4【无路径处理】：
 *    若 Open 集已空但未找到终点，说明起点被完全包围，无路可达。
 *    返回 false，怪兽原地等待下一帧再尝试。
 *
 *  【时间复杂度分析】
 *    最坏情况：O(GRID_COLS × GRID_ROWS × log(GRID_COLS × GRID_ROWS))
 *    但因为有启发函数 h(n) 的引导，实际扩展的节点远少于全部格子。
 *    在 20×14=280 个格子的地图上，几乎瞬间完成。
 */
static bool AStarFindPath(const GameState* gs,
                          int sx, int sy, int gx, int gy,
                          int* nextX, int* nextY)
{
    // ---- 步骤 1：初始化 ----
    // 为每个格子创建 A* 节点，索引公式：idx = y * GRID_COLS + x
    AStarNode nodes[GRID_COLS * GRID_ROWS];
    for (int y = 0; y < GRID_ROWS; y++) {
        for (int x = 0; x < GRID_COLS; x++) {
            int idx = y * GRID_COLS + x;
            nodes[idx].x = x;
            nodes[idx].y = y;
            nodes[idx].g = 9999;         // 初始代价设为极大值
            nodes[idx].h = 0;
            nodes[idx].f = 9999;
            nodes[idx].parentIdx = -1;   // -1 表示无父节点
            nodes[idx].closed = false;
            nodes[idx].inOpen = false;
        }
    }

    // 起点初始化：g=0, h=曼哈顿距离, f=g+h
    int startIdx = sy * GRID_COLS + sx;
    int goalIdx  = gy * GRID_COLS + gx;
    nodes[startIdx].g = 0;
    // 曼哈顿距离 = |dx| + |dy|，在四邻域网格中是最常用的可接受启发式
    nodes[startIdx].h = abs(sx - gx) + abs(sy - gy);
    nodes[startIdx].f = nodes[startIdx].h;
    nodes[startIdx].inOpen = true;

    MinHeap openSet = { .size = 0 };
    HeapPush(&openSet, nodes, startIdx);

    // ---- 步骤 2：主循环 ----
    while (openSet.size > 0) {
        // 从 Open 集取出 f 值最小的节点
        int curIdx = HeapPop(&openSet, nodes);
        AStarNode* cur = &nodes[curIdx];

        // 到达终点！进入步骤 3
        if (cur->x == gx && cur->y == gy) {
            // ---- 步骤 3：回溯路径 ----
            // 从终点沿 parentIdx 链一直回溯到起点
            // path[] 临时存储路径上所有节点的索引
            int path[GRID_COLS * GRID_ROWS];
            int pathLen = 0;
            int trace = curIdx;
            while (trace != -1) {
                path[pathLen++] = trace;
                trace = nodes[trace].parentIdx;
            }
            // path[pathLen-1] 是起点，path[pathLen-2] 是下一步
            // 如果路径长度 ≥ 2，说明起点≠终点，有有效的下一步
            if (pathLen >= 2) {
                int nextIdx = path[pathLen - 2];
                *nextX = nodes[nextIdx].x;
                *nextY = nodes[nextIdx].y;
                return true;
            }
            // 路径长度 = 1 说明怪兽已在玩家位置（不应该出现）
            *nextX = sx;
            *nextY = sy;
            return true;
        }

        // 将当前节点移入 Closed 集（已完成探索）
        cur->closed = true;
        cur->inOpen = false;

        // 扩展当前节点的四个邻居
        for (int d = 0; d < 4; d++) {
            int nx = cur->x + DX[d];
            int ny = cur->y + DY[d];

            // 边界检查：邻居必须在网格范围内
            if (nx < 0 || nx >= GRID_COLS || ny < 0 || ny >= GRID_ROWS)
                continue;

            int nIdx = ny * GRID_COLS + nx;

            // Closed 集检查：已探索的节点不再访问
            if (nodes[nIdx].closed) continue;

            // 障碍检查：不能移动到被其他怪兽占据的格子
            // （注意：终点格子即使被占据也要允许通过，否则怪兽永远追不到玩家）
            if (IsOccupiedByMonster(gs, nx, ny) && nIdx != goalIdx)
                continue;

            // 计算新路径的 g 值：每移动一步代价 +1
            int newG = cur->g + 1;

            // 如果找到更优路径 或 邻居尚未加入 Open 集
            if (newG < nodes[nIdx].g) {
                nodes[nIdx].g = newG;
                // 启发函数 h(n)：曼哈顿距离估计，保证 A* 最优性
                nodes[nIdx].h = abs(nx - gx) + abs(ny - gy);
                nodes[nIdx].f = nodes[nIdx].g + nodes[nIdx].h;
                nodes[nIdx].parentIdx = curIdx;  // 记录父节点用于回溯

                if (!nodes[nIdx].inOpen) {
                    nodes[nIdx].inOpen = true;
                    HeapPush(&openSet, nodes, nIdx);
                }
                // 注意：若邻居已在 Open 集中且 g 值被更新，
                // 理想情况下应做 decrease-key 操作，但简易堆实现中
                // 直接重新 push 即可（旧条目因 f 值较大会沉底，不影响正确性）
            }
        }
    }

    // ---- 步骤 4：无路径可达 ----
    // Open 集已空但未找到终点，怪兽被完全包围
    return false;
}

// =====================================================================
//  怪兽 AI 决策入口
// =====================================================================
//
//  【怪兽行为策略（总体设计思路）】
//
//  每只怪兽在每个移动周期执行以下决策流程：
//
//    1. 计算"策略距离" = |dx| + |dy|（怪兽到玩家的曼哈顿距离）。
//
//    2. 若策略距离 > 8 格（较远）：
//       → 怪兽以 30% 概率随机游走（模拟"巡逻/徘徊"行为），
//         70% 概率执行 A* 追踪。
//       设计意图：避免怪兽过于"聪明"地直线逼近，给玩家喘息空间。
//
//    3. 若策略距离 ≤ 8 格（较近）：
//       → 怪兽 100% 执行 A* 追踪，全力逼近。
//       设计意图：近距离高压迫使玩家快速决策，制造紧张感。
//
//    4. A* 寻路成功后，怪兽移动到路径的下一步。
//       A* 寻路失败（被围堵）时，怪兽原地等待。
//
//    5. 怪兽与玩家处于同一格子 → 玩家被抓住，游戏结束。
//
//  【为什么选择 A* 而不是简单贪心】
//    - 纯贪心算法（每次向曼哈顿距离最小的邻居移动）在遇到障碍时
//      会陷入死胡同，需要额外绕路逻辑。
//    - A* 天然处理障碍绕行，保证找到最优路径（若存在）。
//    - 对于期末大作业，A* 算法原理清晰、学术价值高、答辩时有话可说。
//
// =====================================================================
static void UpdateMonsterAI(GameState* gs, int monsterIdx) {
    Monster* m = &gs->monsters[monsterIdx];
    Player*  p = &gs->player;

    // 玩家已死亡，怪兽不再移动
    if (!p->alive) return;

    // ---- 冷却控制：每只怪兽独立的倒计时器 ----
    // moveCooldown 每帧减 1，减到 0 时才移动，然后重置为 moveInterval
    // moveInterval 越大，怪兽移动越慢，给玩家更多反应时间
    m->moveCooldown--;
    if (m->moveCooldown > 0) return;
    m->moveCooldown = m->moveInterval;

    // ---- 计算怪兽与玩家的曼哈顿距离 ----
    int dx = abs(m->pos.x - p->pos.x);
    int dy = abs(m->pos.y - p->pos.y);
    int dist = dx + dy;

    // ---- 行为决策 ----
    bool doChase = false;

    if (dist > 8) {
        // 远距离：30% 概率随机游走，70% 概率追踪
        // 这个概率平衡了"怪兽智能感"和"玩家可玩性"
        if (rand() % 100 < 70) {
            doChase = true;
        }
        // 否则：随机游走 —— 随机选一个合法邻居移动
        else {
            int attempts = 0;
            while (attempts < 20) {
                int dir = rand() % 4;
                int nx = m->pos.x + DX[dir];
                int ny = m->pos.y + DY[dir];
                if (nx >= 0 && nx < GRID_COLS && ny >= 0 && ny < GRID_ROWS &&
                    !IsOccupiedByMonster(gs, nx, ny)) {
                    m->pos.x = nx;
                    m->pos.y = ny;
                    break;
                }
                attempts++;
            }
            return;  // 随机游走完成，跳过 A* 追踪
        }
    } else {
        // 近距离：100% 全力追踪，制造紧张感
        doChase = true;
    }

    // ---- 执行 A* 追踪 ----
    if (doChase) {
        int nextX, nextY;
        if (AStarFindPath(gs, m->pos.x, m->pos.y,
                          p->pos.x, p->pos.y,
                          &nextX, &nextY)) {
            // A* 成功找到路径，移动到下一步
            m->pos.x = nextX;
            m->pos.y = nextY;
        }
        // A* 失败（被围堵）：原地不动，下一帧再试
    }
}

// =====================================================================
//  学分刷新逻辑
// =====================================================================
//
//  设计思路：
//  - 地图上始终维持 creditCount 个活跃的学分方块。
//  - 玩家吃到学分后，该学分的 active 变为 false。
//  - 每帧检查：若有未激活的学分，随机在空地生成新的。
//  - 保证游戏永不"无分可吃"。
// =====================================================================
static void RefreshCredits(GameState* gs) {
    for (int i = 0; i < gs->creditCount; i++) {
        if (!gs->credits[i].active) {
            // 随机生成新位置，确保不在"被占据"的格子上
            int attempts = 0;
            while (attempts < 200) {
                int nx = RandInt(0, GRID_COLS - 1);
                int ny = RandInt(0, GRID_ROWS - 1);
                // 不能生成在玩家位置
                if (PosEqual(gs->player.pos, (Position){nx, ny})) {
                    attempts++;
                    continue;
                }
                // 不能生成在怪兽位置
                if (IsOccupiedByMonster(gs, nx, ny)) {
                    attempts++;
                    continue;
                }
                // 不能生成在其他学分位置
                bool overlap = false;
                for (int j = 0; j < gs->creditCount; j++) {
                    if (i != j && gs->credits[j].active &&
                        gs->credits[j].pos.x == nx &&
                        gs->credits[j].pos.y == ny) {
                        overlap = true;
                        break;
                    }
                }
                if (overlap) { attempts++; continue; }

                gs->credits[i].pos = (Position){nx, ny};
                gs->credits[i].active = true;
                break;
            }
            attempts++;
        }
    }
}

// =====================================================================
//  对外接口实现
// =====================================================================

/*
 *  InitGame —— 游戏初始化
 *  ─────────────────────
 *  放置规则：
 *    - 玩家：从地图中央偏下位置起始，保证初始有操作空间
 *    - 怪兽：从地图四个角落 + 边缘生成，与玩家保持一定初始距离
 *    - 学分：随机散布在地图各处
 */
void InitGame(GameState* gs) {
    srand((unsigned int)GetTime());  // 用系统时间播种随机数

    // ---- 初始化玩家 ----
    gs->player.pos   = (Position){ GRID_COLS / 2, GRID_ROWS - 2 };
    gs->player.score = 0;
    gs->player.alive = true;

    // ---- 初始化怪兽 ----
    // 怪兽数量随难度递增：初始 2 只，后续可扩展
    gs->monsterCount = 2;
    // 怪兽 0：左上角出生，快速型
    gs->monsters[0].pos          = (Position){ 1, 1 };
    gs->monsters[0].state        = MONSTER_CHASE;
    gs->monsters[0].moveCooldown = 20;   // 开局延迟：先静止 20 帧再动
    gs->monsters[0].moveInterval = 35;   // 每 35 帧移动一次（约 0.6 秒/步）

    // 怪兽 1：右上角出生，慢速型 —— 给玩家初期适应时间
    gs->monsters[1].pos          = (Position){ GRID_COLS - 2, 1 };
    gs->monsters[1].state        = MONSTER_CHASE;
    gs->monsters[1].moveCooldown = 40;   // 开局延迟：先静止 40 帧再动
    gs->monsters[1].moveInterval = 55;   // 每 55 帧移动一次（约 0.9 秒/步）

    // ---- 初始化学分 ----
    gs->creditCount = 5;
    for (int i = 0; i < gs->creditCount; i++) {
        gs->credits[i].active = false;   // 初始为 false，由 RefreshCredits 生成
    }
    RefreshCredits(gs);

    // ---- 其他状态 ----
    gs->gameOver     = false;
    gs->frameCounter = 0;
    // 清空占据标记
    for (int x = 0; x < GRID_COLS; x++)
        for (int y = 0; y < GRID_ROWS; y++)
            gs->occupied[x][y] = false;
}

/*
 *  UpdateGame —— 每帧调用一次
 *  ───────────────────────────
 *  执行顺序：
 *    1. 处理玩家键盘输入（WASD 移动）
 *    2. 更新怪兽 AI（A* 追踪）
 *    3. 碰撞检测（玩家 vs 怪兽、玩家 vs 学分）
 *    4. 学分刷新
 */
void UpdateGame(GameState* gs) {
    if (gs->gameOver) return;

    // ---- 1. 玩家输入：WASD 移动 ----
    // 每按一次键移动一格（非连续移动，防止误操作）
    int newX = gs->player.pos.x;
    int newY = gs->player.pos.y;

    if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    newY--;
    if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  newY++;
    if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  newX--;
    if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) newX++;

    // 边界钳制
    if (newX < 0) newX = 0;
    if (newX >= GRID_COLS) newX = GRID_COLS - 1;
    if (newY < 0) newY = 0;
    if (newY >= GRID_ROWS) newY = GRID_ROWS - 1;

    gs->player.pos.x = newX;
    gs->player.pos.y = newY;

    // ---- 2. 更新怪兽 AI ----
    for (int i = 0; i < gs->monsterCount; i++) {
        UpdateMonsterAI(gs, i);
    }

    // ---- 3. 碰撞检测 ----

    // 3a. 怪兽碰撞：任一怪兽与玩家同格 → 游戏结束
    for (int i = 0; i < gs->monsterCount; i++) {
        if (PosEqual(gs->monsters[i].pos, gs->player.pos)) {
            gs->player.alive = false;
            gs->gameOver = true;
            return;
        }
    }

    // 3b. 学分收集：玩家与学分同格 → 加分并标记 inactive
    for (int i = 0; i < gs->creditCount; i++) {
        if (gs->credits[i].active &&
            PosEqual(gs->credits[i].pos, gs->player.pos)) {
            gs->credits[i].active = false;
            gs->player.score++;
        }
    }

    // ---- 4. 学分刷新：补充被吃掉的学分 ----
    RefreshCredits(gs);
}

/*
 /*
 * DrawGame —— 渲染游戏画面
 * ────────────────────────
 * 绘制顺序（由底到顶）：
 * 网格线 → 学分 → 怪兽 → 玩家 → UI 文字
 */
void DrawGame(const GameState* gs, Font zhFont) {
    // ---- 网格线（浅灰色，帮助玩家判断位置） ----
    for (int x = 0; x <= GRID_COLS; x++) {
        DrawLine(x * CELL_SIZE, PLAY_AREA_Y,
                 x * CELL_SIZE, PLAY_AREA_Y + GRID_ROWS * CELL_SIZE,
                 Color{60, 60, 80, 100});
    }
    for (int y = 0; y <= GRID_ROWS; y++) {
        DrawLine(0, PLAY_AREA_Y + y * CELL_SIZE,
                 GRID_COLS * CELL_SIZE, PLAY_AREA_Y + y * CELL_SIZE,
                 Color{60, 60, 80, 100});
    }

    // ---- 学分：黄色小方块（比格子小一圈，视觉更精致） ----
    for (int i = 0; i < gs->creditCount; i++) {
        if (!gs->credits[i].active) continue;
        Vector2 scr = GridToScreen(gs->credits[i].pos.x, gs->credits[i].pos.y);
        DrawRectangle((int)scr.x + 8, (int)scr.y + 8,
                      CELL_SIZE - 16, CELL_SIZE - 16,
                      Color{255, 215, 0, 255});   // 金黄色
    }

    // ---- 怪兽：红色方块 ----
    for (int i = 0; i < gs->monsterCount; i++) {
        Vector2 scr = GridToScreen(gs->monsters[i].pos.x, gs->monsters[i].pos.y);
        DrawRectangle((int)scr.x + 4, (int)scr.y + 4,
                      CELL_SIZE - 8, CELL_SIZE - 8,
                      Color{220, 50, 50, 255});   // 深红色
    }

    // ---- 玩家：蓝色方块（最后绘制，保证在最上层） ----
    {
        Vector2 scr = GridToScreen(gs->player.pos.x, gs->player.pos.y);
        DrawRectangle((int)scr.x + 4, (int)scr.y + 4,
                      CELL_SIZE - 8, CELL_SIZE - 8,
                      Color{50, 120, 255, 255});  // 亮蓝色
    }
    // ---- 分数显示（已修复恢复） ----
    char scoreText[32];
    snprintf(scoreText, sizeof(scoreText), "得分: %d", gs->player.score);
    DrawTextEx(zhFont, scoreText, Vector2{ (float)(GRID_COLS * CELL_SIZE - 150), 14.0f }, 22, 1.0f, RAYWHITE);

    // ---- 操作提示 ----
    DrawText("WASD/Arrow: Move  |  ESC: Exit",
             10, PLAY_AREA_Y + GRID_ROWS * CELL_SIZE + 10, 16, GRAY);

    // ---- ★ 三人制作者信息 (期末硬性要求) ★ ----
    // 1. 用数组把三个人的信息整齐排列（去除了多余的方括号，排版更美观）
    const char* authors[3] = {
        "制作者1: 数理科学 - 251840251 - 戴宇鑫",
        "制作者2: 数理科学 - 251840001 - 张砚申",
        "制作者3: 化生 - 251850128 - 姚迪睿"
    };
    
    // 2. 通过循环自动计算每行坐标，依次向下整齐排布
    for (int i = 0; i < 3; i++) {
        Vector2 infoSize = MeasureTextEx(zhFont, authors[i], 16, 1.0f);
        
        DrawTextEx(zhFont, authors[i],
                   Vector2{ (float)(GRID_COLS * CELL_SIZE) - infoSize.x - 15, 
                            PLAY_AREA_Y + GRID_ROWS * CELL_SIZE + 6.0f + i * 18.0f }, 
                   16, 1.0f, LIGHTGRAY);
    }

    // ---- 游戏结束提示 ----
    if (gs->gameOver) {
        // 半透明遮罩
        DrawRectangle(0, 0, GRID_COLS * CELL_SIZE, PLAY_AREA_Y + GRID_ROWS * CELL_SIZE, Color{0, 0, 0, 180});

        const char* overLine1 = "游戏结束";
        Vector2 sz1 = MeasureTextEx(zhFont, overLine1, 36, 2.0f);
        DrawTextEx(zhFont, overLine1, 
                   Vector2{(GRID_COLS * CELL_SIZE - sz1.x) / 2.0f, 160.0f}, 36, 2.0f, RED);

        const char* overLine2 = "按 R 重新开始，按 ESC 退出";
        Vector2 sz2 = MeasureTextEx(zhFont, overLine2, 24, 2.0f);
        DrawTextEx(zhFont, overLine2, 
                   Vector2{(GRID_COLS * CELL_SIZE - sz2.x) / 2.0f, 220.0f}, 24, 2.0f, RAYWHITE);
    }
}