# Tui 扩展重构计划：从单一 App 类拆分为按域分类

> 状态：**未开始实施**（计划已评审，代码零改动）
> 创建：2026-11-16
> 目标版本：0.2.0

## 0. 背景与结论

`Tphp\Tui\App` 目前平铺 314 个方法，PHP 层臃肿。C 层其实早已按域拆好
（`tui.c` 17 / `tui_widgets.c` 37 / `tui_form.c` 33 / `tui_layout.c` 120 /
`tui_charts.c` 21 / `tui_content.c` 66 / `tui_app.c` 20），
臃肿只存在于「全部注册到同一个类」。

**方法数口径（全文统一）**：314 = 313 个 `tui_cls_add_native` 普通方法 + 1 个
`__construct`（经 `tui_cls_add_ctor` 注册）。§3 表格的「方法数」只计普通方法，
合计 313；探针验收按 313 + 1 = 314 核对，勿漏 ctor。

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
| 文件级 `g_*` 静态无 TLS（**2 个文件、10 组**） | `tui_layout.c:115-141`（7 组）/ `tui_content.c:304-312,886`（3 组） | 补 `_Thread_local` 只解决**线程**隔离，**不**解决同线程多窗口交错（见 §7） |
| 6 个类各有/可复用注册函数 | `tui_*.c` 末尾（`tui.c` 的 App 注册已存在） | 改造点收敛；5 个域类复用现有注册函数，`App` 用现有 `tui_register_app_class` |

## 3. 类划分

按 C 域文件归属。`tui.c` 的 16 个普通方法按语义拆分：
容器 3（columnBegin/rowBegin/boxEnd）→ `Layout`；控件 2（button/toggle）→ `Widgets`；
文本原语 6（heading/secondary/text/label/spacer/separator）**留 `App`**（不设 Core 类，
见下方说明）；窗口生命周期 5 留 `App`。`tui_app.c` 20 归 `App`。

| 类 | 内容 | 方法数 |
|---|---|---|
| `App` | 窗口生命周期 5 + 文本原语 6 + `tui_app.c` 20 | 31 |
| `Layout` | `begin()/end()` + 全部容器/覆盖层/导航/表格骨架（`tui_layout.c` 120 + `tui.c` 容器 3） | 123 |
| `Form` | 输入、数值、勾选、日期时间 | 33 |
| `Widgets` | 显示小件、按钮组、文本特效（`tui_widgets.c` 37 + `tui.c` 控件 2） | 39 |
| `Charts` | 21 种图表 | 21 |
| `Content` | 富文本、编辑器、聊天、节点、devtools | 66 |
| **合计** | 6 个类（App + 5 域类）；普通方法 313，加 `__construct` = **314** | **313** |

**为何不设 `Core` 类**：6 个文本原语单独成类收益极小，却要多一个 `tphp_obj_alloc`
子对象、一个私有 props 槽、一个 `core()` 访问器和一处注册函数；且 §0 示例与
README「App/Layout/Form/Widgets/Charts/Content」本就是 6 类，无 Core。留 `App`
最简。`App`=31 略高于 §1 的「约 30」，可接受（多出的 6 个是 1 行转发的文本原语）。

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
> - **TLS 化（原 A.2）**：核验确认 tui 是主线程阻塞式 API（README「单线程」），
>   CUI 的 `cui__thread_*` 只是聊天绘制样式而非线程模型，引擎 worker.c 不引用 tui——
>   **无真实多线程调用路径**，给 10 组静态补 `_Thread_local` 属预防性优化，暂不做。
>   若 B 阶段引入真正的多线程使用，再随 B 落地，并逐项核对 10 组变量。
> - **引用计数纪律（原 A.3）**：仅在 B 建子对象时才有落地点，届时随 B 实现；
>   核验要点已记入 §7 风险表（alloc rc=1 归返回值 / 多存一处各加 1 / dtor 改写槽为 NULL）。

### 阶段 B：类拆分（有 API 破坏，含兼容层）

- [ ] **B.1** C 侧基础设施：`tui_util.h`/`.c` 新增
      `TObj *tui_make_child(VM*, TObj *parent, TClass *cls)`（照 §7 引用计数纪律：
      `tphp_obj_alloc` 建，复制父 `app` 属性值，addref 配对）；
      新增 `int tui_prop_int(TObj*, const char *name)` /
      `void tui_prop_set_int(...)`（按名读写 int 私有槽，走 `tphp_class_find_prop`）
