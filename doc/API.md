# API 分组表

## `namespace` 命名空间 `use Tphp\Tui;`

### `class` 类名 `App`

窗口生命周期、文本原语、字体、主题、截图、测试面 + 域访问器

| 方法名 | 参数 | 返回值 | 说明 |
| :--- | :--- | :--- | :--- |
| `__construct` | `string` `$title` = "CUI" 标题 <br> `int` `$width` = 1080 窗口宽度 <br> `int` `$height` = 720 窗口高度 <br> `bool` `$hidden` = false 是否隐藏窗口（测试用，不抢焦点） <br> `bool` `$continuous` = false 是否连续渲染 | - | 创建窗口。隐藏窗口仍可截图 |
| `frame` | - | bool | 渲染上一段 → 等事件 → 开新段；关窗返回 false |
| `quit` | - | - | 请求退出：下一次 frame() 返回 false |
| `setTitle` | `string` `$title` 标题 | - | 设置窗口标题 |
| `setSize` | `int` `$width` 宽度 <br> `int` `$height` 高度 | - | 设置窗口尺寸 |
| `destroy` | - | - | 幂等；释放窗口与纹理，循环退出后必调 |
| `spacer` | - | - | 弹性空白 |
| `separator` | - | - | 分隔线 |
| `heading` | `string` `$text` 文本 <br> `int` `$level` 层级 | - | 标题；level 由 CUI 内部夹到 1..4 |
| `secondary` | `string` `$text` 文本 | - | 次要文本（弱化色） |
| `text` | `string` `$text` 文本 | - | 正文文本 |
| `label` | `string` `$text` 文本 | - | 标签文本 |
| `fontLoad` | `string` `$path` 字体文件路径 | int | 加载 .ttf/.otf/.ttc（ttc 取第 0 face）；失败抛 RuntimeException |
| `fontLoadSystem` | `string` `$path` = NULL 指定路径（留空则按平台探测） | int | 探测系统 CJK 字体；失败返回 -1，不抛 |
| `fontSetRole` | `FontRole` `$role` 字体角色 <br> `int` `$font` 字体 id | - | 让某个 role 改用指定字体 |
| `fontAddFallback` | `FontRole` `$font` 目标角色 <br> `int` `$fallback` 回退字体 id | - | 缺字时回退；每字体最多 4 个 |
| `savePng` | `string` `$path` 输出路径 | bool | 保存当前帧为 PNG；隐藏窗口亦可 |
| `screenshotRgba` | - | array | 返回 w / h / data（RGBA 原始字节） |
| `fileDialog` | `int` `$kind` = 0 类型 <br> `string` `$title` = "" 标题 <br> `string` `$filterName` = "" 过滤器名 <br> `string` `$filter` = "" 过滤器 <br> `string` `$defaultName` = "" 默认文件名 | string\|false | 阻塞式系统对话框；冒烟测试不调用 |
| `openUrl` | `string` `$url` URL | bool | 用系统默认浏览器打开；冒烟测试不调用 |
| `setTheme` | `int` `$mode` 模式 | - | 0 = light，1 = dark |
| `setAccent` | `int` `$accent` 强调色 | - | 0..7 主题强调色 |
| `setReducedMotion` | `bool` `$reduced` 是否减少动效 | - | 减少动画 |
| `dpiScale` | - | float | 当前 DPI 缩放 |
| `size` | - | array | 逻辑尺寸 w / h |
| `backend` | - | string | 窗口后端名称 |
| `droppedFiles` | - | array | 本帧拖入的文件路径 |
| `simulateMouse` | `float` `$x` X 坐标 <br> `float` `$y` Y 坐标 | - | 测试面：注入鼠标移动 |
| `simulateMouseButton` | `int` `$button` = 0 0 左 / 1 右 / 2 中 <br> `bool` `$down` = true 按下或抬起 | - | 测试面：注入鼠标按键 |
| `simulateKey` | `int` `$key` cui_key 枚举值 <br> `bool` `$down` = true 按下或抬起 <br> `int` `$mods` = 0 修饰键掩码 | - | 测试面：注入键盘事件 |
| `simulateText` | `string` `$text` 文本 | - | 测试面：注入 UTF-8 文本 |
| `lastRect` | - | array | 测试面：上一段最后一个组件的矩形 x/y/w/h |
| `layout` | `LayoutType` `$type` 容器类型提示 | Layout | 取得 Layout 域对象（缓存） |
| `form` | - | Form | 取得 Form 域对象（缓存） |
| `widgets` | - | Widgets | 取得 Widgets 域对象（缓存） |
| `charts` | - | Charts | 取得 Charts 域对象（缓存） |
| `content` | - | Content | 取得 Content 域对象（缓存） |

### `class` 类名 `Layout`

容器、覆盖层、导航、表格、停靠（经 `$app->layout()` 取得）

