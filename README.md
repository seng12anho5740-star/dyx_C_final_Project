# Grid Survival Game <br> 🎮 网格生存博弈系统

[![Language](https://img.shields.io/badge/Language-C++-blue.svg)]()
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)]()
[![Status](https://img.shields.io/badge/Status-Completed-green.svg)]()

本系统是针对课程大作业开发的 2D 动作博弈系统，旨在通过底层工程实践，探讨复杂网格环境下的寻路逻辑与数据持久化架构。

---

## 📌 项目概述
**Grid Survival Game** 是一款基于 C++ 与 Raylib 开发的 2D 动作游戏。在 20x14 的离散网格空间内，系统模拟了主控实体与追踪实体的动态交互。本项目重点攻克了底层寻路算法效率、本地化数据存储逻辑以及跨环境编译链配置等技术挑战。

## ✨ 核心技术亮点

| 功能模块 | 技术要点说明 |
| :--- | :--- |
| **A* 寻路算法** | 纯 C 手写实现。集成曼哈顿距离启发函数，实现追踪 AI 对主控实体的实时最优路径计算。 |
| **持久化存储** | 封装高效文件 I/O 接口，实现排行榜数据的 CRUD 操作，支持 Top-5 自动化排序与长效存储。 |
| **环境解耦** | 自研 `compile.bat` 自动化构建脚本，通过相对路径管理依赖，实现无 IDE 环境下的一键秒级编译。 |
| **UTF-8 渲染** | 基于 `LoadFontEx` 与自定义字形提取技术，深度优化中文字库加载，规避字符乱码问题。 |

## 🏗️ 系统结构

```text
.
├── src/                # 核心逻辑与 API 接口层
├── assets/             # 资源库 (包含中文字体)
├── include/            # Raylib 依赖头文件
├── lib/                # 链接库文件 (Static/Dynamic)
├── compile.bat         # 自动化构建脚本
└── .gitignore          # Git 过滤规则 (确保仓库纯净)