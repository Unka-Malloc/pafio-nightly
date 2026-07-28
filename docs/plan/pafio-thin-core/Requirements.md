# Pafio 薄包管理核心 — 需求

**Plan:** `pafio-thin-core`  
**Last updated:** 2026-07-28

## 1. 产品定位

Pafio 只负责把“包依赖意图”变成“经过校验、可复现、可被 Styio 消费的本地包图”：

```text
manifest + existing lock + registry metadata
    -> resolve
    -> verify/materialize
    -> canonical lock + local resolution map
```

这条流水线之外的事情不属于 Pafio 核心。Pafio 不编译 Styio，不运行测试，不安装或切换
Styio 编译器，不调度云任务，不承载 registry 服务端，也不为某个 IDE 生成产品专用状态。

薄并不等于省略正确性。解析确定性、制品完整性、原子写入、并发互斥和离线复现都是包管理
本身的职责，必须保留。

## 2. 第一个可独立验收的用户能力

给定一个合法项目 manifest，执行一次 `sync` 后：

1. 依赖图被确定性解析；
2. lockfile 记录全部直接和传递依赖的精确版本、来源与内容摘要；
3. 缺失制品被下载、校验并放入不可变内容寻址存储；
4. 项目本地生成一个最小 `resolution-v1` 文件，把包 ID/别名映射到本机源码根；registry 包携带
   已验证内容摘要，本地可变包显式使用 `null`；
5. 再次执行时复用 lockfile 与缓存；`--locked --offline` 不访问网络且不修改状态；
6. Styio 只消费 `resolution-v1`，不推断 registry、cache 或 solver 状态。

首个闭环沿用当前实现中的 v1 文件名和精确版本策略，避免把品牌改名、manifest 合并和新求解器
同时塞进一个交付。若执行 Pafio 公共命名迁移，必须在独立闭环中一次性完成，不能保留双命令、
双环境变量或双文件名兼容层。

## 3. 功能需求

### REQ-THIN-001 — 单一 `sync` 闭环

`sync` 是核心写操作，负责校验 manifest、复用或生成 lock、获取缺失制品、物化只读源码树并
原子生成 `resolution-v1`。`add` 和 `remove` 只是 manifest 编辑后调用同一事务；不得拥有第二套
解析或获取逻辑。

**验收：** path 与 registry 依赖的混合传递图一次成功；prepare 失败时旧 lock/resolution
字节不变。commit 中 lock 与 resolution 各自保持单文件原子，resolution 最后提交；进程在两次
提交之间中断时，旧 resolution 必须因 lock 摘要失配而被消费者拒绝，不能出现可被接受的撕裂
状态。

### REQ-THIN-002 — Lock-first 的确定性精确版本解析

首个闭环保留 `single-version-v1` 精确版本策略和每个包名一个有效版本/来源的约束。已有 lock
中仍满足 manifest 的包必须优先复用；只有新增、删除或显式变化的边允许改变。图遍历、冲突路径、
lock 序列化和 resolution 序列化必须稳定。

**验收：** 同一输入重复执行两次产生字节相同的 lock/resolution；菱形冲突与循环错误包含完整
依赖链；解析复杂度按已访问图保持 `O(V + E)`。

### REQ-THIN-003 — 经校验的内容寻址存储

registry 源制品以 SHA-256 为唯一身份。临时下载、摘要校验、安全解包、只读物化、同目录原子
rename 和每摘要互斥必须组成一个提交事务。项目不得复制第二份包内容；resolution 文件直接指向
缓存中的不可变源码根。

**验收：** 并发 `sync` 对相同摘要只产生一个完整对象；损坏对象在线模式重新获取，离线模式以
稳定错误失败；任何失败均不留下可被读取为成功对象的半成品。

### REQ-THIN-004 — Styio `resolution-v1` 最小合同

生成文件只包含编译器解析包导入所需的事实：

- schema 版本；
- manifest 与 lock 的内容摘要；
- 根包 ID；
- 按包 ID 排序的包列表；
- 每个包的本机源码根、registry 制品内容摘要（path/workspace 为 JSON `null`）与按别名排序的
  依赖映射。

合同不得包含 target、profile、toolchain、cloud policy、IDE confidence、构建目录、收据或命令
执行结果。绝对路径只能出现在项目本地生成且不提交版本库的 resolution 文件中；lockfile 不得
写入绝对路径。

**验收：** schema 正负用例通过；manifest 或 lock 改变后旧 resolution 被判定 stale；测试用
Styio 消费者只凭该文件即可解析一个传递依赖别名。

### REQ-THIN-005 — 严格产品边界

第一个闭环不得新增 build/run/test/check、toolchain、cloud、IDE、registry server 或 hosted
control-plane 行为。现有宽表面只作为后续一次性所有权迁移的待移除代码，不再扩展。

**验收：** 新核心模块的依赖图不依赖 `SpioCloud`、`SpioTool`、`SpioToolchain`、`SpioPlan`、
`WorkflowApp` 或 `ProjectGraphContract`；`resolution-v1` schema 中没有这些领域字段。

## 4. 非功能约束

- **可复现：** canonical 输出不含时间戳、随机 ID 或主机身份。
- **热路径：** lock 与 CAS 命中时不得访问网络、重复解包或重复扫描未变化的包图。
- **并发：** 网络/解包采用有界队列；同一内容摘要使用 single-flight，独立摘要可并行。
- **内存：** 图只保存 manifest/索引摘要和邻接关系，不把制品内容驻留内存。
- **安全：** 继续复用现有 TUF、摘要校验、归档预扫描、原子写和文件锁实现；安全能力不是可删
  的“附加功能”。
- **错误：** 文本和 JSON 诊断共享稳定分类；不得解析子进程自然语言来决定领域结果。

## 5. 明确非目标

首个闭环不实现：

- semver 区间、PubGrub/SAT、features 或 target 条件依赖；
- Git 作为新的公共依赖来源扩展（现有 Git 路径不再演进，后续收敛为 path + registry）；
- workspace selector、changed/affected package、搜索和推荐；
- vendor 导出协议、cache 管理命令或自动 GC；
- pack/publish 生产者流程；
- 编译、运行、测试、编译器安装/切换、云执行和 IDE 专用载荷；
- Pafio/Spio 品牌与文件名迁移。

## 6. 求解器演进门槛

精确版本不是长期生态承诺，而是最快的可靠闭环。只有同时满足以下条件才启动下一求解器闭环：

1. registry 中存在真实的多版本传递依赖图；
2. 精确 pin 已出现可复现的版本锁定问题；
3. Styio 决定 major-version package identity 与兼容规则；
4. 有公开 corpus 可比较候选算法的解质量、诊断和性能。

默认候选是 Go 风格的 Minimal Version Selection：每条边声明最低版本，major 版本进入包身份，
用单调选择避免 NP-hard 回溯。若 Styio 无法承诺这一兼容模型，再单独评估 PubGrub；不得在首个
闭环中先写一个不完整的范围求解器。

## 7. 最终边界目标

收敛后的常用消费者命令最多保留：

```text
pafio add
pafio remove
pafio sync
pafio tree
```

`pack/publish` 是消费者闭环稳定后的第二个独立能力。`fetch` 与 `lock` 被 `sync` 吸收；其余现有
命令按 `Architecture.md` 的所有权迁移表一次性移出或删除。
