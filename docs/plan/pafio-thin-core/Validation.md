# Pafio 薄包管理核心 — 冻结验收合同

**Plan:** `pafio-thin-core`  
**Node:** `0861f893-f534-42a9-99d1-05480d9fe05b`  
**Acceptance dispatch:** `c9d208e7-5670-423b-8c8c-c25d3f60df94`  
**Frozen:** 2026-07-28

## 1. 本节点唯一场景

一个本地 Styio 项目经由 path 包传递依赖一个 localhost registry 包。一次 `sync` 生成
`spio.lock`、可信共享缓存对象和项目本地 `.spio/resolution-v1.json`；第二次普通运行以及服务器
停止后的 `--locked --offline` 运行复用相同状态，并产生字节不变的 lock/resolution。

本节点不执行 build/run/test、toolchain、cloud、hosted control-plane、Vityo、vendor、pack 或
publish 产品行为。interop 测试允许调用仓库已有 registry-v2 脚本制作一次性 localhost 只读
夹具；这只是测试前置条件，不构成 Pafio 核心的新生产者能力。

## 2. 冻结的文件与字节合同

- resolution 路径固定为 manifest 同目录下的 `.spio/resolution-v1.json`。
- 文件是 UTF-8 JSON、两空格缩进、末尾一个 LF；object key、`roots`、`packages` 和每包
  `dependencies` 均按字典序 canonical 输出。
- `manifest_sha256` 是 `LoadManifest` 后 `SerializeManifestCanonical` 所得字节的 SHA-256；
  `lock_sha256` 是实际提交的 canonical `spio.lock` 字节的 SHA-256。
- 顶层只允许 `schema_version`、`manifest_sha256`、`lock_sha256`、`roots`、`packages`。
- package 只允许 `id`、`root`、`content_sha256`、`dependencies`；dependency 只允许
  `alias`、`package_id`。
- `root` 必须是当次 sync 后存在的绝对源码目录。lockfile 中不得出现这些绝对路径。
- registry package 的 `content_sha256` 必须逐字等于已验证 registry 制品 SHA-256，不能使用
  URL、mtime、FNV、manifest hash 或缓存路径替代。
- path/workspace 是可变源码根，当前设计没有真实内容摘要。验收明确禁止用 manifest hash冒充
  package content hash；冻结合同要求它们输出 JSON `null`。真实 tree digest 如需引入，必须
  另立节点设计忽略规则、性能和自引用语义，不能改变本闭环合同。

## 3. 可执行验收映射

| Case | 路径 / observation target | 前置条件 | Action | 可观察结果与抗假阳性 oracle | 映射 |
|---|---|---|---|---|---|
| AC-01 canonical serializer | `tests/native/ResolutionContractTests.cpp`; `SerializeResolutionCanonical` | 人工构造乱序 roots/packages/aliases，真实目录存在，registry digest 固定 | 对同一图及等价乱序图序列化 | 两份 bytes 完全相同；精确 JSON 字段集合；排序明确；registry digest 原样保留；本地摘要为 `null`；末尾单 LF | REQ-THIN-002/004；criterion 0/3；pure serializer seam |
| AC-02 invalid graph rejection | `tests/native/ResolutionContractTests.cpp`; serializer error | 分别构造重复 package id、dangling alias、重复 alias、缺失/相对 root、非法摘要 | 单独序列化每个坏图 | 每一项都抛 `ResolutionError`；测试逐案改变一个事实，避免“任意异常即通过” | REQ-THIN-004；criterion 3；resolution-v1 interface errors |
| AC-03 path sync replay | `tests/native/SyncTests.cpp`; `spio.lock` 与 `.spio/resolution-v1.json` | root → path → path，隔离 `SPIO_HOME` | 普通 sync 两次，再 `--locked --offline` 一次 | 三次成功；两文件 bytes 均不变；摘要逐字等于独立重算值；alias 可遍历到传递包；lock 不含临时根绝对路径 | REQ-THIN-001/002/004；criteria 0/1 |
| AC-04 mixed graph + Styio consumer | `tests/interop/pafio-thin-core-contract.sh`; CLI 与参考 consumer | root → path → localhost registry，TUF/descriptor 夹具有效 | cold sync；参考 consumer 仅读取 manifest、lock、resolution、package roots | consumer 得到完整传递 alias；所有 ID 唯一、引用闭合、root 存在；schema/source 都没有越界字段 | REQ-THIN-001/003/004/005；criteria 0/3/4；minimal consumer seam |
| AC-05 two-process single-flight | `tests/interop/pafio-thin-core-contract.sh`; localhost access log 与 CAS | 两项目共享空 cache 和同一 registry digest | 同时启动两个 sync | 两进程成功；artifact URL 只有一次 GET；只有一个摘要正确的 blob/ready checkout；无 `.tmp.*` 或半成品 | REQ-THIN-003；criterion 2；two-process seam |
| AC-06 warm locked/offline | `tests/interop/pafio-thin-core-contract.sh`; bytes、server、marker stat | AC-05 已完成 | 停止服务器，执行 `--locked --offline` | 成功；lock/resolution bytes 与 checkout marker stat 不变；服务器已不可达，因此成功不能由网络回退伪造 | REQ-THIN-001/002/003；criteria 0/1 |
| AC-07 corrupt cache policy | `tests/interop/pafio-thin-core-contract.sh`; blob digest 与项目状态 | 已有有效 blob 和 committed resolution | 损坏 blob；先 offline sync，再恢复 localhost server 做 online sync | offline 稳定失败且 lock/resolution bytes 不变；online 重新取得一次 artifact、恢复正确摘要且项目输出保持 canonical | REQ-THIN-003；criterion 2 |
| AC-08 stale/shape rejection | `tests/interop/pafio-thin-core-contract.sh`; 参考 Styio consumer | 从有效 resolution 分别制作单缺陷副本 | 分别改变 manifest digest、lock digest、schema version、duplicate id、dangling alias、missing root、额外领域字段；lock digest 单缺陷同时作为“新 lock 已提交、旧 resolution 尚未提交”的摘要失配 oracle | 每个副本均被 consumer 拒绝，原件仍接受；lock mismatch 必须稳定返回 `lock_stale`，证明跨文件中断 fail closed；每案断言稳定错误码而非只看非零退出 | REQ-THIN-001/004/005；criteria 2/3/4 |
| AC-09 resolver/security inheritance | 现有 `tests/native/LockTests.cpp`, `SecurityTests.cpp`, `TufTests.cpp` | 已有 exact resolver、TUF 和 prescan 夹具 | 运行冻结 filter | 完整 cycle/两条冲突链、deterministic lock、traversal/TUF tamper fail-closed 继续通过 | REQ-THIN-002/003；criteria 1/2 |
| AC-10 boundary audit | `tests/interop/pafio-thin-core-contract.sh`; source/schema/CMake target | 新 contract 已接入 `spio_resolution` | 静态读取 contract source、schema 与 `src/CMakeLists.txt` | contract 不含禁止 include/token/字段；`spio_resolution` 不链接 Cloud/Tool/Toolchain/Plan/Project service | REQ-THIN-005；criterion 4 |

