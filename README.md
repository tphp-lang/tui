# tphp/tui

基于 [CUI 1.4.0](https://github.com/yappologistic/CUI) 的 tphp 窗口 UI 扩展包（type=ext），
单类 `Tphp\Tui\App` 立即模式封装，**310 个方法**，覆盖 CUI 组件面（覆盖矩阵与排除清单见
[doc/coverage.md](doc/coverage.md)）。

## 链入

```text
build\tphp.exe ext --add C:\project\tphp\tui     # 编译 + 重链（换像，下次运行生效）
build\tphp.exe run example/hello.php             # 交互示例
```

POSIX 上为 `./build/tphp ext --add …`。窗口后端按平台分栏（mod.json `os.<平台>.sources`）：

| 平台 | 后端 | 额外依赖 |
|---|---|---|
| Windows | `cui/backends/win32` | 无（引擎默认库 + 包内 gdi32/opengl32/imm32/dwmapi/comdlg32） |
| Linux | `cui/backends/x11` | X11 开发头文件（`libx11-dev` / `xorg-x11-devel`），链接 `-lX11` |
| macOS | `cui/backends/cocoa`（`.m`，`-fobjc-arc`） | 系统 SDK 自带 Cocoa/QuartzCore/OpenGL frameworks |

## 主循环与语义约定

```php
$app = new \Tphp\Tui\App("demo", 1080, 720);   // 第4参 true = hidden（测试用）
while ($app->frame()) {                        // 渲染上一段 + 等事件 + 开新段；关窗 false
    $app->heading("Hello");
    if ($app->button("点我")) { /* clicked */ }
    $on = $app->toggle("开关", $on);           // 返回新值（PHP 持状态）
}
$app->destroy();
```

- **返回三态**：`bool` = 点击/可见/翻转结果；`int` = 新索引/句柄/页码；关联数组 = 结构化结果
  （如 `pathBar()` 返回 `['clicked','submitted','editing']`）。值型控件一律**返回新值**。
- **buffer 往返**：`input/textarea/search/otpInput/tagInput/codeEditor/dataGridCellEdit`
  值由 PHP 持有，传入返回出（idle 帧原样返回）。
- **scope 成对**：`xxxBegin/xxxEnd` 必须配对；`open` 类参数传入当前值、返回新值。
- **单例/静态暂存**：terminal/log 需先 `terminalInit()/logInit()`；hex 选择、image 视图、
  裁剪矩形、split ratio、table/dataGrid/treeTable 的 columns 等跨段暂存（同类 scope 每段至多一层）。
- 错误一律抛异常（TypeError/ValueError/LogicException），无静默。

## API 分组表（310 方法）

### 核心（tui.c）
| 组 | 方法 | 返回 |
|---|---|---|
| 生命周期 | `__construct(title,w,h,hidden=false)`、`frame()`、`quit()`、`destroy()` | frame: bool |
| 窗口 | `setTitle(title)`、`setSize(w,h)` | void |
| 布局原语 | `columnBegin(pad,gap)`、`rowBegin(pad,gap)`、`boxEnd()`、`spacer()`、`separator()` | void |
| 基础文本 | `heading(s)`、`secondary(s)`、`text(s)`、`label(s)` | void |
| 基础控件 | `button(label)`、`toggle(label,value)` | bool（点击 / 新值） |

### 显示小件（tui_widgets.c）
| 组 | 方法 | 返回 |
|---|---|---|
| 文本 | `muted` `code` `kbd` `tooltip` `gap(size)` | void；`link` bool |
| 图标 | `icon(name,size=0,color=-1)`（0xRRGGBBAA，-1=主题色） | void |
| 卡片 | `cardBegin` / `cardEnd` | bool / void |
| 徽章头像 | `badge` `avatar` `avatarGroup(names,maxVisible=3,size=32)` | void |
| 进度 | `progress(value)` `progressRing(label,value,size=48)` `spinner(size=24)` `skeleton(height=20)` `loader()` | void |
| 状态 | `emptyState(title)` `metric(label,value)` `meter(label,used,total,unit="")` | bool / void / void |
| 按钮 | `iconButton` `copyButton(id,text)` `holdButton` `confirmButton` `statusButton(label,status=0..3)` | bool |
| | `buttonGroupBegin/End`、`splitButton(label,items[]): int`（-1 无） | void / int |
| 社交 | `reactions(id,items[]): int`（items=[icon,count,active]） | int |
| 文本特效 | `shimmerText` `textReveal(id,s)` `wordRotate(id,words,interval=2)` `numberRoll(id,s)` `gradientText` `counter(id,v)` `ticker(id,items)` | void；`typewriter(id,s): bool` |

### 表单（tui_form.c）
| 组 | 方法 | 返回 |
|---|---|---|
| buffer 往返 | `input(id,value,cap=512,opts=[])`、`textarea(cap=4096)`、`search(cap=256)`、`otpInput(id,code,len=6)`、`tagInput(id,buf,cap=512)`、`moneyInput(id,value)` | string / float |
| 数值 | `number(id,v,min,max)` `slider(label,v,min,max)` `rulerSlider` `knob` `fader` `scrub` | float（新值） |
| | `rangeSlider(label,lo,hi,min,max)`、`histogramSlider(label,bins,lo,hi,min,max)` | array{lo,hi} |
| | `levelMeter(id,levels[]): bool` | bool |
| 勾选选择 | `checkbox(label,v): bool`、`chip(label,v): bool` | bool（新值） |
| | `radio(label,current,option): int`、`radioGroup(id,items,index): int`、`segmented(id,items,index): int`、`select(id,items,index): int`、`wheelPicker(id,items,index): int`、`rating(id,v,max): int` | int（新索引） |
| | `toggleGroup(id,labels,values): bool[]`、`multiSelect(id,items,selected): bool[]` | bool[]（新状态） |
| 日期时间 | `dateToday(): [y,m,d]`、`calendar(id,date)`、`datePicker(id,date)` | [y,m,d] |
| | `dateRangePicker(id,start,end)`、`rangeCalendar(id,start,end,months=1)` | [start,end] |
| | `timePicker(id,h,m)`、`timeDial(id,h,m,size=64)` → [h,m]；`colorPicker(id,color): int`（0xRRGGBBAA） | |

### 容器与导航（tui_layout.c）
| 组 | 方法 | 返回 |
|---|---|---|
| 容器 | `boxBegin(pad=0,gap=0)`/`boxEnd`、`scrollBegin(id="",horizontal=false)`/`scrollEnd`/`scrollTo(id,y)` | void |
| | `splitBegin(id,ratio)`/`splitNext`/`splitEnd` | splitEnd: float（新比例） |
| 导航 | `tabs(id,items,index): int`、`navItem(label): bool`、`navSection`、`breadcrumb(items): int`（-1 无）、`pagination(id,page,pageCount): int`、`stepper(steps,current)` | |
| | `pathBar(id,segments): assoc`、`pathBarMenu(id): int`、`pathBarMenuEnd`、`toc(scrollId,items): int`、`tocAnchor` | |
| 覆盖层 | `popoverBegin/End/Open/Close/Toggle/IsOpen`、`menuBegin/End/Item/Separator/Label`、`contextMenuBegin/End` | 见名 |
| | `dialogBegin(title,open): bool`/`End`/`dialogFooterBegin/End`、`sheetBegin/End`、`alert(title): bool`（dismiss 前 true）、`banner(text,open): bool` | open 语义 |
| | `toast(title): int` 句柄、`toastActionClicked(h): bool`、`toastDismiss(h)`、`hoverCardBegin/End`、`userMenuBegin/End` | |
| | `speedDial(id,actions): int`（-1 无）、`spotlightBegin/End` | |
| 列表树 | `accordionBegin(title,open): bool/End`、`treeBegin(label,open): bool/End/`、`treeClicked(): bool`、`listItem(title): bool` | |
| | `virtualList(id,count,rowHeight,height=0): assoc{first,last}`/`End`、`swipeItemBegin(id,left,right)/End: int`、`reorderBegin(id,order)/End: int[]` | |
| 表格 | `tableBegin(id,columns): bool`/`tableRow(selected=false): bool`/`tableCell`/`tableCellText`/`tableEnd` | columns=[title,sortable] |
| | `dataGridBegin(id,columns,rowCount): false\|assoc{first,last}`/`Row(row): bool`/`Cell`/`CellText`/`CellEdit(value,cap=256): string`/`End` | columns 增 width/align/fixed/priority |
| | `treeCell(label,depth,hasChildren,open,icon=""): bool`、`treeTableBegin(id,columns,nodes)`/`Row(): int`/`Name`/`Depth`/`End: assoc`、`filterBar(id,fields,selected,search): assoc` | nodes=[parent,first_child,next_sibling,flags] |
| 工具条 | `toolbarBegin(id)/End/`+`Button(icon): bool`/`Separator`/`Spacer`、`statusBarBegin/End/Spacer/Text/Item: bool/Progress(v)`、`menuBarBegin/End/MenuBegin: bool/MenuEnd` | |
| 停靠 | `dockBegin/End/Item(icon,label): bool/Separator`、`dockspaceBegin/End`、`dockPanelBegin(title,open): bool/End`、`dockSave(id): string`、`dockLoad(id,json): bool` | |
| 轮播看板 | `carouselBegin(id,count,index): int`/`SlideBegin/End`/`End`、`kanbanBegin/End: assoc\|null`+`ColumnBegin(title,n): bool`/`CardBegin(id): bool`/`CardEnd`/`ColumnEnd`、`expandableCardBegin(title,open): bool/End`、`cardStackBegin(id,count)/ItemBegin(depth)/ItemEnd/End: int` | |

### 图表（tui_charts.c）
| 组 | 方法 | 返回 |
|---|---|---|
| 基础 | `lineChart(id,series,opts)` `barChart(id,series,opts)`（series=[name,values,color?]；opts=[labels,area,no_legend,no_grid,stacked,smooth,format,height,min,max]） | void |
| | `donutChart(id,values,labels,opts)`、`sparkline(values,width=100,height=24,color=0)`、`gauge(label,v,size=120)`、`radialGauge(id,v,min,max)` | void |
| | `scatterChart(id,series): int`（points=[[x,y]]）、`radarChart(id,series,axes): int` | int（hover） |
| 网格 | `heatmap(id,values,rows,cols): int`、`calendarHeatmap(id,start,values): int` | int |
| 展示 | `treemap/funnel/waffle/splitBar/barList/activityRings(id,items): int`（items=[label,value,color?]） | int |
| | `sankey(id,nodes,links): int`（links=[from,to,value]） | int |
| 金融事件 | `candlestick(id,candles,labels=[]): int`（candles=[open,high,low,close,volume]）、`waterfall(id,requests): assoc{clicked,activated,hovered}` | |
| | `dotChart(id,series): assoc{hovered,x,value,clicked}`、`timelineItem(title)` | |

### 富内容（tui_content.c）
| 组 | 方法 | 返回 |
|---|---|---|
| 代码文本 | `codeBlock(id,code,lang="",title="")`、`jsonView(id,json): bool`（合法=true）、`markdown(id,text): assoc` | |
| | `codeEditor(id,code,lang=""): assoc{text,changed,refused,focused,cursor,anchor,line,column,…}` + `codeEditorGoto/Select/Find` | |
| | `diffView(id,old,new): assoc{added,removed,changes,current}`、`hexView(id,bytes): assoc` + `hexViewGoto(id,offset)` | |
| 终端日志 | `terminalInit(textCap=65536,lineCap=4096)`/`terminalWrite(s)`/`terminalClear()`/`terminal(id): assoc{command,interrupt}` | 单例 |
| | `logInit(textCap,lineCap)`/`logAppend(level 0..4,text)`/`logClear()`/`logView(id,timestamps=false)` | 单例 |
| 向导计划 | `commandPalette(id,open,commands): assoc{picked,open}`、`tour(id,steps,step): assoc{event,step}`、`gantt(id,tasks,origin,days): int` | |
| 文件 | `fileBrowser(id,path): assoc{path,picked,selected,navigated,error}`、`filePicker(id,open,path=""): assoc{open,path}` | |
| 图片 | `imageLoad(path): assoc{tex,w,h}`、`imageCreate(w,h,rgba): int`、`imageDestroy(tex)`、`image(tex)`、`imageViewer(id,tex,w,h): assoc{zoom,cx,cy}`、`cropper(id,tex,w,h): assoc{x,y,w,h}` | |
| emoji | `emoji(s,size=0)`、`emojiPicker(id): assoc{picked}`、`emojiSupported(s): bool`、`emojiFontLoadFile(path): bool`、`emojiFontLoadSystem(): bool` | |
| 社交评论 | `chatMessage(text,outgoing=false,author="",markdown=false)`、`chatDivider(label)`、`typingIndicator(name)`、`comment(id,author,text): int`、`presence(id,users,maxVisible=3,size=32)` | |
| 签名 | `signaturePad(id): bool`、`signaturePoints(id): assoc{points,strokes}`、`signatureClear(id)` | |
| 节点编辑 | `nodeEditorBegin(id): false\|assoc{x,y,zoom}`/`End`、`nodeBegin(id,x,y,title): assoc{x,y,selected}`/`End`、`nodeInput/Output(pin,label,type=0): bool`、`nodeContentBegin/End`、`nodeLink(id,from,to): bool`、`nodeLinkCreated/Removed/Deleted: assoc\|null` | |
| devtools | `testExplorer(id,items): assoc`、`stackTrace(id,title,message,frames,lang=""): assoc{clicked}`、`requestBar(id,url,method=0,loading=false,placeholder=""): assoc`、`responseStatus(status,seconds,bytes)` | |
| | `uptime(id,services): assoc{service,day}`、`timeHistogram(id,start,bucketSeconds,buckets,series,selStart=0,selEnd=0): assoc`、`coverage(id,files): assoc{activated,lines}`、`blame(id,lines,lineCommit,commits): assoc{clicked,hovered}` | |
| | `featureFlags(id,flags,envs): assoc{flag,env,flags}`、`apiExplorer(id,endpoints): assoc{selected,try_it}`、`readout(label,value)`、`dotDisplay(text)` | |

### App 层与测试面（tui_app.c）
| 组 | 方法 | 返回 |
|---|---|---|
| 截图 | `savePng(path): bool`、`screenshotRgba(): assoc{w,h,data}` | |
| 系统 | `fileDialog(kind=0,title="",filterName="",filter="",defaultName=""): string\|false`（冒烟不调用）、`openUrl(url): bool`（同） | |
| 主题查询 | `setTheme(0 light/1 dark)`、`setAccent(0..7)`、`setReducedMotion(bool)`、`dpiScale(): float`、`size(): assoc{w,h}`、`backend(): string`、`droppedFiles(): string[]` | |
| 测试面 | `simulateMouse(x,y)`、`simulateMouseButton(0 left/1 right/2 middle, down=true)`、`simulateKey(cui_key枚举值,down,mods=0)`、`simulateText(utf8)`、`lastRect(): assoc{x,y,w,h}` | 见 coverage.md「测试面时序」 |

## 测试

```text
powershell -ExecutionPolicy Bypass -File tests\run.ps1     # cwd = tui 包根
```

7 个冒烟（basic/widgets/form/container/chart/advanced/interaction）全部 hidden 窗口、
自断言、exit code 语义；interaction 用输入注入驱动真实点击（button 返回 true、toggle 翻转、
PNG 截图落盘校验）。任一失败退出码非 0。

## 示例

| 文件 | 内容 |
|---|---|
| `example/hello.php` | 最小循环 + 计数按钮 |
| `example/todo.php` | 输入添加、勾选计数、删除/清空 |
| `example/form.php` | 表单控件全集（文本/数值/选择/日期） |
| `example/dashboard.php` | 指标 + 折线/柱状/环形/仪表 + 表格 |
| `example/markdown.php` | markdown / codeBlock / jsonView / hexView / diffView |

运行：`build\tphp.exe run example\<name>.php`（POSIX 同理）。