| 方法名 | 参数 | 返回值 | 说明 |
| :--- | :--- | :--- | :--- |
| `columnBegin` | `int` `$pad` 内边距 <br> `int` `$gap` 间距 | - | 纵向容器；与 boxEnd() 配对 |
| `rowBegin` | `int` `$gap` 间距 | - | 横向容器；与 boxEnd() 配对 |
| `boxEnd` | - | - | 结束最近一次 columnBegin/rowBegin/boxBegin |
| `boxBegin` | `int` `$pad` = 0 内边距 <br> `int` `$gap` = 0 间距 | - | 通用 box 容器；与 boxEnd 配对 |
| `scrollBegin` | `string` `$id` = "" 组件 id <br> `bool` `$horizontal` = false 是否横向 | - | 滚动容器；与 scrollEnd 配对 |
| `scrollEnd` | - | - | 结束滚动容器 |
| `scrollTo` | `string` `$id` 组件 id <br> `float` `$y` 目标偏移 | - | 滚动到指定 y |
| `splitBegin` | `string` `$id` 组件 id <br> `float` `$ratio` 当前比例 | - | 分栏开始；ratio 传当前比例 |
| `splitNext` | - | - | 切到下一格 |
| `splitEnd` | - | float | 结束分栏；返回新比例 |
| `tabs` | `string` `$id` 组件 id <br> `string[]` `$items` 标签页 <br> `int` `$index` 当前索引 | int | 返回新索引 |
| `navItem` | `string` `$label` 标签 | bool | 返回是否被点击 |
| `navSection` | `string` `$title` 标题 | - | 导航分组标题 |
| `breadcrumb` | `string[]` `$items` 路径段 | int | 返回点击项索引，-1 = 无 |
| `pagination` | `string` `$id` 组件 id <br> `int` `$page` 当前页 <br> `int` `$pageCount` 总页数 | int | 返回新页码 |
| `stepper` | `string[]` `$steps` 步骤 <br> `int` `$current` 当前步 | - | 步骤条 |
| `pathBar` | `string` `$id` 组件 id <br> `string[]` `$segments` 路径段 | array | 返回 clicked / submitted / editing |
| `pathBarMenu` | `string` `$id` 组件 id | int | 返回选中索引 |
| `pathBarMenuEnd` | - | - | 结束路径栏菜单 |
| `toc` | `string` `$scrollId` 滚动容器 id <br> `array` `$items` 目录项 | int | 返回点击项索引 |
| `tocAnchor` | `string` `$id` 锚点 id | - | 目录锚点 |
| `popoverBegin` | `string` `$id` 组件 id | bool | 返回 false 时 popoverEnd() 仍要无条件调用 |
| `popoverEnd` | - | - | 结束浮层（无条件调用） |
| `popoverOpen` | `string` `$id` 组件 id | - | 打开浮层 |
| `popoverClose` | `string` `$id` 组件 id | - | 关闭浮层 |
| `popoverToggle` | `string` `$id` 组件 id | - | 切换浮层 |
| `popoverIsOpen` | `string` `$id` 组件 id | bool | 浮层是否打开 |
| `menuBegin` | `string` `$id` 组件 id | bool | 菜单开始 |
| `menuEnd` | - | - | 菜单结束 |
| `contextMenuBegin` | `string` `$id` 组件 id | bool | 右键菜单开始 |
| `contextMenuEnd` | - | - | 右键菜单结束 |
| `menuItem` | `string` `$label` 标签 | bool | 返回是否被点击 |
| `menuSeparator` | - | - | 菜单分隔线 |
| `menuLabel` | `string` `$text` 文本 | - | 菜单分组标签 |
| `dialogBegin` | `string` `$title` 标题 | bool | open 传当前展开态，返回本帧新态 |
| `dialogEnd` | - | - | 结束对话框 |
| `dialogFooterBegin` | - | - | 对话框底栏开始 |
| `dialogFooterEnd` | - | - | 对话框底栏结束 |
| `sheetBegin` | `string` `$title` 标题 | bool | 底部抽屉开始；open 传当前态 |
| `sheetEnd` | - | - | 结束底部抽屉 |
| `toast` | `string` `$title` 标题 | int | 返回句柄，供 toastActionClicked/toastDismiss 使用 |
| `toastActionClicked` | `int` `$handle` 句柄 | bool | 操作按钮是否被点击 |
| `toastDismiss` | `int` `$handle` 句柄 | - | 关闭 toast |
| `alert` | `string` `$title` 标题 | bool | dismiss 之前恒 true |
| `banner` | `string` `$text` 文本 | bool | open 传当前态，返回本帧新态 |
| `hoverCardBegin` | `string` `$id` 组件 id | bool | 悬停卡片开始 |
| `hoverCardEnd` | - | - | 结束悬停卡片 |
| `userMenuBegin` | `string` `$id` 组件 id | bool | 用户菜单开始 |
| `userMenuEnd` | - | - | 结束用户菜单 |
| `speedDial` | `string` `$id` 组件 id <br> `array` `$actions` 动作项 | int | 返回选中 action 索引，-1 = 无 |
| `spotlightBegin` | `string` `$id` 组件 id | - | 聚焦引导开始 |
| `spotlightEnd` | - | - | 结束聚焦引导 |
| `accordionBegin` | `string` `$title` 标题 | bool | 返回 false 时 accordionEnd() 仍要无条件调用 |
| `accordionEnd` | - | - | 结束手风琴（无条件调用） |
| `treeBegin` | `string` `$label` 标签 | bool | 返回 false 时 treeEnd() 仍要无条件调用 |
| `treeEnd` | - | - | 结束树（无条件调用） |
| `treeClicked` | - | bool | 树节点是否被点击 |
| `listItem` | `string` `$title` 标题 | bool | 返回是否被点击 |
| `virtualList` | `string` `$id` 组件 id <br> `int` `$count` 总数 <br> `float` `$rowHeight` 行高 <br> `float` `$height` = 0 视口高度（0 = 自适应） | array | 返回 first / last；与 virtualListEnd 配对 |
| `virtualListEnd` | - | - | 结束虚拟列表 |
| `swipeItemBegin` | `string` `$id` 组件 id <br> `array` `$left` 左侧动作 | - | 与 swipeItemEnd 配对 |
| `swipeItemEnd` | - | int | 结束滑动项；返回选中 action |
| `reorderBegin` | `string` `$id` 组件 id <br> `int[]` `$order` 待排序索引 | - | 拖拽排序开始 |
| `reorderEnd` | - | array | 结束拖拽排序；返回新顺序 |
| `tableBegin` | `string` `$id` 组件 id <br> `array` `$columns` 列定义（title / sortable） | bool | 返回 true 才继续 tableRow/tableCell/tableEnd |
| `tableRow` | `bool` `$selected` = false 选中态 | bool | 表格行开始 |
| `tableCell` | - | - | 表格单元格开始 |
| `tableCellText` | `string` `$text` 文本 | - | 单元格文本 |
| `tableEnd` | - | - | 结束表格 |
| `dataGridBegin` | `string` `$id` 组件 id <br> `array` `$columns` 列定义 <br> `int` `$rowCount` 行数 | array\|bool | 返回 false 或 first / last |
| `dataGridRow` | `int` `$row` 行索引 | bool | 数据网格行开始 |
| `dataGridCell` | - | - | 数据网格单元格开始 |
| `dataGridCellText` | `string` `$text` 文本 | - | 单元格文本 |
| `dataGridCellEdit` | `string` `$value` 当前值 <br> `int` `$cap` = 256 字节上限 | string | buffer 往返；返回编辑后文本 |
| `dataGridEnd` | - | - | 结束数据网格 |
| `treeCell` | `string` `$label` 标签 <br> `int` `$depth` 层级 <br> `bool` `$hasChildren` 是否有子节点 <br> `string` `$icon` = "" 图标 | bool | 返回是否被点击 |
| `treeTableBegin` | `string` `$id` 组件 id <br> `array` `$columns` 列定义 <br> `array` `$nodes` 节点 | - | 与 treeTableEnd 配对 |
| `treeTableRow` | - | int | 树表格行；返回行索引 |
| `treeTableName` | `string` `$text` 节点名 <br> `string` `$icon` = "" 图标 | - | 树表格节点名 |
| `treeTableDepth` | - | int | 节点层级 |
| `treeTableEnd` | - | array | 结束树表格；返回结果 |
| `filterBar` | `string` `$id` 组件 id <br> `array` `$fields` 字段 <br> `int[]` `$selected` 选中索引 <br> `string` `$search` = "" 搜索词 | array | 返回选中与搜索状态 |
| `toolbarBegin` | `string` `$id` 组件 id | - | 工具栏开始 |
| `toolbarEnd` | - | - | 工具栏结束 |
| `toolbarButton` | `string` `$icon` 图标名 | bool | 返回是否被点击 |
| `toolbarSeparator` | - | - | 工具栏分隔 |
| `toolbarSpacer` | - | - | 工具栏空白 |
| `statusBarBegin` | `string` `$id` 组件 id | - | 状态栏开始 |
| `statusBarEnd` | - | - | 状态栏结束 |
| `statusBarSpacer` | - | - | 状态栏空白 |
| `statusBarText` | `string` `$text` 文本 | - | 状态栏文本 |
| `statusBarItem` | `string` `$label` 标签 | bool | 返回是否被点击 |
| `statusBarProgress` | `float` `$value` 进度 | - | 状态栏进度 |
| `menuBarBegin` | `string` `$id` 组件 id | - | 菜单栏开始 |
| `menuBarEnd` | - | - | 菜单栏结束 |
| `menuBarMenuBegin` | `string` `$label` 标签 | bool | 菜单栏下拉开始 |
| `menuBarMenuEnd` | - | - | 菜单栏下拉结束 |
| `dockBegin` | `string` `$id` 组件 id | - | 停靠区开始 |
| `dockEnd` | - | - | 停靠区结束 |
| `dockItem` | `string` `$icon` 图标 <br> `string` `$label` 标签 | bool | 返回是否被点击 |
| `dockSeparator` | - | - | 停靠分隔 |
| `dockspaceBegin` | `string` `$id` 组件 id | - | 停靠空间开始 |
| `dockspaceEnd` | - | - | 停靠空间结束 |
| `dockPanelBegin` | `string` `$title` 标题 | bool | open 传当前态；与 dockPanelEnd 配对 |
| `dockPanelEnd` | - | - | 结束停靠面板 |
| `dockSave` | `string` `$id` 组件 id | string | 返回布局 JSON |
| `dockLoad` | `string` `$id` 组件 id <br> `string` `$json` 布局 JSON | bool | json 为 dockSave 的产物 |
| `carouselBegin` | `string` `$id` 组件 id <br> `int` `$count` 页数 <br> `int` `$index` 当前页 | int | index 传当前页，返回新页索引 |
| `carouselSlideBegin` | - | - | 轮播页开始 |
| `carouselSlideEnd` | - | - | 轮播页结束 |
| `carouselEnd` | - | - | 结束轮播 |
| `kanbanBegin` | `string` `$id` 组件 id | - | 看板开始 |
| `kanbanColumnBegin` | `string` `$title` 列标题 <br> `int` `$cardCount` 卡片数 | bool | 看板列开始 |
| `kanbanCardBegin` | `int` `$cardId` 卡片 id | bool | 看板卡片开始 |
| `kanbanCardEnd` | - | - | 看板卡片结束 |
| `kanbanColumnEnd` | - | - | 看板列结束 |
| `kanbanEnd` | - | array\|null | 与 kanbanBegin 配对；返回拖拽结果或 null |
| `expandableCardBegin` | `string` `$title` 标题 | bool | open 传当前态；与 expandableCardEnd 配对 |
| `expandableCardEnd` | - | - | 结束可展开卡片 |
| `cardStackBegin` | `string` `$id` 组件 id <br> `int` `$count` 卡片数 | - | 与 cardStackEnd 配对 |
| `cardStackItemBegin` | `int` `$depth` 层级 | - | 卡片堆叠项开始 |
| `cardStackItemEnd` | - | - | 卡片堆叠项结束 |
| `cardStackEnd` | - | int | 结束卡片堆叠；返回选中项 |

