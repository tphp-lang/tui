# Tui 扩展重构计划：从单一 App 类拆分为按域分类

> 状态：**未开始实施**（计划已评审，代码零改动）
> 创建：2026-11-16
> 目标版本：0.2.0

## 0. 背景与结论

`Tphp\Tui\App` 目前平铺 314 个方法，PHP 层臃肿。C 层其实早已按域拆好
（`tui.c` 17 / `tui_widgets.c` 37 / `tui_form.c` 33 / `tui_layout.c` 120 /
`tui_charts.c` 21 / `tui_content.c` 66 / `tui_app.c` 20），
臃肿只存在于「全部注册到同一个类」。

**原始基线方法数口径**：314 = 313 个 `tui_cls_add_native` 普通方法 + 1 个
`__construct`（经 `tui_cls_add_ctor` 注册）。§3「规划目标」表格的「方法数」只计
普通方法，合计 313 + ctor = 314。**阶段 B 实施后实际为 319**（App + 5 访问器），
见 §3「实际实施」与附录——本口径仅描述拆分前的基线。

**选定的方案**（在四个候选方案中最终采用）：

```php
$app  = new App("Hello", 420, 300);
$col  = $app->layout(LayoutType::Column);
$form = $app->form();

while ($app->frame()) {
    $col->begin(32, 16);
    $name = $form->input("name", $name, 128, ["label" => "用户名"]);
    $col->end();
}
```

**为什么选它**：唯一同时满足「不用闭包」「对象只建一次」「begin/end 同源」的方案。

**实施策略**：分 A/B/C 三阶段（详见 §5）。A = bug 修复（SCOPE_IF 失配检测，
无 API 破坏，**已完成**）；B = 类拆分（含兼容层、TLS、引用计数纪律，有破坏性）择机；
C = 文档跟随 B。三阶段各自独立可交付、独立验证，A 不被 B 阻塞。

已否决的方案及原因：

| 方案 | 否决原因 |
|---|---|
| 纯静态类（`App::frame()`） | 多窗口时「当前窗口」歧义（`smoke.php` 有 2 个窗口同时存活） |
| 闭包容器（`$app->column(..., fn())`） | 需引擎新增「C 回调 PHP 闭包」通道，扩展层从未有此能力 |
| 按域平级拆（`Column::begin` + `App::boxEnd`） | 闭合对不上，CUI 节点栈不校验来源，静默错位 |

## 1. 目标与成功标准

1. `App` 只剩窗口生命周期、文本原语、主题、字体、截图、测试面（31 个方法）
2. 容器 begin/end 全部由 `Layout` 提供，且能检测失配（抛异常，不静默错位）
3. 全部 7 个冒烟测试通过；5 个示例改写后行为不变
4. `doc/API.md` 按现有模板（`## namespace` / `### class` / 逐方法表）重写
5. 引擎主套件仍 29 passed

## 2. 已核实的技术事实（决定了设计）

| 事实 | 位置 | 影响 |
|---|---|---|
| `cui__open/close` 是纯栈操作 | `cui/src/cui_layout.c:42-49` | 自维护配平计数可行 |
| `cui_box_end` 对 scroll 节点特判 | `cui/src/cui_layout.c:189-193` | `Layout::end()` 必须按类型分派 |
| `ui->parent_depth` 是私有字段 | `cui/src/cui_internal.h:473` | 不能依赖，改用自有计数 |
| tui 至今只 include 公开 `cui_app.h` | 全部 `src/` | 不引入 CUI 私有头 |
| `tphp_obj_alloc(cls)` 创建对象 | `TinyPHP.rt/src/class.h:93` | 子对象可从 C 侧返回 |
| `tui_need_ui` 经 `props["app"]` 取窗口 | `src/tui_util.c:141` | 子对象持同样句柄即可 |
| 文件级 `g_*` 静态（**2 个文件、10 组**）→ **已迁 per-App state** | 原 `tui_layout.c`/`tui_content.c` 静态 → 现 `TuiLayoutState`/`TuiContentState` struct 存域对象 state 槽（见 §5 B.10） | 同时解决线程隔离与同线程多窗口交错（各 App 的域子对象各持 state） |
| 6 个类各有/可复用注册函数 | `tui_*.c` 末尾（`tui.c` 的 App 注册已存在） | 改造点收敛；5 个域类复用现有注册函数，`App` 用现有 `tui_register_app_class` |

## 3. 类划分

### 规划目标（理想终态，未完全达成——见下方「实际实施」）

