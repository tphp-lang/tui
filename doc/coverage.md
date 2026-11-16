# tphp/tui 覆盖矩阵与约定

对照 `cui/include/cui/cui.h`（CUI 1.4.0）核对：每个组件级 `CUI_API …(cui_ctx*)` 入口
要么有对应 PHP 方法（API.md 按 6 类全量列出，共 313 个组件方法，另 +5 域访问器），
要么在下方排除清单。方法按域挂在 `App`/`Layout`/`Form`/`Widgets`/`Charts`/`Content`
上，经 `$app->form()` 等访问器取得（见 README）。
核对方式：按 cui.h 章节逐节对照（抽查脚本口径：`tui_cls_add_native` 注册名 vs 章节入口）。

## 1. 覆盖矩阵（按 cui.h 章节）

| cui.h 章节 | PHP 方法（域文件） | 状态 |
|---|---|---|
| App chrome（menu_bar/toolbar/status_bar/document tabs） | menuBar*、toolbar*、statusBar*（tui_layout.c） | 全覆盖；document tabs 由 tabs() 承担 |
| Layout（box/column/row/scroll/split/stack） | columnBegin/rowBegin/boxEnd/spacer（tui.c）、boxBegin、scroll*、split*（tui_layout.c） | 全覆盖 |
| Controls（input/textarea/search/button/toggle/checkbox/radio/segmented/select/slider…） | input 族、checkbox/radio/segmented/chip/rating/toggleGroup/select/multiSelect、slider 族（tui_form.c）、button/toggle（tui.c） | 全覆盖（含 _ex 变体） |
| Pickers（date/time/wheel/color/calendar） | calendar、datePicker、dateRangePicker、rangeCalendar、timePicker、timeDial、wheelPicker、colorPicker、dateToday（tui_form.c） | 全覆盖 |
| Actions（icon/copy/hold/confirm/split/status button、speed_dial、reactions、user_menu…） | iconButton、copyButton、holdButton、confirmButton、splitButton、statusButton、speedDial、reactions、userMenu*（tui_widgets/tui_layout.c） | 全覆盖 |
| Disclosure（accordion/tree/list_item/virtual_list/expandable…） | accordion*、tree*、listItem、virtualList*、expandableCard*（tui_layout.c） | 全覆盖 |
| Overlay（popover/menu/context_menu/dialog/sheet/toast/alert/banner/hover_card/spotlight…） | popover*、menu*、contextMenu*、dialog*、sheet*、toast*、alert、banner、hoverCard*、spotlight*（tui_layout.c） | 全覆盖 |
| Tooltip / badge / avatar / progress / spinner / skeleton / empty / metric / meter | tooltip、badge、avatar(Group)、progress(Ring)、spinner、skeleton、loader、emptyState、metric、meter（tui_widgets.c） | 全覆盖 |
| Text fx（shimmer/typewriter/reveal/word_rotate/number_roll/gradient/counter/ticker） | shimmerText、typewriter、textReveal、wordRotate、numberRoll、gradientText、counter、ticker（tui_widgets.c） | 全覆盖 |
| Grid（table/data_grid/tree_table/filter_bar） | table*、dataGrid*、treeCell、treeTable*、filterBar（tui_layout.c） | 全覆盖 |
| Dock | dock*、dockspace*、dockPanel*、dockSave/Load（tui_layout.c） | 全覆盖 |
| Carousel / Kanban / Card stack / Swipe / Reorder / TOC / Path bar | carousel*、kanban*、cardStack*、swipeItem*、reorder*、toc*、pathBar*（tui_layout.c） | 全覆盖 |
| Charts（line/bar/donut/sparkline/gauge/radar/scatter/heatmap/…） | lineChart、barChart、donutChart、sparkline、gauge、radialGauge、scatterChart、radarChart、heatmap、calendarHeatmap、treemap、funnel、sankey、waffle、splitBar、barList、activityRings、candlestick、waterfall、dotChart、timelineItem（tui_charts.c） | 全覆盖 |
| Markdown / Code / Hex / Diff | markdown、codeBlock、jsonView、codeEditor(+goto/select/find)、hexView(+goto)、diffView（tui_content.c） | 全覆盖 |
| Terminal / Log console | terminal*、log*（tui_content.c，单例缓冲） | 全覆盖 |
| Files（browser/picker） | fileBrowser、filePicker（tui_content.c） | 全覆盖（OS 级另见 fileDialog） |
| Images / Viewer / Cropper | imageLoad/imageCreate/imageDestroy/image/imageViewer/cropper（tui_content.c） | 全覆盖 |
| Emoji | emoji、emojiPicker、emojiSupported、emojiFontLoadFile/System（tui_content.c） | 全覆盖 |
| Social（chat/comment/presence/signature） | chatMessage、chatDivider、typingIndicator、comment、presence、signaturePad/Points/Clear（tui_content.c） | 全覆盖 |
| Node editor | nodeEditor*、node*、nodeLink*（tui_content.c，基础可用面） | 全覆盖 |
| Command palette / Tour / Gantt / Test explorer | commandPalette、tour、gantt、testExplorer（tui_content.c） | 全覆盖 |
| Devtools（stack_trace/request_bar/uptime/histogram/coverage/blame/flags/api_explorer） | stackTrace、requestBar、responseStatus、uptime、timeHistogram、coverage、blame、featureFlags、apiExplorer（tui_content.c） | 全覆盖 |
| Instruments（readout/dot_display） | readout、dotDisplay（tui_content.c） | 全覆盖 |
| App 层（cui_app.h） | frame/quit/setTitle/setSize/destroy（tui.c）、savePng/screenshotRgba/fileDialog/openUrl/size/backend/droppedFiles（tui_app.c） | 全覆盖 |
| Theme/input/query（cui.h） | setTheme/setAccent/setReducedMotion/dpiScale、simulate*、lastRect（tui_app.c） | 全覆盖（见排除清单的只读项） |

