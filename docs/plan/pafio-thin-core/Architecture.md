# Pafio 薄包管理核心 — 架构与收敛路线

**Plan:** `pafio-thin-core`  
**Last updated:** 2026-07-28

## 1. 唯一核心流水线

```text
Manifest
  │ parse + canonical digest
  ▼
Lock-aware exact resolver ──> immutable ResolvedGraph
  │                              │
  │ missing package descriptors  │ canonical lock
  ▼                              ▼
Verified registry reader      Atomic lock write
  │
  ▼
Bounded materializer -> SHA-256 CAS -> Atomic resolution-v1 write
                                          │
                                          ▼
                                        Styio
```

核心只有五个领域：

1. **Manifest/Lock**：声明输入与可提交的精确解析结果；
2. **Resolver**：纯包图决策，不执行编译器或云策略；
3. **Registry Reader/Security**：验证静态只读元数据和制品；
4. **Store**：内容寻址、并发互斥、安全解包与原子提交；
5. **Resolution Contract**：把本机源码根交给 Styio。

CLI 只是这一流水线的适配层，不拥有第二套领域逻辑。

## 2. 模块目标

### 保留并收紧

| 目标领域 | 当前落点 | 收敛方向 |
|---|---|---|
| Manifest/Lock | `src/SpioManifest/` | 只保存 package/workspace/dependency 与 lock 事实 |
| Resolver | `src/SpioResolve/Resolver.*` | 输出不可变图；继续使用排序邻接表和稳定错误链 |
| Registry Reader | `src/SpioRegistryClient/` | 只读静态 registry；服务端行为不得反向依赖 |
| Security | `src/SpioSecurity/` | 作为 reader/materializer 的内部保障 |
| Store primitives | `src/SpioCore/` | digest identity、FileLock、AtomicFile、路径约束 |
| Consumer commands | `src/SpioWorkflow/Dependencies.*`, `src/SpioApp/PackageApp.*` | add/remove/sync/tree 共享一条服务路径 |

### 后续一次性迁移，不在首个节点中继续开发

| 当前表面 | 目标所有者/动作 |
|---|---|
| `SpioWorkflow`, `SpioPlan`, `WorkflowApp` 的 build/run/test/check | Styio CLI/编译服务；Pafio 仅提供 `resolution-v1` |
| `SpioTool`, `SpioToolchain`, `ToolApp`, install/use/set/doctor | Styio 安装器或独立 toolchain manager |
| `SpioCloud`, `CloudApp`, hosted job/control-plane | Styio Platform |
| `ProjectGraphContract` 中 Vityo/cloud/tool 聚合字段 | 删除；IDE 经 Styio/LSP 与最小 resolution 合同取事实 |
| `src/spio_registry_v2/` 与 registry server/control-plane scripts | 独立 registry service/publisher 工具仓库 |
| `SpioPack`, `SpioPublish` | 消费者闭环稳定后，以第二个独立能力重做最小客户端 |
| Git source cache | 停止扩展，迁移后只保留 path + registry |

每一行是一个所有权迁移场景。开始某一行时必须同时完成新所有者接管、调用方切换、旧命令/代码/
文档/测试删除；不得用兼容 shim 长期并存。

## 3. 首个闭环的领域模型

```text
PackageKey        = canonical namespace/name
PackageId         = PackageKey + exact version + source digest
DependencyEdge    = alias + target PackageId + kind
ResolvedPackage   = PackageId + source descriptor + sorted edges
ResolvedGraph     = sorted roots + sorted packages
LockPackage       = portable ResolvedPackage projection (no local path)
ResolutionPackage = local ResolvedPackage projection (contains local root and nullable content digest)
```

建议数据结构：

- `unordered_map<PackageKey, NodeIndex>`：包名冲突与单版本选择，平均 `O(1)`；
- `unordered_map<SourceFingerprint, NodeIndex>`：相同来源去重；
- `vector<ResolvedPackage>`：紧凑连续存储；
- 每个节点使用按 alias 排序的 `vector<DependencyEdge>`：稳定输出、低额外内存；
- DFS active/done color + active stack：一次遍历给出循环路径；
- 最终序列化前按 `PackageId` 排序，避免依赖 hash iteration order。

精确版本图的时间复杂度保持 `O(V + E)`，内存为 `O(V + E)`。不得在这一闭环中引入通用 SAT
状态、全版本候选集或重复扫描整个图。

## 4. `sync` 事务

`sync` 分为 prepare 与 commit 两段：

### Prepare（不改项目可见状态）