- [ ] **B.2** `LayoutType` 枚举：复用 `tui_register_enum`（`src/tui.c:283`），
      新增 `TUI_LC_LAYOUT_TYPE` 到 `src/tui_util.h`
- [ ] **B.3** `tphp_ext_tphp_tui_init` 注册 **6 个类**（与 §0/README 一致）：
      `tui_register_app_class()`（现有，保留 31 方法）+ 5 个域类：
      `tui_register_layout_class()` / `tui_register_form_class()` /
      `tui_register_widgets_class()` / `tui_register_charts_class()` /
      `tui_register_content_class()`
      - 各域注册内部：`tui_cls_new(名)` → `tui_cls_add_prop(cls,"app")` →
        `tui_cls_add_prop(cls,"depth")`（Layout 专用）→ 现有 `tui_cls_add_native` 不变 →
        `tphp_class_register_compile(cls)`
      - **现有 314 处 `tui_cls_add_native` 调用逐字不动**（= 313 普通方法 + 1 个
        `__construct`），只改挂载的 `cls`；迁移后 diff 总数须仍为 314
- [ ] **B.4** 方法归属调整（与 §3 表格一致）：`columnBegin`/`rowBegin`/`boxEnd`
      （3 个）从 `tui.c` 迁到 `Layout` 注册；`button`/`toggle`（2 个）迁到 `Widgets`；
      文本原语 6 个留 `App`（不设 Core 类，理由见 §3）。
      `Layout` 加私有槽 `depth` + begin/end 配平检测（LayoutType 5 种）：
      begin 校验参数数量 → 开容器 → `depth++`；end `depth==0` 抛
      `Error: Layout::end() without matching begin()`，否则 `depth--` 并按
      `LayoutType` 分派到 `cui_box_end`/`cui_scroll_end`/`cui_split_end`；
      `App::frame()` 收尾检查已缓存 Layout 实例，任一 `depth!=0` 抛
      `Error: Layout::begin() without matching end()`。
      已知限制：只保证同一 Layout 实例配平，跨实例误配检测不到。
- [ ] **B.5** `App` 增加域访问器（带缓存）：
      `layout(LayoutType): Layout`、`form(): Form`、`widgets(): Widgets`、
      `charts(): Charts`、`content(): Content`
      - `App` 新增私有槽保存已创建实例（TObj 引用），首次创建并缓存，
        后续返回同一实例（避免每帧分配，也让 depth 计数跨帧配平）
      - `destroy()` 时释放这些引用（照 §7 引用计数纪律，避免二次 release）
- [ ] **B.6** `App` 保留全部旧方法作为薄转发，标记 `@deprecated`。
      现有 7 个冒烟 + 5 个示例**零改动先跑绿**（关键中途安全网）
- [ ] **B.7** 改写示例（hello → todo → form → dashboard → markdown）与 `tests/` 到新 API
- [ ] **B.8** 移除 `App` 上的转发方法，完成最终形态
- [ ] **B.9**（条件项）若 B 引入真正多线程使用，再给 10 组文件级静态补 `_Thread_local`
      （2 文件：`tui_layout.c` 的 `g_split_ratio`/`g_*_on`(13)/`g_reorder`/`g_swipe_*`/
      `g_tcols`/`g_gcols`/`g_tnodes`，`tui_content.c` 的 `g_term_*`/`g_log_*`/`g_nodeed_on`）；
      单线程下无触发场景，不做（理由见 §5 阶段 A 的 A.2 移出说明）

### 阶段 C：文档重写（跟随 B 完成）

- [ ] **C.1** 重写 `doc/API.md`（6 个类的逐方法表）
- [ ] **C.2** 更新 `README.md` 与 `doc/coverage.md`（反映 6 类 API 与新调用形态）

## 6. 验证清单

