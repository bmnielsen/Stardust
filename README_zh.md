# ShushanKing — AIIDE 2026 星际争霸 AI 竞赛提交说明（中文版）

- 参赛名称：ShushanKing
- 联系人：Stephen Howsiny <howsiny@mail.sdu.edu.cn>
- 所属单位：iFlytek Digital Technology (Hefei)
- 种族：Protoss（神族）
- 形态：原生 BWAPI DLL（无需代理/客户端）
- BWAPI 版本：**4.4.0**（官方发布版；随包附带头文件位于 `3rdparty/BWAPILIB/include`，`BWAPI_VER "4.4.0"`）
- 需要 GPU：否
- 简介：基于 Stardust（其本身源自 UAlbertaBot 框架）深度改造的 Protoss bot，重写了策略选择、对手学习与微操模块。随包原样附带上游 Stardust 许可证（见 LICENSE）。

## 包内容

```
ShushanKing/
  ShushanKing.dll        编译好的二进制（可直接运行）
  params.json            运行时配置（从本目录读取）
  strategies/*.json      开局策略库（5 个文件）
  recognizers.json       敌方开局识别器阈值（可选）
  fingerprints.json      策略标签词表（可选）
  README.md              英文说明
  README_zh.md           本文件（中文说明）
  LICENSE                Stardust 许可证原文（上游项目条款，逐字附带）
  src/                   全量 C++ 源码
  data/                  运行数据参考副本（params/strategies 快照）
  tests/                 离线测试工具（开发用途，不参与构建；脚本默认路径可用参数覆盖）
  vs/                    Visual Studio 2022 解决方案与工程文件
  3rdparty/              第三方依赖（BWAPILIB 头文件、BWEM、FAP、zstd、
                         zstdstream、nlohmann、bitsery、cppcrc）——不含 BWAPI 本体
```

## 安装

将本文件夹复制到 `bwapi-data/AI/ShushanKing/`，并把 BWAPI 的 `ai` 配置行指向
`bwapi-data/AI/ShushanKing/ShushanKing.dll`。DLL 按以下顺序查找 `params.json`、
`strategies/`、`recognizers.json`、`fingerprints.json`：**DLL 自身所在目录**（因此
无论安装目录叫什么名字都能找到配置，例如比赛方的别名部署）、`bwapi-data/AI/ShushanKing/`、
`bwapi-data/`、`bwapi-data/read/`、`bwapi-data/write/`。所有可选文件缺失时均安全
降级为内置默认值；建议附带 `params.json`（使用调优后的配置）。

学习档案写入 `bwapi-data/write/shushanking_<对手名>.json`（每个对手名一个累积文件，
保留最近 500 局）。诊断日志（`shushanking_framestats.txt`、`shushanking_htpipe.txt`、
`shushanking_dt_suspect.txt`）同样为累积式、文件名唯一，写入 `bwapi-data/write/`。

## 编译

环境要求：Visual Studio 2022（v143 工具集）、Windows SDK（任意 10.x 版本——工程
默认 10.0.26100.0，会自动重定向）、C++20、仅 Win32 (x86) Release 配置。无需任何
环境变量——所有包含/库路径均相对解决方案目录，除随包 `3rdparty/` 外无其他第三方
依赖。不得附带 BWAPI 本体；`3rdparty/BWAPILIB/include` 仅含编译所需的 BWAPI 4.4.0
公开头文件。

命令行：

```
msbuild vs\ShushanKing.sln /p:Configuration=Release /p:Platform=Win32
```

产物：`vs\src\Release\ShushanKing.dll`（约 2.0 MB）。四个依赖工程（BWAPILIB、BWEM、
zstd、zstdstream）随同一解决方案构建；关于缺失 `libzstd_static.vcxproj` 工程引用的
MSB9008 警告为已知无害项（zstd 静态库由同一解决方案构建并正常链接）。包内不含任何
预编译产物；干净构建会在 `vs\3rdparty\*\Release\` 自行生成全部依赖库。

## 源码与二进制对应关系

包内源码为验证版（108 局验证集总胜率 92.6%）附加仅涉及命名的提交期改动：
运行时文件名前缀（`shushanking_*`）、AI 模块类名（`ShushanKingAIModule`）、
工程/解决方案文件名，以及上述配置搜索根。这些改动只涉及字符串常量、标识符与
文件名——游戏玩法、策略与决策逻辑与验证版完全一致。

随包 `ShushanKing.dll` 由本源码树直接构建（SHA256 前 8 位：`C1D7C8A3`；
2,017,280 字节）。与改名前二进制的字符串集审计显示，真实字符串差异仅为预期的
改名文件名与 RTTI 类名。

## 注意事项

- 帧预算：bot 自带 `onFrame` 耗时监控与反应式阶梯限速（`params.json` 中
  `frame_governor.mode=ladder, limit_ms=55, skip_max=3`）；实测平均每帧 <1ms、
  1000+ 帧采样中无超过 35ms 的帧，远在比赛 42ms / 55ms 限制之内。
- 无作弊项：从不开启完美信息，不发送刷屏聊天，仅写入 `bwapi-data/write/`。