### `class` 类名 `Form`

输入、数值、勾选、日期时间（经 `$app->form()` 取得）

| 方法名 | 参数 | 返回值 | 说明 |
| :--- | :--- | :--- | :--- |
| `input` | `string` `$id` 组件 id <br> `string` `$value` 当前值 <br> `int` `$cap` = 512 字节上限 | string | buffer 往返：值由 PHP 持有，传入返回出 |
| `textarea` | `string` `$id` 组件 id <br> `string` `$value` 当前值 <br> `int` `$cap` = 4096 字节上限 | string | buffer 往返 |
| `search` | `string` `$id` 组件 id <br> `string` `$value` 当前值 <br> `int` `$cap` = 256 字节上限 | string | buffer 往返 |
| `otpInput` | `string` `$id` 组件 id <br> `string` `$code` 当前验证码 <br> `int` `$length` = 6 位数 | string | buffer 往返 |
| `tagInput` | `string` `$id` 组件 id <br> `string` `$buffer` 当前缓冲 <br> `int` `$cap` = 512 字节上限 | string | buffer 往返 |
| `moneyInput` | `string` `$id` 组件 id <br> `float` `$value` 当前值 | float | 返回新值 |
| `number` | `string` `$label` 标签 <br> `float` `$value` 当前值 <br> `float` `$min` 最小值 <br> `float` `$max` 最大值 | float | 返回新值 |
| `slider` | `string` `$label` 标签 <br> `float` `$value` 当前值 <br> `float` `$min` 最小值 <br> `float` `$max` 最大值 | float | 返回新值 |
| `rangeSlider` | `string` `$label` 标签 <br> `float` `$lo` 下界 <br> `float` `$hi` 上界 <br> `float` `$min` 最小值 <br> `float` `$max` 最大值 | array | 返回 [lo, hi] |
| `rulerSlider` | `string` `$label` 标签 <br> `float` `$value` 当前值 <br> `float` `$min` 最小值 <br> `float` `$max` 最大值 | float | 返回新值 |
| `histogramSlider` | `string` `$label` 标签 <br> `float[]` `$bins` 区间划分 <br> `float` `$lo` 下界 <br> `float` `$hi` 上界 <br> `float` `$min` 最小值 <br> `float` `$max` 最大值 | array | 返回 [lo, hi] |
| `knob` | `string` `$label` 标签 <br> `float` `$value` 当前值 <br> `float` `$min` 最小值 <br> `float` `$max` 最大值 | float | 返回新值 |
| `fader` | `string` `$label` 标签 <br> `float` `$value` 当前值 <br> `float` `$min` 最小值 <br> `float` `$max` 最大值 | float | 返回新值 |
| `scrub` | `string` `$label` 标签 <br> `float` `$value` 当前值 <br> `float` `$min` 最小值 <br> `float` `$max` 最大值 | float | 返回新值 |
| `levelMeter` | `string` `$id` 组件 id <br> `float[]` `$levels` 电平数组 | bool | 返回是否变化 |
| `checkbox` | `string` `$label` 标签 <br> `bool` `$value` 当前状态 | bool | 返回新状态 |
| `radio` | `string` `$label` 标签 <br> `int` `$current` 当前选中 <br> `int` `$option` 本项值 | int | 返回选中项索引 |
| `radioGroup` | `string` `$id` 组件 id <br> `string[]` `$items` 选项 <br> `int` `$index` 当前索引 | int | 返回选中索引 |
| `segmented` | `string` `$id` 组件 id <br> `string[]` `$items` 选项 <br> `int` `$index` 当前索引 | int | 返回选中索引 |
| `chip` | `string` `$label` 标签 <br> `bool` `$selected` 当前选中态 | bool | 返回新状态 |
| `rating` | `string` `$id` 组件 id <br> `int` `$value` 当前分值 <br> `int` `$max` 满分 | int | 返回新分值 |
| `toggleGroup` | `string` `$id` 组件 id <br> `string[]` `$labels` 标签数组 <br> `bool[]` `$values` 状态数组 | array | 返回各开关新状态 |
| `select` | `string` `$id` 组件 id <br> `string[]` `$items` 选项 <br> `int` `$index` 当前索引 | int | 返回选中索引 |
| `multiSelect` | `string` `$id` 组件 id <br> `string[]` `$items` 选项 <br> `bool[]` `$selected` 选中态 | array | 返回各选项新选中状态 |
| `dateToday` | - | array | 返回 [年, 月, 日] |
| `calendar` | `string` `$id` 组件 id | array | 传入当前日期，返回新日期 [年, 月, 日] |
| `datePicker` | `string` `$id` 组件 id | array | 返回 [年, 月, 日] |
| `dateRangePicker` | `string` `$id` 组件 id | array | 返回 [start, end] |
| `rangeCalendar` | `string` `$id` 组件 id <br> `int` `$months` = 1 可见月数 | array | 返回 [start, end] |
| `timePicker` | `string` `$id` 组件 id <br> `int` `$hour` 时 <br> `int` `$minute` 分 | array | 返回 [时, 分] |
| `timeDial` | `string` `$id` 组件 id <br> `int` `$hour` 时 <br> `int` `$minute` 分 <br> `float` `$size` = 64 直径 | array | 返回 [时, 分] |
| `wheelPicker` | `string` `$id` 组件 id <br> `string[]` `$items` 选项 <br> `int` `$index` 当前索引 | int | 返回选中索引 |
| `colorPicker` | `string` `$id` 组件 id <br> `int` `$color` 当前颜色 | int | 返回 0xRRGGBBAA |

