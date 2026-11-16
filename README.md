# tphp/tui

基于 [CUI 1.4.0](https://github.com/yappologistic/CUI) 的 tphp 窗口 UI 扩展包（type=ext），
立即模式封装，按域分为 **6 个类**：`App`（窗口/字体/截图）/`Layout`/`Form`/`Widgets`/
`Charts`/`Content`，共 313 个组件方法 + 5 个域访问器（覆盖矩阵与排除清单见
[doc/coverage.md](doc/coverage.md)，逐方法参数见 [doc/API.md](doc/API.md)）。

> 域方法经 `$app->form()` / `$app->layout()` 等访问器取得，**不再挂在 `App` 上**
> （旧的单类 API 已移除，域方法报错的 `who` 前缀按所属域类显示）。

## 链入

```text
# 以下命令在 TinyPHP.rt 仓库根执行（build\ 是其构建输出目录）
build\tphp.exe ext --add C:\project\tphp\tui                      # 编译 + 重链（换像，下次运行生效）
build\tphp.exe run C:\project\tphp\tui\example\hello.php          # 交互示例
```

POSIX 上为 `./build/tphp ext --add …`。窗口后端按平台分栏（mod.json `os.<平台>.sources`）：

| 平台 | 后端 | 额外依赖 |
|---|---|---|
| Windows | `cui/backends/win32` | 无（引擎默认库 + 包内 gdi32/opengl32/imm32/dwmapi/comdlg32） |
| Linux | `cui/backends/x11` | X11 开发头文件（`libx11-dev` / `xorg-x11-devel`），链接 `-lX11` |
| macOS | `cui/backends/cocoa`（`.m`，`-fobjc-arc`） | 系统 SDK 自带 Cocoa/QuartzCore/OpenGL frameworks |

## 主循环与语义约定

```php
use Tphp\Tui\ButtonVariant;

$app = new \Tphp\Tui\App("demo", 1080, 720);   // 第4参 true = hidden（测试用）
while ($app->frame()) {                        // 渲染上一段 + 等事件 + 开新段；关窗 false
    $app->heading("Hello");
    if ($app->widgets()->button("点我", ButtonVariant::Primary)) { /* clicked */ }
    $on = $app->widgets()->toggle("开关", $on);           // 返回新值（PHP 持状态）
}
$app->destroy();
```

### 中文（CJK）显示

内嵌字体（Inter / Geist / Lucide）是**拉丁子集**——实测汉字覆盖率 0/20992，直接写中文
只会得到等宽豆腐块。CUI 的 fallback 链在找不到字形时会遍历全部已加载字体，所以
**装一个含 CJK 字形的系统字体即可**，无需替换内嵌字体：

```php
use Tphp\Tui\FontRole;

$app = new App("中文界面", 900, 600);
$cjk = $app->fontLoadSystem();          // 自动探测：雅黑/黑体/苹方/Noto CJK；-1 = 没找到
if ($cjk >= 0) {
    foreach ([FontRole::Body, FontRole::Medium,
              FontRole::Display, FontRole::Mono] as $role) {
        $app->fontAddFallback($role, $cjk);   // 该 role 缺字时回退到中文字体
    }
}
// 也可指定字体文件，或整体替换某个 role：
//   $f = $app->fontLoad("C:/Windows/Fonts/simhei.ttf");   // 失败抛 RuntimeException
//   $app->fontSetRole(FontRole::Body, $f);
```

- `fontLoadSystem(path="")`：无参按平台探测，给了 `path` 就用它；**加载失败返回 -1，不抛**。
- `fontAddFallback(font, fallback)`：首参是 `FontRole` 枚举，也接受 `fontLoad()` 返回的字体 id；
  每个字体最多挂 4 个回退（CUI 限制）。
- `FontRole` 枚举 = `Body`(0) / `Medium`(1) / `Display`(2) / `Mono`(3) / `Icons`(4)，
  对应 CUI 的 `CUI_FONT_*` role。
- `.ttc` 字体集合取第 0 个 face（雅黑第 0 face = Regular），已实测可用。
- **断行**：CUI 的换行器按字处理 CJK（不依赖空格），并带最小禁则——句读/收尾括号不落
  行首、开括号不留行尾。中日韩与韩文音节都在断行字符集内。

- **返回三态**：`bool` = 点击/可见/翻转结果；`int` = 新索引/句柄/页码；关联数组 = 结构化结果
  （如 `pathBar()` 返回 `['clicked','submitted','editing']`）。值型控件一律**返回新值**。
- **buffer 往返**：`input/textarea/search/otpInput/tagInput/codeEditor/dataGridCellEdit`
  值由 PHP 持有，传入返回出（idle 帧原样返回）。
- **scope 成对**：`xxxBegin/xxxEnd` 必须配对；`open` 类参数传入当前值、返回新值。
- **单例/静态暂存**：terminal/log 需先 `terminalInit()/logInit()`；hex 选择、image 视图、
  裁剪矩形、split ratio、table/dataGrid/treeTable 的 columns 等跨段暂存（同类 scope 每段至多一层）。
- 错误一律抛异常（TypeError/ValueError/LogicException），无静默。

## PHP 侧使用指南

### 最小应用

```php
<?php
use Tphp\Tui\App;

$app = new App("我的窗口", 800, 600);      // 第4参 true = 隐藏窗口（测试）
while ($app->frame()) {                    // 渲染上一段 → 等事件 → 开新段；关窗返回 false
    $app->heading("Hello tui");
    if ($app->widgets()->button("退出")) {
        $app->quit();                      // 下一次 frame() 返回 false，循环自然结束
    }
}
$app->destroy();                           // 释放窗口/纹理（循环退出后必调）
```

组件调用只发生在 `frame()` 返回 true 的段内（立即模式：每帧重放一遍调用即可，
不需要手工维护控件树）。**`frame()` 会阻塞等事件**——不要在段外空转调用组件。

### 状态管理四范式

立即模式没有控件对象，**状态全部放在 PHP 变量里**，每帧传入、取回新值：

```php
// 1) 值型：返回新值（checkbox/toggle/slider/select/tabs/...）
$agree = false;
$tab   = 0;
$vol   = 0.5;
while ($app->frame()) {
    $agree = $app->form()->checkbox("同意协议", $agree);
    $tab   = $app->layout()->tabs("main", ["首页", "设置"], $tab);   // 点击后返回新索引
    $vol   = $app->form()->slider("音量", $vol, 0.0, 1.0);
}

// 2) buffer 往返：值由 PHP 持有，传入返回出（input/textarea/search/otp/tag/codeEditor）
$text = "";
while ($app->frame()) {
    $text = $app->form()->input("q", $text, 256, ["label" => "搜索"]);
    if ($app->widgets()->button("提交") && trim($text) !== "") {
        do_search($text);
        $text = "";
    }
}

// 3) open 类：传入当前展开态，返回本帧新态（dialog/accordion/tree/banner/...）
$dialog_open = false;
while ($app->frame()) {
    if ($app->widgets()->button("打开对话框")) $dialog_open = true;
    if ($app->layout()->dialogBegin("标题", $dialog_open)) {
        $app->text("内容");
        if ($app->widgets()->button("关闭")) $dialog_open = false;
        $app->layout()->dialogFooterBegin();
        $app->widgets()->button("OK");
        $app->layout()->dialogFooterEnd();
        $app->layout()->dialogEnd();                  // Begin/End 必须配对（关态时封装层自动跳过）
    }
}

// 4) scope 成对：xxxBegin / xxxEnd 每帧一一配对（table/scroll/split/menu/...）
while ($app->frame()) {
    if ($app->layout()->tableBegin("t1", [["title" => "名"], ["title" => "值"]])) {
        $app->layout()->tableRow(false);
        $app->layout()->tableCell();  $app->layout()->tableCellText("a");
        $app->layout()->tableCell();  $app->layout()->tableCellText("1");
        $app->layout()->tableEnd();
    }
}
```

### 常见场景片段

```php
// 计数器（点击 +1，按钮返回 bool）
$count = 0;
$app->frame();
if ($app->widgets()->button("+1")) $count++;
$app->text("count = " . $count);

// 列表增删（数组即状态）
$items = ["a", "b"];
$keep = [];
foreach ($items as $it) {
    $on = $app->widgets()->toggle($it, false);            // 行内勾选态也可放平行数组
    if (!$app->widgets()->button("删")) $keep[] = $it;
}
$items = $keep;

// 轻反馈：toast 句柄 + alert/banner（返回 open/可见态）
$th = $app->layout()->toast("已保存");
if ($app->layout()->toastActionClicked($th)) { /* ... */ }

// 图表数据 = 每帧构造的 PHP 数组
$app->charts()->lineChart("c1", [
    ["name" => "访问", "values" => [120.0, 180.0, 150.0], "color" => 0x3B82F6FF],
], ["height" => 180]);

// 日期：[y, m, d] 三元组往返
$d = $app->form()->datePicker("d1", [2026, 11, 15]);
$app->widgets()->muted("选中 {$d[0]}-{$d[1]}-{$d[2]}");
```

### 异常处理

所有参数错误都抛异常，可正常 try/catch（tphp 原生异常）：

```php
try {
    $app->setAccent(99);                     // ValueError: accent must be in 0..7
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
```

TypeError（类型不符）/ ValueError（取值越界）/ LogicException（时序错误，如未
`terminalInit()` 就 `terminal()`）三类；没有静默失败分支。

### 写测试（hidden 窗口骨架）

```php
<?php
$fail = 0;
function check(bool $c, string $n): void {
    global $fail;
    if ($c) echo "PASS $n\n"; else { echo "FAIL $n\n"; $fail++; }
}

$app = new \Tphp\Tui\App("t", 640, 480, true);   // 隐藏窗口
$app->frame();                                    // 开首段
// —— 每帧内容：绘制 + 断言返回值 ——
check($app->widgets()->button("hi") === false, "idle-not-clicked");
$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();
echo $fail ? "SMOKE=FAIL\n" : "SMOKE=OK\n";
exit($fail ? 1 : 0);
```

运行 `tphp run tests\xxx.php`；整套冒烟用 `tests\run.ps1` 一键驱动。需要测
点击/翻转等交互时，用 `simulateMouse/simulateMouseButton/simulateKey` 注入 +
`lastRect()` 定位（时序规则见 [doc/coverage.md](doc/coverage.md) §4）。

### 使用注意

- `xxxBegin()` 返回 false（未展开/不可见）时，对应 `xxxEnd()` **仍要无条件调用**
  ——封装层已按 SCOPE_IF 语义内部守卫，直接配对写即可。
- 同一组件每段只调用一次；id 类参数（`input("q",...)` 等）在同帧内保持稳定。
- 资源型对象（`imageLoad` 的纹理）用完调 `imageDestroy`；窗口结束调 `destroy()`。
- `terminal()/logView()` 等单例组件需先 `terminalInit()/logInit()`，否则抛
  LogicException。
- 需要真实显示窗口时去掉第4参（hidden），关窗即 `frame()` 返回 false。


## API

6 个类共 **313 个组件方法**的逐方法参数表见 **[doc/API.md](doc/API.md)**（参数带中文说明与默认值）：

| 类 | 取得方式 | 内容 |
|---|---|---|
| `App` | `new App(...)` | 窗口生命周期、文本原语、字体、主题、截图、测试面 + 5 个域访问器 |
| `Layout` | `$app->layout(LayoutType)` | 容器、覆盖层、导航、表格、停靠 |
| `Form` | `$app->form()` | 输入、数值、勾选、日期时间 |
| `Widgets` | `$app->widgets()` | 显示小件、按钮、文本特效 |
| `Charts` | `$app->charts()` | 21 种图表 |
| `Content` | `$app->content()` | 富文本、编辑器、聊天、节点、devtools |

域方法**只挂在对应域类上**，`$app->input()` 这类旧写法不再可用，改用 `$app->form()->input()`。

三个枚举：

| 枚举 | case | 用途 |
|---|---|---|
| `Tphp\Tui\ButtonVariant` | `Default`=0 `Secondary`=1 `Primary`=2 `Outline`=3 `Ghost`=4 `Danger`=5 `Link`=6 | `widgets()->button()` 的 variant |
| `Tphp\Tui\FontRole` | `Body`=0 `Medium`=1 `Display`=2 `Mono`=3 `Icons`=4 | `fontSetRole()` / `fontAddFallback()` 的 role |
| `Tphp\Tui\LayoutType` | `Column`=0 `Row`=1 `Box`=2 `Scroll`=3 `Split`=4 | `layout()` 访问器的容器类型提示 |

## 测试

```text
powershell -ExecutionPolicy Bypass -File tests\run.ps1     # cwd = tui 包根
```

12 个冒烟全部 hidden 窗口、自断言、exit code 语义：基本/高级/图表/容器/表单/交互/小件
（原 7 个）+ `scopeif`（SCOPE_IF 漏 End 检测）+ `domain`（6 类 API）+ `depth`（容器配平）+
`multiwindow`（per-App 状态隔离）+ `thread`（真 OS 线程 × per-App × TLS：
Tphp\Thread 两 worker 线程各自建窗，A 的残留不污染 B、terminal 缓冲跨线程独立、
主窗口在 wait() 回来后仍正常）。
interaction 用输入注入驱动真实点击（button 返回 true、toggle 翻转、PNG 截图落盘校验）。
任一失败退出码非 0。

## 示例

| 文件 | 内容 |
|---|---|
| `example/hello.php` | 最小循环 + 计数按钮 |
| `example/todo.php` | 输入添加、勾选计数、删除/清空 |
| `example/form.php` | 表单控件全集（文本/数值/选择/日期） |
| `example/dashboard.php` | 指标 + 折线/柱状/环形/仪表 + 表格 |
| `example/markdown.php` | markdown / codeBlock / jsonView / hexView / diffView |

运行（在 TinyPHP.rt 仓库根执行）：`build\tphp.exe run C:\project\tphp\tui\example\<name>.php`（POSIX 同理）。