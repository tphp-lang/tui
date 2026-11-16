<?php
/**
 * tphp/tui 冒烟：容器与导航层（Task 4）
 * hidden 窗口一帧内调用全部容器方法，重点断言 begin/end 配对与返回值。
 * 运行：build\tphp.exe run tests\smoke-container.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

$app = new \Tphp\Tui\App("tui-container", 760, 600, true);
$app->frame();
$app->heading("Container smoke");

// 容器
$app->layout()->boxBegin(4, 4);
$app->text("box content");
$app->layout()->boxEnd();
$app->layout()->scrollBegin("sc1", false);
$app->text("scroll content");
$app->layout()->scrollEnd();
$app->layout()->scrollTo("sc1", 0);
$app->layout()->splitBegin("sp1", 0.5);
$app->layout()->columnBegin(0, 0);
$app->text("left");
$app->layout()->boxEnd();
$app->layout()->splitNext();
$app->layout()->columnBegin(0, 0);
$app->text("right");
$app->layout()->boxEnd();
check($app->layout()->splitEnd() === 0.5, "split-ratio");

// 导航
check($app->layout()->tabs("tb1", ["Main", "Logs"], 0) === 0, "tabs-index");
check($app->layout()->navItem("Dashboard") === false, "navItem-bool");
$app->layout()->navSection("System");
check($app->layout()->breadcrumb(["Home", "Settings", "Users"]) === -1, "breadcrumb-int");
check($app->layout()->pagination("pg1", 0, 5) === 0, "pagination-page");
$app->layout()->stepper(["Plan", "Build", "Ship"], 1);
$pb = $app->layout()->pathBar("pb1", ["src", "core", "vm.c"]);
check(is_array($pb) && array_key_exists("clicked", $pb) && array_key_exists("submitted", $pb), "pathBar-result");
check($app->layout()->pathBarMenu("pb1") === -1, "pathBarMenu-int");
$app->layout()->pathBarMenuEnd();
check($app->layout()->toc("toc1", [
    ["title" => "Intro", "anchor" => "intro", "level" => 0],
    ["title" => "API", "anchor" => "api", "level" => 1],
]) >= -1, "toc-int");
$app->layout()->tocAnchor("intro");

// 覆盖层
check($app->layout()->popoverBegin("po1") === false, "popoverBegin-bool");
$app->layout()->popoverEnd();
$app->layout()->popoverOpen("po1");
$app->layout()->popoverClose("po1");
$app->layout()->popoverToggle("po1");   // 关→开
check($app->layout()->popoverIsOpen("po1") === true, "popoverIsOpen-after-toggle");
check($app->layout()->menuBegin("Menu") === false, "menuBegin-bool");
$app->layout()->menuEnd();
$app->layout()->menuItem("Open");
$app->layout()->menuSeparator();
$app->layout()->menuLabel("Recent");
check($app->layout()->contextMenuBegin("cm1") === false, "contextMenu-bool");
$app->layout()->contextMenuEnd();
$db = $app->layout()->dialogBegin("Confirm", true);
check($db === true, "dialog-open-true");
$app->layout()->dialogFooterBegin();
$app->widgets()->button("OK");
$app->layout()->dialogFooterEnd();
$app->layout()->dialogEnd();
check($app->layout()->sheetBegin("Sheet", false) === false, "sheet-false");
$app->layout()->sheetEnd();
$th = $app->layout()->toast("Saved");
check(is_int($th) && $th > 0, "toast-handle");
check($app->layout()->toastActionClicked($th) === false, "toastAction-bool");
$app->layout()->toastDismiss($th);
check($app->layout()->alert("Heads up") === true, "alert-visible");   // 未 dismiss 前返回 true
check($app->layout()->banner("Update available", true) === true, "banner-open");
check($app->layout()->hoverCardBegin("hc1") === false, "hoverCard-bool");
$app->layout()->hoverCardEnd();
check($app->layout()->userMenuBegin("kllxs") === false, "userMenu-bool");
$app->layout()->userMenuEnd();
check($app->layout()->speedDial("sd1", [
    ["icon" => "plus", "label" => "New"],
    ["icon" => "edit", "label" => "Edit"],
]) === -1, "speedDial-int");
$app->layout()->spotlightBegin("sp-light");
$app->text("spotlit");
$app->layout()->spotlightEnd();

// 列表/树
check($app->layout()->accordionBegin("Advanced", false) === false, "accordion-false");
$app->layout()->accordionEnd();
check($app->layout()->treeBegin("Root", true) === true, "tree-open");
check($app->layout()->treeClicked() === false, "treeClicked-bool");
$app->layout()->treeEnd();
check($app->layout()->listItem("Row 1") === false, "listItem-bool");
$vl = $app->layout()->virtualList("vl1", 100, 24, 200);
check(is_array($vl) && array_key_exists("first", $vl) && array_key_exists("last", $vl), "virtualList-range");
$app->layout()->virtualListEnd();
$app->layout()->swipeItemBegin("sw1", [["icon" => "trash", "label" => "Delete"]], [["icon" => "pin", "label" => "Pin"]]);
$app->text("swipe row");
check($app->layout()->swipeItemEnd() === -1, "swipeItem-int");
$app->layout()->reorderBegin("ro1", [2, 0, 1]);
$app->text("a");
$app->text("b");
$app->text("c");
$ro = $app->layout()->reorderEnd();
check(is_array($ro) && $ro === [2, 0, 1], "reorder-roundtrip");

// 表格
$cols = [
    ["title" => "Name", "sortable" => true],
    ["title" => "Role", "align" => 2],
    ["title" => "Age"],
];
check($app->layout()->tableBegin("t1", $cols) === true, "tableBegin-true");
for ($i = 0; $i < 3; $i++) {
    $app->layout()->tableRow($i === 0);
    $app->layout()->tableCell();
    $app->layout()->tableCellText("row$i");
    $app->layout()->tableCell();
    $app->layout()->tableCellText("dev");
    $app->layout()->tableCell();
    $app->layout()->tableCellText((string)(20 + $i));
}
$app->layout()->tableEnd();
$dg = $app->layout()->dataGridBegin("dg1", $cols, 50);
check(is_array($dg) && array_key_exists("first", $dg), "dataGridBegin-range");
foreach (range($dg["first"], $dg["last"]) as $r) {
    $app->layout()->dataGridRow($r);
    $app->layout()->dataGridCell();
    $app->layout()->dataGridCellText("v$r");
    $app->layout()->dataGridCell();
    $app->layout()->dataGridCellText((string)$r);
    $app->layout()->dataGridCell();
    $app->layout()->dataGridCellText((string)($r * 2));
}
$app->layout()->dataGridEnd();
check($app->layout()->treeCell("folder", 0, true, false) === false, "treeCell-false");
$nodes = [
    ["parent" => -1, "first_child" => 1, "next_sibling" => -1, "flags" => 0],
    ["parent" => 0, "first_child" => -1, "next_sibling" => 2, "flags" => 0],
    ["parent" => 0, "first_child" => -1, "next_sibling" => -1, "flags" => 0],
];
$app->layout()->treeTableBegin("tt1", [["title" => "Tree"], ["title" => "Value"]], $nodes);
while (($node = $app->layout()->treeTableRow()) >= 0) {
    $app->layout()->treeTableName("node$node");
    $app->layout()->tableCell();
    $app->layout()->tableCellText("v$node");
}
$tev = $app->layout()->treeTableEnd();
check(is_array($tev) && array_key_exists("activated", $tev), "treeTableEnd-events");
$fb = $app->layout()->filterBar("fb1", [
    ["name" => "Status", "options" => ["open", "closed"]],
    ["name" => "Owner", "icon" => "user"],
], [0, 0], "");
check(is_array($fb) && array_key_exists("changed", $fb) && array_key_exists("selected", $fb), "filterBar-result");

// 工具条/状态栏/菜单栏
$app->layout()->toolbarBegin("tl1");
$app->layout()->toolbarButton("save");
$app->layout()->toolbarSeparator();
$app->layout()->toolbarSpacer();
$app->layout()->toolbarButton("settings");
$app->layout()->toolbarEnd();
$app->layout()->statusBarBegin("sb1");
$app->layout()->statusBarSpacer();
$app->layout()->statusBarText("Ready");
check($app->layout()->statusBarItem("git: main") === false, "statusBarItem-bool");
$app->layout()->statusBarProgress(0.5);
$app->layout()->statusBarEnd();
$app->layout()->menuBarBegin("mb1");
check($app->layout()->menuBarMenuBegin("File") === false, "menuBarMenu-bool");
$app->layout()->menuItem("New");
$app->layout()->menuBarMenuEnd();
$app->layout()->menuBarEnd();

// 停靠
$app->layout()->dockBegin("dk1");
check($app->layout()->dockItem("home", "Home") === false, "dockItem-bool");
$app->layout()->dockSeparator();
$app->layout()->dockEnd();
$app->layout()->dockspaceBegin("ds1");
$app->layout()->dockPanelBegin("Inspector", true);
$app->text("panel");
$app->layout()->dockPanelEnd();
$app->layout()->dockspaceEnd();
$dockJson = $app->layout()->dockSave("ds1");
check(is_string($dockJson) && strlen($dockJson) > 0, "dockSave-string");
check($app->layout()->dockLoad("ds1", $dockJson) === true, "dockLoad-bool");

// 轮播/看板/卡片栈
check($app->layout()->carouselBegin("cr1", 3, 0) === 0, "carousel-index");
$app->layout()->carouselSlideBegin();
$app->text("slide 1");
$app->layout()->carouselSlideEnd();
$app->layout()->carouselEnd();
$app->layout()->kanbanBegin("kb1");
$app->layout()->kanbanColumnBegin("Todo", 1);
$app->layout()->kanbanCardBegin(101);
$app->text("task A");
$app->layout()->kanbanCardEnd();
$app->layout()->kanbanColumnEnd();
$move = $app->layout()->kanbanEnd();
check($move === null || is_array($move), "kanbanEnd-null-or-array");
check($app->layout()->expandableCardBegin("More", false) === false, "expandableCard-false");
$app->layout()->expandableCardEnd();
$app->layout()->cardStackBegin("cs1", 2);
$app->layout()->cardStackItemBegin(0);
$app->text("stack 1");
$app->layout()->cardStackItemEnd();
$app->layout()->cardStackItemBegin(1);
$app->text("stack 2");
$app->layout()->cardStackItemEnd();
$app->layout()->cardStackEnd();

$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();
echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