### `class` 类名 `Widgets`

显示小件、按钮、文本特效（经 `$app->widgets()` 取得）

| 方法名 | 参数 | 返回值 | 说明 |
| :--- | :--- | :--- | :--- |
| `button` | `string` `$label` 标签 <br> `ButtonVariant` `$variant` = 0 变体 | bool | 返回本帧是否被点击 |
| `toggle` | `string` `$label` 标签 <br> `bool` `$value` 当前值 | bool | 返回新值（PHP 持状态） |
| `muted` | `string` `$text` 文本 | - | 弱化文本 |
| `code` | `string` `$text` 文本 | - | 行内代码 |
| `kbd` | `string` `$text` 文本 | - | 键盘按键样式文本 |
| `link` | `string` `$text` 文本 | bool | 返回是否被点击 |
| `icon` | `string` `$name` 图标名 <br> `float` `$size` = 0 尺寸（0 = 跟随当前字号） <br> `int` `$color` = -1 颜色（0xRRGGBBAA，-1 = 主题色） | - | 绘制图标 |
| `tooltip` | `string` `$text` 文本 | - | 悬停提示 |
| `gap` | `float` `$size` = 8 间距 | - | 固定间距 |
| `cardBegin` | - | bool | 返回 true 才需调 cardEnd() |
| `cardEnd` | - | - | 结束卡片 |
| `badge` | `string` `$text` 文本 | - | 徽章 |
| `avatar` | `string` `$name` 名称 | - | 头像 |
| `avatarGroup` | `string[]` `$names` 名称列表 <br> `int` `$maxVisible` = 3 最多显示数 <br> `float` `$size` = 32 尺寸 | - | 头像组 |
| `progress` | `float` `$value` = 0 进度 0..1 | - | 进度条 |
| `progressRing` | `string` `$label` 标签 <br> `float` `$value` = 0 进度 <br> `float` `$size` = 48 直径 | - | 环形进度 |
| `spinner` | `float` `$size` = 24 尺寸 | - | 旋转加载指示 |
| `skeleton` | `float` `$height` = 20 高度 | - | 骨架占位 |
| `loader` | - | - | 加载中占位 |
| `emptyState` | `string` `$title` 标题 | bool | 返回是否有操作 |
| `metric` | `string` `$label` 标签 <br> `string` `$value` 数值 | - | 指标 |
| `meter` | `string` `$label` 标签 <br> `float` `$used` 已用 <br> `float` `$total` 总量 <br> `string` `$unit` = "" 单位 | - | 用量条 |
| `iconButton` | `string` `$icon` 图标名 | bool | 返回是否被点击 |
| `copyButton` | `string` `$id` 组件 id <br> `string` `$text` 要复制的文本 | bool | 返回是否被点击 |
| `holdButton` | `string` `$label` 标签 | bool | 返回是否触发 |
| `confirmButton` | `string` `$label` 标签 | bool | 返回是否确认 |
| `buttonGroupBegin` | - | - | 按钮组开始 |
| `buttonGroupEnd` | - | - | 按钮组结束 |
| `splitButton` | `string` `$label` 标签 <br> `string[]` `$items` 菜单项 | int | 返回选中项索引，-1 = 未选 |
| `statusButton` | `string` `$label` 标签 <br> `int` `$status` = 0 状态 0..3 | bool | 返回是否被点击 |
| `reactions` | `string` `$id` 组件 id | int | 返回选中的 reaction 索引 |
| `shimmerText` | `string` `$text` 文本 | - | 流光文本 |
| `typewriter` | `string` `$id` 组件 id <br> `string` `$text` 文本 | bool | 返回本帧是否有推进 |
| `textReveal` | `string` `$id` 组件 id <br> `string` `$text` 文本 | - | 逐字显现 |
| `wordRotate` | `string` `$id` 组件 id <br> `string[]` `$words` 词表 <br> `float` `$interval` = 2.0 间隔秒 | - | 轮播词 |
| `numberRoll` | `string` `$id` 组件 id <br> `string` `$value` 数值 | - | 数字滚动 |
| `gradientText` | `string` `$text` 文本 | - | 渐变文本 |
| `counter` | `string` `$id` 组件 id <br> `float` `$value` = 0 目标值 | - | 计数动画 |
| `ticker` | `string` `$id` 组件 id | - | 滚动字幕 |