| 类 | 内容 | 方法数 |
|---|---|---|
| `App` | 窗口生命周期 5 + 文本原语 6 + `tui_app.c` 20 | 31 |
| `Layout` | 全部容器/覆盖层/导航/表格骨架 | 123 |
| `Form` | 输入、数值、勾选、日期时间 | 33 |
| `Widgets` | 显示小件、按钮组、文本特效 | 39 |
| `Charts` | 21 种图表 | 21 |
| `Content` | 富文本、编辑器、聊天、节点、devtools | 66 |
| **合计** | 6 个类；普通方法 313 + `__construct` = 314 | 313 |

### 实际实施（阶段 B 已完成，2026-11-16）

采用**「方法真正迁移到域类 + 测试/示例改用新 API」**策略（用户明确否决双 API 并存）：

| 类 | 实际业务方法数 | 构成 |
|---|---|---|
| `App` | 36 | 窗口生命周期 5 + 文本原语 6 + `tui_app.c` 20 + 域访问器 5（另 +1 ctor） |
| `Layout` | 123 | `tui_layout.c` 120 + `tui.c` 桥接 3（columnBegin/rowBegin/boxEnd） |
| `Form` | 33 | `tui_form.c` |
| `Widgets` | 39 | `tui_widgets.c` 37 + `tui.c` 桥接 2（button/toggle） |
| `Charts` | 21 | `tui_charts.c` |
| `Content` | 66 | `tui_content.c` |

- **App 已精简**：283 个域方法从 App 移除，实测 `$app->checkbox()`/`$app->lineChart()`
  抛 Error（不再存在），`$app->form()->checkbox()`/`$app->charts()->lineChart()` 可用。
- 所有测试（10 个）与 5 个示例已改用新 API；静态 add_native 总数 319（每个方法只注册一次，
  不再重复挂 App）。
- `tui.c` 的 5 个方法（3 容器 + 2 控件）实现留在 tui.c，经 bridge 注册函数挂到域类
  （`tui_register_core_bridge_layout/widgets`）——impl 复用，但**注册位置**已按语义归域。
- `tui_app.c` 的 20 个（主题/字体/截图/测试面）留在 App，无独立域类。

**否决的替代方案**：
1. 283 个静态转发 stub 保留旧 API = 臃肿转移、出错面大（用户否决双 API 并存）；
2. `__call` 动态转发 = 违反严格 AOT「拒绝动态分发」约束。

**为何不设 `Core` 类**：6 个文本原语单独成类收益极小，且 §0/README 本就是 6 类。

## 4. `LayoutType` 枚举（backed int）

`Column`=0, `Row`=1, `Box`=2, `Scroll`=3, `Split`=4

只覆盖可用同一 `begin/end` 表达的 5 种。`card`/`table`/`dialog`/`tabs` 等
有返回值或需额外参数者，保留为 `Layout` 的独立命名方法。

### `Layout::begin()` 参数契约

| LayoutType | 参数 |
|---|---|
| `Column` | `int $pad = 0, int $gap = 0` |
| `Row` | `int $gap = 0` |
| `Box` | `int $pad = 0, int $gap = 0` |
| `Scroll` | `string $id = "", bool $horizontal = false` |
| `Split` | `string $id, float $ratio` |

参数数量超出该类型所需 → 抛 `ArgumentCountError`（不静默忽略）。

## 5. 实施步骤

三阶段按**价值密度**排序，各阶段可独立交付、独立验证：

| 阶段 | 内容 | API 破坏 | 独立价值 |
|---|---|---|---|
| **A** | bug 修复：SCOPE_IF 失配检测 | 无 | 消灭随机 AV（曾 16-20% 复现）——**已完成** |
| **B** | 类拆分：314 方法 → 6 类 + 域访问器 + TLS + 引用纪律 | 有（含兼容层） | API 组织清晰 |
| **C** | 文档重写 | 无 | 与最终形态对齐 |

A 不被 B 阻塞——已先交付 A 拿到真 bug 修复，B 可视需要择机启动。

### 阶段 A：bug 修复（无 API 破坏，可独立交付）—— 已完成

- [x] **A.1** SCOPE_IF 14 对失配检测（清单见 `doc/coverage.md` §3）：
      `tui_util.c` 汇总 + 各域导出 `unclosed`/`clear`，`nm_app_frame` 渲染前检查，
      残留则抛 `Error: <name>Begin() without matching End()` 并清标记（catch 后可恢复）；
      `destroy()` 清残留防污染下一实例。**复用现有 `g_*_on` 标记，未建第二套计数**。
      `tests/smoke-scopeif.php` 专测通过（漏 End 抛错/配对名/可恢复/关态无误报），全套 8/8 全绿。