1. 读取并校验 manifest；
2. 读取现有 lock，判断是否满足 manifest；
3. 解析为不可变 `ResolvedGraph`；
4. 计算待获取的唯一 digest 集；
5. 通过有界任务队列并发获取不同 digest；
6. 每个 digest 使用 single-flight/per-digest lock，执行下载、哈希、归档预扫描和临时解包；
7. 构造 canonical lock bytes 与 resolution bytes。

### Commit

1. 对每个 CAS 对象同目录原子 rename；
2. 原子写 lock；
3. 最后原子写 resolution。

resolution 最后提交，使它成为“完整事务已成功”的项目本地标记。任何 prepare/commit 失败都
不能让新 resolution 指向未提交对象。lock 与 resolution 是两个独立文件，不能假装两次 rename
具备跨文件原子性：若进程在两次写入之间中断，旧 resolution 的 `lock_sha256` 必须与新 lock
失配，消费者据此 fail closed；恢复由下一次 `sync` 完成，不引入 journal 或双格式兼容层。

默认并行度取一个小的固定上限（例如 8）并允许内部配置覆盖；不能按依赖数无限创建线程。相同
digest 的并发请求共享一个 future/result，独立 digest 才并发。

## 5. Store 与缓存策略

```text
store/
  sha256/
    ab/
      cd/
        <full-digest>/
          artifact
          source/
          verified
```

- digest 是身份，package name/version 只用于诊断与索引；
- `artifact` 在下载完成后校验 SHA-256；
- 解包前统一做路径、symlink 与大小限制预扫描；
- `source/` 在提交后视为不可变并尽可能只读；
- `verified` 记录验证所依据的 digest/size/store schema，不是身份来源；
- 读到摘要不符时，在线删除该单一损坏对象并重取；离线只报错；
- 不以 FNV、URL、mtime 或绝对路径作为可信身份；
- 热 sync 只验证 lock/resolution 摘要和对象 marker，不重复解包。

首个闭环不做自动 GC。GC 需要项目引用追踪和并发删除语义，应在有实际磁盘数据后单独设计。

## 6. `resolution-v1` 合同

概念形状：

```json
{
  "schema_version": 1,
  "manifest_sha256": "<hex>",
  "lock_sha256": "<hex>",
  "roots": ["workspace:acme/app@1.0.0"],
  "packages": [
    {
      "id": "registry:acme/base@1.2.0#<digest>",
      "root": "<absolute-local-source-root>",
      "content_sha256": "<registry-digest-or-null>",
      "dependencies": [
        {"alias": "util", "package_id": "registry:acme/util@1.1.0#<digest>"}
      ]
    }
  ]
}
```

规则：

- 项目本地生成，不提交版本库；
- roots/packages/dependencies 都 canonical 排序；
- registry package 的 `content_sha256` 是已验证制品 SHA-256；mutable path/workspace package
  明确为 JSON `null`，不得用 manifest hash 冒充源码内容摘要；
- Styio 在读取时比对 manifest/lock 摘要，stale 即失败并提示运行 `pafio sync`；
- Styio 不读取 Pafio cache index、registry metadata 或 lock 私有扩展；
- Pafio 不在合同中选择 target/profile/compiler，也不分配构建输出目录。

## 7. 收敛顺序

### 闭环 A — 消费者薄核心（当前唯一计划节点）

完成 `sync` 事务、verified CAS 缺口和 `resolution-v1`。只跑 manifest/lock/resolve/sync/security
相关回归。成功标准是包图可复现且可以由一个最小 Styio 消费者读取。

### 闭环 B — Styio 所有权切换

在同一个迁移中让 Styio 消费 `resolution-v1` 并接管 build/check/run/test，然后删除 Pafio 的
Workflow/Plan/Toolchain/Cloud/ProjectGraph 命令、目标、合同、文档和测试。迁移完成前不发布双
入口；完成后用一次性脚本/`rg` 证明旧实现不存在。

### 闭环 C — 最小生产者

仅实现 deterministic pack + authenticated publish client；registry 服务端、签名基础设施和
运维仍在独立服务。pack 的输出必须与消费者 CAS 输入完全同构。

### 闭环 D — 真实数据驱动的版本策略

先用 registry corpus 验证 MVS 的兼容前提。成立则一次性从 `single-version-v1` 迁移到
major-aware MVS；不成立才设计 PubGrub。新 resolver 必须替换旧实现和旧合同，不保留双策略。

### 闭环 E — 公共命名切换

如需从遗留 `spio` 全面切到 Pafio，单独一次迁移 binary、manifest、lock、env、目录、schema ID、
文档和测试；不提供永久 alias。该闭环不与算法或所有权迁移混做。

## 8. 停止条件

完成闭环 A 后立即停止，不自动进入 B。只有 A 的聚焦回归和独立审计通过，才由主流程依据用户
最新意图决定是否启动下一项。