### `class` 类名 `Charts`

图表（经 `$app->charts()` 取得）

| 方法名 | 参数 | 返回值 | 说明 |
| :--- | :--- | :--- | :--- |
| `lineChart` | `string` `$id` 组件 id <br> `array` `$series` [name, values, color?] | - | 折线图 |
| `barChart` | `string` `$id` 组件 id <br> `array` `$series` [name, values, color?] | - | 柱状图 |
| `donutChart` | `string` `$id` 组件 id <br> `string[]` `$labels` 分段标签 | - | 环形图 |
| `sparkline` | `float` `$width` = 100 宽度 <br> `float` `$height` = 24 高度 <br> `int` `$color` = 0 颜色 | - | 迷你折线 |
| `gauge` | `string` `$label` 标签 <br> `float` `$value` 值 <br> `float` `$size` = 120 尺寸 | - | 仪表盘 |
| `radialGauge` | `string` `$id` 组件 id <br> `float` `$value` 当前值 <br> `float` `$min` 最小值 <br> `float` `$max` 最大值 | - | 径向仪表 |
| `scatterChart` | `string` `$id` 组件 id <br> `array` `$series` 系列 | int | 返回 hover 的点索引 |
| `radarChart` | `string` `$id` 组件 id <br> `array` `$series` 系列 <br> `string[]` `$axes` 坐标轴 | int | 返回 hover 的系列索引 |
| `heatmap` | `string` `$id` 组件 id <br> `int` `$rows` 行数 <br> `int` `$cols` 列数 | int | 返回 hover 的格子索引 |
| `calendarHeatmap` | `string` `$id` 组件 id | int | 日历热力图 |
| `treemap` | `string` `$id` 组件 id <br> `array` `$items` 项 | int | 返回 hover/点击的项索引 |
| `funnel` | `string` `$id` 组件 id <br> `array` `$items` 项 | int | 漏斗图 |
| `sankey` | `string` `$id` 组件 id <br> `string[]` `$nodes` 节点 <br> `array` `$links` [from, to, value] | int | 桑基图 |
| `waffle` | `string` `$id` 组件 id <br> `array` `$items` 项 | int | 华夫图 |
| `splitBar` | `string` `$id` 组件 id <br> `array` `$items` 项 | int | 分段条 |
| `barList` | `string` `$id` 组件 id <br> `array` `$items` 项 | int | 条形列表 |
| `activityRings` | `string` `$id` 组件 id <br> `array` `$items` 项 | int | 活动环 |
| `candlestick` | `string` `$id` 组件 id <br> `array` `$candles` [open, high, low, close, volume] <br> `string[]` `$labels` 标签 | int | K 线图 |
| `waterfall` | `string` `$id` 组件 id <br> `array` `$requests` 请求 | array | 返回 clicked / activated / hovered |
| `dotChart` | `string` `$id` 组件 id <br> `array` `$series` 系列 | array | 返回 hovered / x / value / clicked |
| `timelineItem` | `string` `$title` 标题 | - | 时间线条目 |

