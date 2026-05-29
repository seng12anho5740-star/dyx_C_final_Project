#include "raylib.h"
#include "game_logic.h"
#include "data_manager.h"       // ★ 排行榜数据持久化
#include <stdio.h>

// ===== UTF-8 码点构建器：为 LoadFontEx 提取所有需要的字形 =====
// Raylib 的 LoadFontEx 不会自动加载全部 Unicode 字形，
// 必须显式传入程序会用到的每个字符的码点列表，否则中文显示为 ????
static int* BuildCodepoints(const char* text, int* outCount) {
    static int buffer[512];
    int count = 0;
    const unsigned char* p = (const unsigned char*)text;
    while (*p && count < 500) {
        int cp;
        if      ((*p & 0x80) == 0x00) { cp = *p++; }
        else if ((*p & 0xE0) == 0xC0) { cp = ((*p & 0x1F) << 6)  | (*(p+1) & 0x3F); p += 2; }
        else if ((*p & 0xF0) == 0xE0) { cp = ((*p & 0x0F) << 12) | ((*(p+1) & 0x3F) << 6) | (*(p+2) & 0x3F); p += 3; }
        else                           { cp = ((*p & 0x07) << 18) | ((*(p+1) & 0x3F) << 12) | ((*(p+2) & 0x3F) << 6) | (*(p+3) & 0x3F); p += 4; }
        buffer[count++] = cp;
    }
    *outCount = count;
    return buffer;
}

int main()
{
    // ===== 窗口初始化 =====
    const int screenWidth  = 800;
    const int screenHeight = 680;
    InitWindow(screenWidth, screenHeight, "南大挑战赛");
    SetTargetFPS(60);

   // ===== 加载中文字体 =====
    const char* usedChinese =
        "南大挑战赛C大作业"
        "游戏结束按重新开始退出"
        "排行榜暂无历史记录得分第名"
        "制作者数理科学化生"           // <--- 确保包含“数理科学”和“化生”
        "戴宇鑫张砚申姚迪睿"           // <--- 确保包含三个人的完整姓名
        " 0123456789:-,，.——/|"       
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ" 
        "abcdefghijklmnopqrstuvwxyz";
    int cpCount = 0;
    int* codepoints = BuildCodepoints(usedChinese, &cpCount);
    Font zhFont = LoadFontEx("assets/simhei.ttf", 32, codepoints, cpCount);

    // ===== 游戏状态初始化 =====
    GameState gameState;
    bool scoreSaved = false;                // ★ 防止 Game Over 时重复写文件
    InitGame(&gameState);

    // ===== 主循环 =====
    while (!WindowShouldClose())
    {
        // ---- ESC 退出 ----
        if (IsKeyPressed(KEY_ESCAPE)) break;

        // ---- R 键重新开始 ----
        if (IsKeyPressed(KEY_R) && gameState.gameOver) {
            InitGame(&gameState);
            scoreSaved = false;             // ★ 复位标记，新一局可再次存分
        }

        // ---- 更新游戏逻辑（输入、AI、碰撞） ----
        UpdateGame(&gameState);

        // ★★★ Game Over 时保存分数（仅触发一次） ★★★
        if (gameState.gameOver && !scoreSaved) {
            SaveScore(gameState.player.score);
            scoreSaved = true;
        }

        // ---- 渲染 ----
        BeginDrawing();
            ClearBackground(Color{30, 30, 45, 255});

            DrawGame(&gameState, zhFont);

        // ★★★ Game Over 时渲染排行榜 Top 5 ★★★
            if (gameState.gameOver) {
                ScoreRecord top5[LEADERBOARD_MAX];
                int count = LoadLeaderboard(top5);

                if (count > 0) {
                    // 标题下移到 Y=300
                    DrawTextEx(zhFont, "—— 排行榜 TOP 5 ——",
                               Vector2{250, 300}, 24, 2.0f, GOLD);

                    for (int i = 0; i < count; i++) {
                        char line[64];
                        snprintf(line, sizeof(line),
                                 "No.%d   %d 分   %s", // 这里的 '.' 终于能正常显示了
                                 i + 1, top5[i].score, top5[i].date);
                        // 列表下移，从 Y=350 开始依次往下排
                        DrawTextEx(zhFont, line,
                                   Vector2{260, 350.0f + i * 35.0f},
                                   20, 1.5f, RAYWHITE);
                    }
                } else {
                    DrawTextEx(zhFont, "暂无历史记录",
                               Vector2{300, 330}, 22, 2.0f, LIGHTGRAY);
                }
            }
        EndDrawing();
    }

    // ===== 清理资源 =====
    UnloadFont(zhFont);
    CloseWindow();
    return 0;
}