> **A.2 / A.3 已移入阶段 B**（本次评审决定，2026-11-16）：
> - **TLS 化（原 A.2）→ 已在 B.9 完成**。决策曾反转：初判「tui 主线程阻塞式、
>   无多线程路径、暂不做」；**后续深查推翻**——引擎确实支持 worker 多线程跑 PHP
>  （`ext/worker/worker.c:406` `tphp_vm_run_closure`），且核心大量 `_Thread_local`
>  （class.c 类表 / mem.c GC 链表 / vm.c 当前 VM）；CUI 本体无进程级全局状态
>   （状态都在 `cui_ctx`），故 tui 封装层的 10 组 `static g_*` 是**唯一的跨线程共享点**
>  ——当用户在 worker 线程各建 App 时构成真实数据竞争，非纯预防。遂在 B.9 全部补
>  `_Thread_local`（tui_layout.c 7 组 + tui_content.c 3 组），实测编译通过、10/10 + 引擎 29 全绿。
> - **引用计数纪律（原 A.3）**：仅在 B 建子对象时才有落地点，届时随 B 实现；
>   核验要点已记入 §7 风险表（alloc rc=1 归返回值 / 多存一处各加 1 / dtor 改写槽为 NULL）。

### 阶段 B：类拆分（已完成，App 真正精简）

> 用户明确否决双 API 并存，故按「迁移 + 测试改写」完成，非早期的「App 保留全部」增量方案。

- [x] **B.1** C 侧基础设施：`tui_cls_new`/`tui_cls_add_ctor`/`tui_cls_add_prop` 迁至
      `tui_util.c/.h`；`tui_make_child`（复制 app 句柄 + rc 转移）、
      `tui_prop_int`/`tui_prop_set_int`、`tui_depth_inc`/`tui_depth_dec` 均已实现。
- [x] **B.2** `LayoutType` 枚举（5 case）已注册，`TUI_LC_LAYOUT_TYPE` 宏已加。
- [x] **B.3** 注册 6 个类：App（36 业务方法 + ctor）+ Layout 123 / Form 33 /
      Widgets 39 / Charts 21 / Content 66。静态 add_native 总数 319，每方法只注册一次。
- [x] **B.4** `Layout` 的 `depth` begin/end 配平检测：Layout 加 `depth` 私有槽，
      5 种容器 begin 自增、end 自减并校验；`frame()` 入口检查上一段 depth 归零，
      失配抛 Error 并提示「window state corrupted; destroy and recreate」（CUI node 栈
      已失衡、不可恢复）。**注意**：CUI 每帧重置 node 栈（`cui_core.c:788-790`），
      故此项是把难查的视觉错乱变显式错误，非 bug 修复。`tests/smoke-depth.php` 专测通过。
- [x] **B.5** `App` 域访问器 `layout(LayoutType)/form()/widgets()/charts()/content()`
      带缓存（5 个 dom 槽）+ `destroy`/`dtor` 释放（§7 纪律）。`smoke-domain.php` 通过。
- [x] **B.7** 7 个冒烟 + 5 个示例（hello/todo/form/dashboard/markdown）全部改写为新 API；
      自动改写脚本按「方法注册文件→域」映射，0 残留旧 API 调用（已扫描验证）。
- [x] **B.8** 精简 `App`：283 个域方法从 App 移除。实测 `$app->checkbox()` 抛 Error、
      `$app->form()->checkbox()` 可用、App 核心 frame/quit 正常。用户否决双 API 并存，
      未采用转发 stub；`__call` 因违反严格 AOT 被排除。
- [x] **B.9** 10 组文件级静态补 `_Thread_local`（决策反转后完成，理由见上方 A.2 说明）：
      `tui_layout.c`（split_ratio / 13 个 `g_*_on` / reorder / swipe×3 / tcols×2 /
      gcols×2 / tnodes）与 `tui_content.c`（terminal×4 / log×4 / nodeed_on）。
      实测编译通过、10/10 冒烟 + 引擎 29 passed。