### `class` 类名 `Content`

富文本、编辑器、聊天、节点、devtools（经 `$app->content()` 取得）

| 方法名 | 参数 | 返回值 | 说明 |
| :--- | :--- | :--- | :--- |
| `codeBlock` | `string` `$id` 组件 id <br> `string` `$code` 代码 <br> `string` `$lang` = "" 语言 <br> `string` `$title` = "" 标题 | - | 代码块 |
| `jsonView` | `string` `$id` 组件 id <br> `string` `$json` JSON 文本 | bool | 返回是否为合法 JSON |
| `markdown` | `string` `$id` 组件 id <br> `string` `$text` 文本 | array | markdown 渲染；返回链接点击等 |
| `codeEditor` | `string` `$id` 组件 id <br> `string` `$code` 代码 <br> `string` `$lang` = "" 语言 | array | buffer 往返；返回文本与状态 |
| `codeEditorGoto` | `string` `$id` 组件 id <br> `int` `$line` 行号 <br> `int` `$column` = 1 列号（从 1 起） | - | 移动光标 |
| `codeEditorSelect` | `string` `$id` 组件 id <br> `int` `$anchor` 选区锚点 <br> `int` `$cursor` 光标 | - | 设置选区 |
| `codeEditorFind` | `string` `$id` 组件 id <br> `string` `$text` 查找文本 | - | 在编辑器内查找 |
| `diffView` | `string` `$id` 组件 id <br> `string` `$old` 旧文本 <br> `string` `$new` 新文本 | array | 返回 added / removed / changes / current |
| `hexView` | `string` `$id` 组件 id | array | 返回选中区间 |
| `hexViewGoto` | `string` `$id` 组件 id <br> `int` `$offset` 字节偏移 | - | 滚动到指定偏移 |
| `terminalInit` | `int` `$textCap` = 65536 文本上限 <br> `int` `$lineCap` = 4096 行数上限 | - | 单例初始化，必须先于 terminal() |
| `terminalWrite` | `string` `$text` 文本 | - | 写入终端 |
| `terminalClear` | - | - | 清空终端 |
| `terminal` | `string` `$id` 组件 id | array | 返回 command / interrupt |
| `logInit` | `int` `$textCap` = 65536 文本上限 <br> `int` `$lineCap` = 2048 行数上限 | - | 单例初始化，必须先于 logView() |
| `logAppend` | `int` `$level` 级别 0..4 <br> `string` `$text` 文本 | - | 追加日志 |
| `logClear` | - | - | 清空日志 |
| `logView` | `string` `$id` 组件 id <br> `bool` `$timestamps` = false 是否显示时间戳 | - | 日志视图 |
| `commandPalette` | `string` `$id` 组件 id <br> `bool` `$open` 是否展开 <br> `array` `$commands` 命令项 | array | 返回 picked / open |
| `tour` | `string` `$id` 组件 id <br> `array` `$steps` 步骤 <br> `int` `$step` 步骤索引 | array | 返回 event / step |
| `gantt` | `string` `$id` 组件 id <br> `array` `$tasks` 任务 <br> `int` `$days` 天数 | int | 返回 hover 的任务索引 |
| `fileBrowser` | `string` `$id` 组件 id <br> `string` `$path` 当前路径 | array | 返回 path / picked / selected / navigated / error |
| `filePicker` | `string` `$id` 组件 id <br> `bool` `$open` 是否展开 <br> `string` `$path` = "" 当前路径 | array | 返回 open / path |
| `imageLoad` | `string` `$path` 图片路径 | array | 返回 tex / w / h；用完调 imageDestroy |
| `imageCreate` | `int` `$width` 宽度 <br> `int` `$height` 高度 | int | 由 RGBA 数据建纹理，返回 tex |
| `imageDestroy` | `int` `$tex` 纹理 id | - | 释放纹理 |
| `image` | `int` `$tex` 纹理 id | - | 绘制已加载纹理 |
| `imageViewer` | `string` `$id` 组件 id <br> `int` `$tex` 纹理 id <br> `int` `$width` 宽度 <br> `int` `$height` 高度 | array | 返回 zoom / cx / cy |
| `cropper` | `string` `$id` 组件 id <br> `int` `$tex` 纹理 id <br> `int` `$width` 宽度 <br> `int` `$height` 高度 | array | 返回裁剪矩形 x / y / w / h |
| `emoji` | `string` `$text` emoji <br> `float` `$size` = 0 尺寸 | - | 绘制 emoji |
| `emojiPicker` | `string` `$id` 组件 id | array | 返回 picked |
| `emojiSupported` | `string` `$text` emoji | bool | 该 emoji 能否渲染 |
| `emojiFontLoadFile` | `string` `$path` 字体路径 | bool | 从文件加载 emoji 字体 |
| `emojiFontLoadSystem` | - | bool | 加载系统 emoji 字体（仅影响 emoji 子系统） |
| `chatMessage` | `string` `$text` 文本 <br> `bool` `$outgoing` = false 是否自己发出 <br> `string` `$author` = "" 作者 <br> `bool` `$markdown` = false 是否按 markdown 渲染 | - | 聊天气泡 |
| `chatDivider` | `string` `$label` 标签 | - | 聊天分隔（日期等） |
| `typingIndicator` | `string` `$name` 名称 | - | 对方正在输入 |
| `comment` | `string` `$id` 组件 id <br> `string` `$author` 作者 <br> `string` `$text` 文本 | int | 返回交互索引 |
| `presence` | `string` `$id` 组件 id <br> `array` `$users` 成员 <br> `int` `$maxVisible` = 3 最多显示数 <br> `float` `$size` = 32 尺寸 | - | 在线成员 |
| `signaturePad` | `string` `$id` 组件 id | bool | 返回是否有新笔迹 |
| `signaturePoints` | `string` `$id` 组件 id | array | 返回 points / strokes |
| `signatureClear` | `string` `$id` 组件 id | - | 清空签名 |
| `nodeEditorBegin` | `string` `$id` 组件 id | array\|bool | 返回 false 或 x / y / zoom |
| `nodeEditorEnd` | - | - | 结束节点编辑器 |
| `nodeBegin` | `int` `$nodeId` 节点 id <br> `float` `$x` X 坐标 <br> `float` `$y` Y 坐标 <br> `string` `$title` 标题 | array | 返回 x / y / selected |
| `nodeEnd` | - | - | 结束节点 |
| `nodeInput` | `int` `$pinId` 引脚 id <br> `string` `$label` 标签 <br> `int` `$type` = 0 类型 | bool | 返回引脚是否被点击 |
| `nodeOutput` | `int` `$pinId` 引脚 id <br> `string` `$label` 标签 <br> `int` `$type` = 0 类型 | bool | 返回引脚是否被点击 |
| `nodeContentBegin` | - | - | 节点内容开始 |
| `nodeContentEnd` | - | - | 节点内容结束 |
| `nodeLink` | `int` `$linkId` 连线 id <br> `int` `$fromPin` 起始引脚 <br> `int` `$toPin` 目标引脚 | bool | 返回是否被点击 |
| `nodeLinkCreated` | - | array\|null | 无事件返回 null |
| `nodeLinkRemoved` | - | int\|null | 无事件返回 null，否则连线 id |
| `nodeDeleted` | - | int\|null | 无事件返回 null，否则节点 id |
| `testExplorer` | `string` `$id` 组件 id <br> `array` `$items` 项 | array | 测试浏览器 |
| `stackTrace` | `string` `$id` 组件 id <br> `string` `$title` 标题 <br> `string` `$message` 消息 <br> `array` `$frames` 栈帧 <br> `string` `$lang` = "" 代码块语言 | array | 返回 clicked |
| `requestBar` | `string` `$id` 组件 id <br> `string` `$url` URL <br> `int` `$method` = 0 方法枚举值 <br> `bool` `$loading` = false 是否加载中 <br> `string` `$placeholder` = "" 占位文本 | array | HTTP 请求栏 |
| `responseStatus` | `int` `$status` 状态码 <br> `float` `$seconds` 耗时秒 <br> `float` `$bytes` 字节数 | - | 响应状态徽标 |
| `uptime` | `string` `$id` 组件 id <br> `array` `$services` 服务 | array | 返回 service / day |
| `timeHistogram` | `string` `$id` 组件 id <br> `float` `$start` 起点 <br> `float` `$bucketSeconds` 桶秒数 <br> `int` `$buckets` 桶数 <br> `array` `$series` 系列 <br> `float` `$selStart` = 0 选中起点 <br> `float` `$selEnd` = 0 选中终点 | array | 返回选中区间 |
| `coverage` | `string` `$id` 组件 id <br> `array` `$files` 文件 | array | 返回 activated / lines |
| `blame` | `string` `$id` 组件 id <br> `string[]` `$lines` 行文本 <br> `int[]` `$lineCommit` 行所属提交 <br> `array` `$commits` 提交 | array | 返回 clicked / hovered |
| `featureFlags` | `string` `$id` 组件 id <br> `array` `$flags` 标志 <br> `string[]` `$envs` 环境 | array | 返回 flag / env / flags |
| `apiExplorer` | `string` `$id` 组件 id <br> `array` `$endpoints` 端点 | array | 返回 selected / try_it |
| `readout` | `string` `$label` 标签 <br> `string` `$value` 值 | - | 数字读数 |
| `dotDisplay` | `string` `$text` 文本 | - | 点阵显示 |