## 2. 排除清单（不封装，理由）

| CUI 入口 | 理由 |
|---|---|
| `cui_begin_frame/end_frame/set_renderer/set_viewport/needs_redraw/request_redraw/time/delta/frame_count/destroy(ctx)` | 帧循环与渲染内部，由 `frame()`/`quit()`/`destroy()` 与 App 内部持有 |
| `cui_font_load_face/measure_text` 等字体度量底层 | 底层排版接口；**加载面已覆盖**：`fontLoad`/`fontLoadSystem`/`fontSetRole`/`fontAddFallback`（CJK 支持，见 README「中文（CJK）显示」）。measure_text 仍不封装 |
| `cui_set_platform/set_strings/token_color/color_of/theme_get` | 平台/主题只读或底层查询；主题写入已覆盖（setTheme/setAccent），读取走 theme_get 属内部状态 |
| `cui_make_id/last_id/get_rect/push_id/pop_id/push_disabled/…` | 通用 ID/栈内部件；`lastRect()` 已作为测试面单独暴露（`get_rect` 需要内部 id 哈希，不封装） |
| `cui_input_*` 的原始事件泵、`cui_dropped_file` 单项读取 | 已通过 `simulate*` 与 `droppedFiles()` 覆盖应用面 |
| glfw/sdl2 可选后端 | mod.json 仅分栏三个原生后端（win32/x11/cocoa），可选后端不 vendor |

**未纳入 v1 的增量（记档，非遗漏）**：`input` 族的 CHANGED/SUBMITTED 标志位未透传
（buffer 往返只回字符串）；markdown/code_editor 等 `_ex` 的全部 opts 字段仅透传高频项
（README 各行注明），其余 opts 字段后续增量。

## 3. 实现约定

- **opts**：一律 `_ex` 变体 + `NULL` 默认；透传的高频字段在 README 对应行注明。
- **数组/结构体**：两趟执行（先全量校验后分配），raise 不泄漏；PHP 形态见各域文件头注释
  （series/items/frames/tasks/endpoints 等均为 assoc 数组行）。
- **跨段借阅**：table/dataGrid/treeTable 的 columns、reorder 的 order、swipe 的 actions、
  split 的 ratio 等在 begin→end 期间被 CUI 借阅 → 域文件内静态暂存，下次 begin/对应 end
  回收（同类 scope 每段至多一层；异常中断由下一次 begin 兜底）。
