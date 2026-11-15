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
$app->boxBegin(4, 4);
$app->text("box content");
$app->boxEnd();
$app->scrollBegin("sc1", false);
$app->text("scroll content");
$app->scrollEnd();
$app->scrollTo("sc1", 0);
$app->splitBegin("sp1", 0.5);
$app->columnBegin(0, 0);
$app->text("left");
$app->boxEnd();
$app->splitNext();
$app->columnBegin(0, 0);
$app->text("right");
$app->boxEnd();
check($app->splitEnd() === 0.5, "split-ratio");

// 导航
check($app->tabs("tb1", ["Main", "Logs"], 0) === 0, "tabs-index");
check($app->navItem("Dashboard") === false, "navItem-bool");
$app->navSection("System");
check($app->breadcrumb(["Home", "Settings", "Users"]) === -1, "breadcrumb-int");
check($app->pagination("pg1", 0, 5) === 0, "pagination-page");
$app->stepper(["Plan", "Build", "Ship"], 1);
$pb = $app->pathBar("pb1", ["src", "core", "vm.c"]);
check(is_array($pb) && array_key_exists("clicked", $pb) && array_key_exists("submitted", $pb), "pathBar-result");
check($app->pathBarMenu("pb1") === -1, "pathBarMenu-int");
$app->pathBarMenuEnd();
check($app->toc("toc1", [
    ["title" => "Intro", "anchor" => "intro", "level" => 0],
    ["title" => "API", "anchor" => "api", "level" => 1],
]) >= -1, "toc-int");
$app->tocAnchor("intro");

// 覆盖层
check($app->popoverBegin("po1") === false, "popoverBegin-bool");
$app->popoverEnd();
$app->popoverOpen("po1");
$app->popoverClose("po1");
$app->popoverToggle("po1");   // 关→开
check($app->popoverIsOpen("po1") === true, "popoverIsOpen-after-toggle");
check($app->menuBegin("Menu") === false, "menuBegin-bool");
$app->menuEnd();
$app->menuItem("Open");
$app->menuSeparator();
$app->menuLabel("Recent");
check($app->contextMenuBegin("cm1") === false, "contextMenu-bool");
$app->contextMenuEnd();
$db = $app->dialogBegin("Confirm", true);
check($db === true, "dialog-open-true");
$app->dialogFooterBegin();
$app->button("OK");
$app->dialogFooterEnd();
$app->dialogEnd();
check($app->sheetBegin("Sheet", false) === false, "sheet-false");
$app->sheetEnd();
$th = $app->toast("Saved");
check(is_int($th) && $th > 0, "toast-handle");
check($app->toastActionClicked($th) === false, "toastAction-bool");
$app->toastDismiss($th);
check($app->alert("Heads up") === true, "alert-visible");   // 未 dismiss 前返回 true
check($app->banner("Update available", true) === true, "banner-open");
check($app->hoverCardBegin("hc1") === false, "hoverCard-bool");
$app->hoverCardEnd();
check($app->userMenuBegin("kllxs") === false, "userMenu-bool");
$app->userMenuEnd();
check($app->speedDial("sd1", [
    ["icon" => "plus", "label" => "New"],
    ["icon" => "edit", "label" => "Edit"],
]) === -1, "speedDial-int");
$app->spotlightBegin("sp-light");
$app->text("spotlit");
$app->spotlightEnd();

// 列表/树
check($app->accordionBegin("Advanced", false) === false, "accordion-false");
$app->accordionEnd();
check($app->treeBegin("Root", true) === true, "tree-open");
check($app->treeClicked() === false, "treeClicked-bool");
$app->treeEnd();
check($app->listItem("Row 1") === false, "listItem-bool");
$vl = $app->virtualList("vl1", 100, 24, 200);
check(is_array($vl) && array_key_exists("first", $vl) && array_key_exists("last", $vl), "virtualList-range");
$app->virtualListEnd();
$app->swipeItemBegin("sw1", [["icon" => "trash", "label" => "Delete"]], [["icon" => "pin", "label" => "Pin"]]);
$app->text("swipe row");
check($app->swipeItemEnd() === -1, "swipeItem-int");
$app->reorderBegin("ro1", [2, 0, 1]);
$app->text("a");
$app->text("b");
$app->text("c");
$ro = $app->reorderEnd();
check(is_array($ro) && $ro === [2, 0, 1], "reorder-roundtrip");