- [x] **B.10** 10 组静态整体迁入 **per-App state**（根治同线程多窗口串扰）：
      - `tui_util.h` 定义 `TuiLayoutState`（SCOPE_IF 标记/借阅数组）与
        `TuiContentState`（terminal/log/nodeEditor）；域对象加 `state` 私有槽存 struct 指针，
        `tui_state_get` 懒分配、`tui_state_free` 在域类 `native_dtor` 释放内部数组与 struct
        （terminal/log 的 malloc 缓冲也随此释放，修掉原本进程级单例的泄漏点）。
      - 所有域方法经 `GSTATE` 宏从 `self`（域对象）取 state，不再读文件级静态。
        ScopeIf 检查/清理函数改接收域对象，从 App 的 dom0/dom4 槽取。
      - 实测：编译通过；`tests/smoke-multiwindow.php` 验证 App1 的 dialog 残留不污染
        App2、且 App1 仍能检出自己的残留、两窗口 terminal 缓冲独立；
        `smoke-multiwindow.php` 通过后全套 11/11 冒烟 + 引擎 29 passed，0 回归。
      - 边界：CUI 的 `frame()` 第二次起阻塞等事件，**交错 frame 两个窗口**需输入注入、
        属平台事件循环范畴，未在本测覆盖；本测验证的是状态隔离本身。

### 阶段 C：文档重写（跟随 B 完成）

- [x] **C.1** 重写 `doc/API.md`（6 个类的逐方法表 + 3 个枚举，已按方法注册文件自动分组）
- [x] **C.2** 更新 `README.md` 与 `doc/coverage.md`（反映 6 类 API 与新调用形态）

## 6. 验证清单

| 阶段 | 检查项 | 方法 | 通过标准 |
|---|---|---|---|
| A/B/C | 编译 | `clang -c` 每个改动文件 | 零 warning（已实测重链成功） |
| A/B/C | 冒烟 | `tests\run.ps1` | **11/11 PASS**（原 7 + scopeif + domain + depth + multiwindow，已实测） |
| A/B/C | 引擎 | `tphp.exe test tests\lib` | 29 passed |
| A/B/C | 编译真实性 | 重链后确认无 `compile failed` | **曾踩坑**：早期编译失败但输出被吞，跑的是旧二进制——现已核对重链输出 |
| **A** | SCOPE_IF 漏 End（14 对） | `tests/smoke-scopeif.php` | 抛 `Error` 含配对名；关态无误报。**已实现** |
| **B** | 域访问器 + 新 API | `tests/smoke-domain.php` | 6 类 + LayoutType；访问器类型/方法/缓存正确。**已实现** |
| **B** | 类注册 | 静态统计 + 运行时探针 | 6 类；App 36 业务方法+ctor、Layout 123/Form 33/Widgets 39/Charts 21/Content 66；add_native 总 319（每方法一次） |
| **B** | 容器失配（LayoutType 5 种） | `tests/smoke-depth.php` | 漏 End/多 End 抛 Error、配对无误报。**已实现** |
| **B** | App 精简 | 运行时探针 | `$app->checkbox()` 抛 Error、`$app->form()->checkbox()` 可用。**已实测** |
| **B** | 多窗口状态隔离 | `tests/smoke-multiwindow.php` | A 残留不污染 B、A 仍检出自身残留、两窗口 terminal 缓冲独立。**已实现** |
| A/B/C | CJK 回归 | `smoke.php` 断行断言 | 仍 PASS（11/11 含此） |

## 7. 风险与对策

| 风险 | 对策 |
|---|---|
| 缓存槽与 props 布局冲突 | 按名查找（`tphp_class_find_prop`），不硬编码槽号 |
| 子对象在 `destroy()` 后使用 | `tui_need_ui` 已有 "window is already destroyed" 抛错 |
| `Layout::end()` 对 scroll 误调 `cui_box_end` | 按 `LayoutType` 分派，补专测 |
| 注册语句迁移易漏 | 283 方法从 App 摘除、5 个用 bridge 挂域类；实测 add_native 319、11/11 全绿 |
| 跨 Layout 实例误配检测不到 | 记为已知限制；每实例独立 depth 计数已覆盖主要误用 |
| **编译失败被吞 → 跑旧二进制假绿** | 曾踩坑：PowerShell 管道把 `compile failed` 当普通错误流，误判成功。
  对策：重链后必须确认输出含 `Linked N ext package(s)` 且无 `compile failed`，
  再用新探针验证新行为（如 `$app->checkbox()` 抛错） |
| SCOPE_IF 漏 End 致随机 AV | 用现有 `g_*_on` 标记统一做失配计数（§5.A.1），补 14 对专测 |
| Layout 漏 End 视觉错乱 | depth 配平检测（§5.B.4）；失配即报「window corrupted」，因 CUI node 栈失衡不可恢复，
  须销毁重建实例；专测每场景独立 App |