| 阶段 | 检查项 | 方法 | 通过标准 |
|---|---|---|---|
| A/B/C | 编译 | `clang -c` 每个改动文件 | 零 warning |
| A/B/C | 冒烟 | `tests\run.ps1` | 原 7 冒烟全绿（**A 后必跑一次**，B 前后各跑一次）；A 加专测后为 8/8 |
| A/B/C | 引擎 | `tphp.exe test tests\lib` | 29 passed |
| **A** | SCOPE_IF 漏 End（14 对） | `tests/smoke-scopeif.php`（dialog 开态漏 End） | 抛 `Error` 且消息含配对名；catch 后可恢复；关态 begin 无误报。**已实现并 8/8 全绿** |
| **A** | 回归 | 现有 7 冒烟全绿 + 各域一帧绘制 | 与改动前行为一致 |
| **B** | 类注册 | 探针统计各类方法数 | 与 §3 表格一致：6 类，普通方法 313 + ctor 1 = 314 |
| **B** | 容器失配（LayoutType 5 种） | 新测试：漏 `end()` / 多 `end()` | 抛 `Error`，不静默错位 |
| **B** | LayoutType 分派 | 新测试：5 类型各画一次 | 无崩溃、节点树正常 |
| A/B/C | CJK 回归 | `smoke.php` 断行断言 | 仍 PASS |

## 7. 风险与对策

| 风险 | 对策 |
|---|---|
| 缓存槽与 props 布局冲突 | 按名查找（`tphp_class_find_prop`），不硬编码槽号 |
| 子对象在 `destroy()` 后使用 | `tui_need_ui` 已有 "window is already destroyed" 抛错 |
| `Layout::end()` 对 scroll 误调 `cui_box_end` | 按 `LayoutType` 分派，补专测 |
| 314 处注册语句迁移易漏 | 阶段 B.6 转发层让旧测试先全绿；迁移后 diff 方法数（313 普通 + 1 ctor） |
| 跨 Layout 实例误配检测不到 | 记为已知限制；每实例独立计数已覆盖主要误用 |
| SCOPE_IF 漏 End 致随机 AV | 用现有 `g_*_on` 标记统一做失配计数（§5.A.1），补 14 对专测 |
| 建子对象时引用计数错配致 UAF | alloc rc=1 归 native 返回值、不再 addref（`vm.c:1587-1590`）；多存一处各加 1（反面 `worker.c:693-700`）；写 props 用 `addref(new)/release(old)/slot=new`；native_dtor 手动 release 后须改写槽为 NULL（`tui.c:50-54`） |
| 同线程多窗口交错画同类型容器 | `_Thread_local` **不覆盖此场景**；文件级 `g_*_on` 仍进程共享。
  缓解：域对象经各自 App 派生，但静态标记按窗口归属是 TODO。当前
  `smoke.php` 两窗口是顺序绘制（非交错），不触发；记为已知限制，
  真要支持并行多窗口需把 10 组静态改为 per-App 状态（不在本版范围） |

## 8. 假设

1. 不引入 CUI 私有头；校验用自有计数
2. `App` 仍是唯一窗口入口，域对象经 `$app->form()` 派生（非 `new Form()`），
   避免多窗口歧义
3. `LayoutType` 只覆盖 5 种可统一 begin/end 的容器；其余容器走独立命名方法，
   但 SCOPE_IF 14 对仍须纳入失配检测（§5.A.1）
4. 阶段 B 有破坏性；若只想要 bug 修复、不想要 API 变更，交付阶段 A 即可停止
5. 并行多窗口（同线程交错绘制）不在本版范围，10 组文件级静态仍进程共享（§7）

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
   # 期望 8/8 PASS（原 7 冒烟 + 阶段 A 的 smoke-scopeif.php）
   ```
3. 若 `build/tphp.exe` 与源码不同步，先重链：
   ```powershell
   cd ..\TinyPHP.rt\build
   $env:PATH = 'C:\env\msys2\clang64\bin;' + $env:PATH
   .\tphp.exe ext --add C:\project\tphp\tui
   ```
4. 从第一个未勾选项继续

## 附：当前基线（2026-11-16）

- 方法总数 314（313 普通 + 1 `__construct`），全部注册在 `Tphp\Tui\App`；
  已实测 `tui_cls_add_native(cls,...)` 调用 314 处、`tui_*.c` 域注册函数 6 个（无 Core）
- 两个枚举：`ButtonVariant`(7 case)、`FontRole`(5 case)
- 冒烟 7/7 PASS，引擎 29 passed（**阶段 A 后**：新增 `smoke-scopeif.php`，8/8 PASS）
- 已有 CJK 支持：`fontLoad`/`fontLoadSystem`/`fontSetRole`/`fontAddFallback`
  + CUI 断行 CJK 处理（`cui_text.c`）