- **SCOPE_IF 条件 End（重要，曾致随机 AV）**：cui.h 中 `CUI__SCOPE_IF` 类 begin/end
  （popover/menu/contextMenu/dialog/sheet/accordion/table/tree/hoverCard/expandableCard/
  userMenu/dockPanel/dataGrid/nodeEditor 共14对）——**begin 返回 false 表示 CUI 未开
  节点，End 必须跳过**；无条件 End 会关掉父节点 → node 栈失衡 → 后续组件读到垃圾
  `cui__current` 索引而崩溃（实测 SIGSEGV 于 cui_accordion_begin，约16-20% 复现率，
  gdb 定位）。封装层以每类型静态标记守卫：End 内部按标记自动跳过，PHP 侧仍可无条件
  调 End。SCOPE 无条件对（box/scroll/split/card/buttonGroup/spotlight/dock/dockspace/
  carousel/menuBar/toolbar/statusBar/virtualList/dialogFooter/cardStack/swipeItem/
  reorder/treeTable/kanban 等）不受影响。
  **阶段 A 新增**：`frame()` 入口统一检测 14 对 `g_*_on` 残留，漏 End 抛 Error（见
  `tests/smoke-scopeif.php`）；阶段 B 又为 5 种 LayoutType 无条件容器加 `depth` 配平
  （`tests/smoke-depth.php`）——CUI 每帧重置 node 栈，故后者是把视觉错乱变显式错误，
  失配后窗口状态不可恢复，须销毁重建。
- **单例**：terminal/log 缓冲（存储由包持有，`terminalInit/logInit` 重建）、hex 选择、
  image 视图、裁剪矩形——各进程一份。
- **错误**：TypeError/ValueError/LogicException，无静默；CUI vendor（`cui/`）零修改。

## 4. 测试面时序（smoke-interaction 实测结论）

1. `frame()` 阻塞等事件；`simulate*` 注入即事件，用于泵帧（纯 move 也可能不唤醒，点击测试以
   down/up 注入驱动）。
2. 每段（两次 `frame()` 之间）布局只含本段调用的组件；rect 在 present 时写入组件 state；
   `last_id` 每段开始清零 → **读 `lastRect()` 前必须先调用目标组件**，取到的是上一段布局。
3. 输入按「上一段布局」的 rect 命中（prev-rect 模型）。
4. 点击成立时序：hover 落段 → down（组件见 down → pressed）→ 下一段 up 与组件调用同段 →
   **该次调用返回 true**；toggle 同理返回翻转后新值。
5. 同一窗口内第二段注入阶梯会因残留输入状态失效（实测）→ 多控件点击测试用独立 App 实例
   （smoke 中 button 与 toggle 分两个窗口）。
6. `fileDialog/openUrl` 会弹系统框/开浏览器，冒烟只验证注册存在，不实际调用。

## 5. 三平台构建依赖

| 平台 | 编译 | 链接 | 备注 |
|---|---|---|---|
| Windows | 引擎 cc（PATH clang） | 引擎默认库 + 包内 `-lgdi32 -lopengl32 -limm32 -ldwmapi -lcomdlg32` | 主验证平台（7 冒烟全绿） |
| Linux | 同上 | 引擎默认（-lpthread -ldl -lrt -lm）+ `-lX11` | 需 `libx11-dev`/`xorg-x11-devel` 提供 X11 头 |
| macOS | clang 编 `.m`（`os.darwin.cflags: -fobjc-arc`） | 引擎默认 + `-framework Cocoa -framework QuartzCore -framework OpenGL` | 系统 SDK 自带 |

跨平台契约（`os.<平台>.sources` 分栏）由引擎仓 `tests/pkg/test-pkg-ext.php` 在 CI 五平台
（Windows/Ubuntu×2/macOS×2）回归；tui 包在 Linux/macOS 的完整编译链接尚未在本机执行
（本机无 Linux 环境），首次在那两平台 `ext --add` 时如遇编译差异请按本表核对依赖。