| 建子对象时引用计数错配致 UAF | alloc rc=1 归 native 返回值、不再 addref（`vm.c:1587-1590`）；多存一处各加 1（反面 `worker.c:693-700`）；写 props 用 `addref(new)/release(old)/slot=new`；native_dtor 手动 release 后须改写槽为 NULL（`tui.c:50-54`） |
| ~~同线程多窗口状态串扰~~ **已解决（B.10）** | 10 组静态迁入 per-App state（域对象私有槽），
  各 App 的 Layout/Content 子对象各持一份。`smoke-multiwindow.php` 验证残留隔离与缓冲独立。
  遗留边界：交错 frame 两窗口需输入注入唤醒 `cui_app_frame`，属平台事件循环范畴，未覆盖 |
| state struct 释放时机 | 域对象 `native_dtor` 调 `tui_state_free`：先 `fn(state)` 释放内部数组
  （reorder/swipe/cols/term/log），再 free struct 并置槽 0 防二次释放；
  正常配对下 End 已回收数组，dtor 仅兜底异常中断/漏 End 的泄漏 |

## 8. 假设

1. 不引入 CUI 私有头；校验用自有计数
2. `App` 仍是唯一窗口入口，域对象经 `$app->form()` 派生（非 `new Form()`），
   避免多窗口歧义
3. `LayoutType` 只覆盖 5 种可统一 begin/end 的容器；其余容器走独立命名方法，
   但 SCOPE_IF 14 对仍须纳入失配检测（§5.A.1）
4. 阶段 B 有破坏性（旧 API 已移除，测试/示例必须同步迁移）；若只要 bug 修复，
   交付阶段 A 即可停止
5. 状态已 per-App 隔离（B.10），并行多窗口不再串扰；但**交错 frame**
   （两个窗口交替调用 `frame()`）受 CUI 事件循环阻塞限制，不在本版范围（§7）
6. 域方法实现可留在原 C 文件（tui.c 的 5 个桥接方法），仅**注册位置**按语义归域；
   不要求物理迁移 .c 文件

## 9. 不做

- 不改 CUI 本体（除 `cui/src/cui_text.c` 已有的 CJK 改动）
- 不改引擎 `TinyPHP.rt`
- 不引入闭包回调（需引擎新增通道）

## 10. 恢复工作时的入口

1. 读本文件 §5 的勾选框，确认当前处于 A/B/C 哪个阶段、进度到哪一项
2. 重新构建基线：
   ```powershell
   # cwd = tui 包根
   powershell -ExecutionPolicy Bypass -File tests\run.ps1
   # 期望 11/11 PASS（原 7 + scopeif + domain + depth + multiwindow 探针）
   ```
3. 若 `build/tphp.exe` 与源码不同步，先重链：
   ```powershell
   cd ..\TinyPHP.rt\build
   $env:PATH = 'C:\env\msys2\clang64\bin;' + $env:PATH
   .\tphp.exe ext --add C:\project\tphp\tui
   # 必须确认输出含 "Linked N ext package(s)" 且无 "compile failed"！
   # 曾踩坑：编译错误被 PowerShell 当普通错误流吞掉，跑的是旧二进制假绿。
   ```
4. 从第一个未勾选项继续

## 附：当前基线（2026-11-16）

- **阶段 B 后（终态）**：6 类，App 已精简为 36 业务方法+ctor
  （生命周期 5 + 文本原语 6 + app层 20 + 访问器 5）；Layout 123（含桥接 3）、
  Form 33、Widgets 39（含桥接 2）、Charts 21、Content 66。总 add_native 319（每方法一次）。
  原始基线是 App 314 方法。旧 API 已从 App 移除，测试/示例全部改用新 API。
- 三个枚举：`ButtonVariant`(7)、`FontRole`(5)、`LayoutType`(5)
- 冒烟 **11/11 PASS**（原 7 + scopeif + domain + depth + multiwindow），引擎 29 passed
- 已有 CJK 支持：`fontLoad`/`fontLoadSystem`/`fontSetRole`/`fontAddFallback`
  + CUI 断行 CJK 处理（`cui_text.c`）
- 阶段 C 已完成：README.md / API.md（6 类分组 + 3 枚举）/ coverage.md 均已更新到终态
- B.9 TLS 已完成（决策反转理由见 §5 A.2）；B.10 又把 10 组静态迁入 per-App state，
  线程与多窗口隔离一并解决，§7 的串扰限制已消除
- 全部计划项（A/B.1-B.10/C）已完成；剩余仅"交错 frame 需事件注入"的平台边界
