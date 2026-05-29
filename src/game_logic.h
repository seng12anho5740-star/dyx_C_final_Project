#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "raylib.h"

// ===== 网格参数 =====
// 游戏区域：800x600 窗口，顶部 50px 留给标题栏
// 每个格子 40x40，共 20 列 x 14 行 = 800x560
#define GRID_COLS   20
#define GRID_ROWS   14
#define CELL_SIZE   40
#define PLAY_AREA_Y 50

// ===== 游戏实体数量上限 =====
#define MAX_MONSTERS 5
#define MAX_CREDITS  10

// ===== 坐标结构体（纯 C 风格） =====
typedef struct {
    int x, y;   // 网格坐标，x ∈ [0, GRID_COLS-1], y ∈ [0, GRID_ROWS-1]
} Position;

// ===== 玩家结构体 =====
typedef struct {
    Position pos;       // 当前网格位置
    int      score;     // 已收集的学分数
    bool     alive;     // 存活状态
} Player;

// ===== 学分结构体 =====
typedef struct {
    Position pos;       // 学分的网格位置
    bool     active;    // 是否未被收集（false 表示已被吃或待重生）
} Credit;

// ===== 怪兽状态枚举 =====
typedef enum {
    MONSTER_CHASE,      // 追踪模式：持续向玩家逼近
    MONSTER_PATROL      // 巡逻模式：在出生点附近随机游荡（预留扩展）
} MonsterState;

// ===== 怪兽结构体 =====
typedef struct {
    Position     pos;             // 当前网格位置
    MonsterState state;           // 当前行为状态
    int          moveCooldown;    // 移动冷却计数器（帧数），控制怪兽速度
    int          moveInterval;    // 每隔多少帧移动一次（不同怪兽速度不同）
} Monster;

// ===== 游戏全局状态 =====
typedef struct {
    Player  player;
    Monster monsters[MAX_MONSTERS];
    Credit  credits[MAX_CREDITS];
    int     monsterCount;
    int     creditCount;
    bool    gameOver;
    int     frameCounter;
    // A* 寻路专用：标记哪些格子被其他怪兽占据，避免堆叠
    bool    occupied[GRID_COLS][GRID_ROWS];
} GameState;

// ===== 函数声明 =====

// 初始化游戏：放置玩家、生成怪兽、散布学分
void InitGame(GameState* gs);

// 每帧更新：处理输入、AI 移动、碰撞检测、学分刷新
void UpdateGame(GameState* gs);

// 渲染整个游戏画面
void DrawGame(const GameState* gs, Font zhFont);

#endif // GAME_LOGIC_H
