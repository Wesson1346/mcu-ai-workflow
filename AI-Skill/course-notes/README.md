# 课件要点（本仓库不含）

这个目录原本存放课件内容提炼，**已从公开仓库移除**。

## 为什么

这些内容整理自一套第三方免费课程《AI开发单片机系统教程》的配套 PDF，
**版权归原作者**。即使原课程免费，也不代表其中的内容可以再分发。
所以本仓库只保留这份说明，不附带任何课件原文或提炼。

## 需要课件内容怎么办

直接去看原作者的知识库（IMA 平台，知识库名《AI开发单片机系统教程（教程资料库）》）。
里面有 17 篇配套课件 PDF、元件清单、原理图、渐进式工程源码。

## 本仓库的 skill 已覆盖课件的方法论

| 课件章节 | 对应位置 |
| --- | --- |
| 01–03 前言 / 六步流程 / AI 工具 | `orchestrator/SKILL.md` + `references/six-step-map.md` |
| 04 需求设计 | `AI-Skill/mcu-requirement-analysis/` |
| 05 硬件选型 | `AI-Skill/mcu-hardware-selection/` |
| 06 功能拆解 | `AI-Skill/mcu-functional-decomposition/` |
| 07 硬件搭建 | `AI-Skill/mcu-hardware-building/` |
| 08 工程搭建 | `AI-Skill/stm32-driver/references/hal-guide.md` |
| 09–12 四个模块编程 | `AI-Skill/stm32-driver/references/sensor-catalog.md` |
| 13 主控逻辑 | `AI-Skill/mcu-module-programming/`（本地优先 + 查漏补缺） |
| 14–16 OneNET 云平台 | `AI-Skill/stm32-driver/references/onenet-setup-guide.md` + `onenet-guide.md` |
| 17 uniapp（App 端） | 本仓库不含——六步法到 S6 即交付完整系统，App 属加分项 |

## 本地保留

如果你在本地保留了课件提炼文件，`.gitignore` 已经把它们排除在版本控制之外：

```
AI-Skill/course-notes/*.md
!AI-Skill/course-notes/README.md
```

所以你可以放心在本目录放自己的笔记，不会误提交。