// 表格
$cols = [
    ["title" => "Name", "sortable" => true],
    ["title" => "Role", "align" => 2],
    ["title" => "Age"],
];
check($app->tableBegin("t1", $cols) === true, "tableBegin-true");
for ($i = 0; $i < 3; $i++) {
    $app->tableRow($i === 0);
    $app->tableCell();
    $app->tableCellText("row$i");
    $app->tableCell();
    $app->tableCellText("dev");
    $app->tableCell();
    $app->tableCellText((string)(20 + $i));
}
$app->tableEnd();
$dg = $app->dataGridBegin("dg1", $cols, 50);
check(is_array($dg) && array_key_exists("first", $dg), "dataGridBegin-range");
foreach (range($dg["first"], $dg["last"]) as $r) {
    $app->dataGridRow($r);
    $app->dataGridCell();
    $app->dataGridCellText("v$r");
    $app->dataGridCell();
    $app->dataGridCellText((string)$r);
    $app->dataGridCell();
    $app->dataGridCellText((string)($r * 2));
}
$app->dataGridEnd();
check($app->treeCell("folder", 0, true, false) === false, "treeCell-false");
$nodes = [
    ["parent" => -1, "first_child" => 1, "next_sibling" => -1, "flags" => 0],
    ["parent" => 0, "first_child" => -1, "next_sibling" => 2, "flags" => 0],
    ["parent" => 0, "first_child" => -1, "next_sibling" => -1, "flags" => 0],
];
$app->treeTableBegin("tt1", [["title" => "Tree"], ["title" => "Value"]], $nodes);
while (($node = $app->treeTableRow()) >= 0) {
    $app->treeTableName("node$node");
    $app->tableCell();
    $app->tableCellText("v$node");
}
$tev = $app->treeTableEnd();
check(is_array($tev) && array_key_exists("activated", $tev), "treeTableEnd-events");
$fb = $app->filterBar("fb1", [
    ["name" => "Status", "options" => ["open", "closed"]],
    ["name" => "Owner", "icon" => "user"],
], [0, 0], "");
check(is_array($fb) && array_key_exists("changed", $fb) && array_key_exists("selected", $fb), "filterBar-result");

// 工具条/状态栏/菜单栏
$app->toolbarBegin("tl1");
$app->toolbarButton("save");
$app->toolbarSeparator();
$app->toolbarSpacer();
$app->toolbarButton("settings");
$app->toolbarEnd();
$app->statusBarBegin("sb1");
$app->statusBarSpacer();
$app->statusBarText("Ready");
check($app->statusBarItem("git: main") === false, "statusBarItem-bool");
$app->statusBarProgress(0.5);
$app->statusBarEnd();
$app->menuBarBegin("mb1");
check($app->menuBarMenuBegin("File") === false, "menuBarMenu-bool");
$app->menuItem("New");
$app->menuBarMenuEnd();
$app->menuBarEnd();

// 停靠
$app->dockBegin("dk1");
check($app->dockItem("home", "Home") === false, "dockItem-bool");
$app->dockSeparator();
$app->dockEnd();
$app->dockspaceBegin("ds1");
$app->dockPanelBegin("Inspector", true);
$app->text("panel");
$app->dockPanelEnd();
$app->dockspaceEnd();
$dockJson = $app->dockSave("ds1");
check(is_string($dockJson) && strlen($dockJson) > 0, "dockSave-string");
check($app->dockLoad("ds1", $dockJson) === true, "dockLoad-bool");

// 轮播/看板/卡片栈
check($app->carouselBegin("cr1", 3, 0) === 0, "carousel-index");
$app->carouselSlideBegin();
$app->text("slide 1");
$app->carouselSlideEnd();
$app->carouselEnd();
$app->kanbanBegin("kb1");
$app->kanbanColumnBegin("Todo", 1);
$app->kanbanCardBegin(101);
$app->text("task A");
$app->kanbanCardEnd();
$app->kanbanColumnEnd();
$move = $app->kanbanEnd();
check($move === null || is_array($move), "kanbanEnd-null-or-array");
check($app->expandableCardBegin("More", false) === false, "expandableCard-false");
$app->expandableCardEnd();
$app->cardStackBegin("cs1", 2);
$app->cardStackItemBegin(0);
$app->text("stack 1");
$app->cardStackItemEnd();
$app->cardStackItemBegin(1);
$app->text("stack 2");
$app->cardStackItemEnd();
$app->cardStackEnd();

$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();
echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