## 4. 参考 Styio consumer 的最小判定

interop 中的 consumer 不读取 registry metadata、Pafio cache index 或命令输出，只做：

1. schema version 与精确字段集合检查；
2. canonical manifest/lock SHA-256 对比；
3. roots/package IDs 唯一且闭合；
4. root 是存在的绝对目录；
5. aliases 唯一、排序且指向存在的 package；
6. registry ID 的 `content_sha256` 为 64 位小写十六进制；本地 ID 必须为 `null`；
7. 从每个 root 按 alias 遍历到传递依赖。

每个负例只改变一个事实并断言专属错误码，防止由 JSON 解析失败、文件缺失或其他无关错误造成
假阳性。

## 5. 聚焦回归合同

执行者只运行：

```text
cmake -S . -B build-codex
cmake --build build-codex --target spio spio_native_tests
./build-codex/bin/spio_native_tests --gtest_filter='ManifestTests.*:LockfileTests.*:LockGenerationTests.*:ResolverTests.*:SyncCliTests.*:SecurityTests.*:TufVerifierTests.*:ResolutionContractTests.*'
ctest --test-dir build-codex -R 'spio_(cli_sync_help|registry_http_fetch|registry_v2_http_read|registry_v2_unit|tuf_parity_unit|thin_core_contract)$' --output-on-failure --no-tests=error
```

如 gtest suite 名称变化，先执行一次 `--gtest_list_tests` 取得精确 filter；不得用多次全量 ctest
替代。节点实现与修复期间不运行 full regression。

## 6. 既有安全证据复用

- `tests/native/LockTests.cpp`: cycle、diamond conflict、canonical lock、registry fetch、archive traversal；
- `tests/native/SecurityTests.cpp`: registry identity/path/trust descriptor；
- `tests/native/TufTests.cpp`: TUF tamper fail-closed；
- `tests/native/ManifestTests.cpp`: canonical manifest/lock serializers。

这些路径本次无需重复造夹具或改写实现；它们作为 AC-09 的现有可执行证据。

## 7. 已收敛的产品语义

以下两项已同步到 `Requirements.md`、`Architecture.md` 与 `Checkpoints.json`，不再是 executor
开始前的设计缺口：

1. path/workspace 的 `content_sha256` 固定为 JSON `null`；AC-01、AC-03 与参考 consumer 都拒绝
   manifest hash 等伪内容摘要。
2. prepare 失败保持旧 lock/resolution 字节不变；commit 严格先原子写 lock、最后原子写
   resolution。两次单文件提交之间中断时不承诺回滚新 lock，而由旧 resolution 的
   `lock_sha256` 失配触发 `lock_stale`，保证消费者 fail closed；本闭环不引入 journal、
   rollback 或双格式兼容层。

剩余设计缺口：无。

## 8. 最终全量回归

本计划只有一个实现节点。聚焦回归与独立只读审计通过后，如用户授权最终交付验证，只执行一次
现有 delivery/full regression；实现过程中不得重复运行全量门禁。