## `enum` 枚举

### `enum` 枚举名 `ButtonVariant` : `int`

| case | 值 | 说明 |
| :--- | :--- | :--- |
| `Default` | 0 | 默认样式 |
| `Secondary` | 1 | 次要 |
| `Primary` | 2 | 主要 |
| `Outline` | 3 | 描边 |
| `Ghost` | 4 | 幽灵（无背景） |
| `Danger` | 5 | 危险 |
| `Link` | 6 | 链接样式 |


### `enum` 枚举名 `FontRole` : `int`

| case | 值 | 说明 |
| :--- | :--- | :--- |
| `Body` | 0 | 正文字体（Inter Regular） |
| `Medium` | 1 | 中等字重（Inter Medium） |
| `Display` | 2 | 标题字体（Geist Medium） |
| `Mono` | 3 | 等宽字体（Geist Mono） |
| `Icons` | 4 | 图标字体（Lucide） |

### `enum` 枚举名 `LayoutType` : `int`

| case | 值 | 说明 |
| :--- | :--- | :--- |
| `Column` | 0 | 纵向容器 |
| `Row` | 1 | 横向容器 |
| `Box` | 2 | 通用盒子 |
| `Scroll` | 3 | 滚动容器 |
| `Split` | 4 | 分割容器 |
