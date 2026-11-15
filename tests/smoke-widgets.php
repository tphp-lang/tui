<?php
/**
 * tphp/tui 冒烟：反馈与显示小件层（Task 2）
 * hidden 窗口一帧内调用全部 widgets 方法 + 返回值断言。
 * 运行：build\tphp.exe run tests\smoke-widgets.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

$app = new \Tphp\Tui\App("tui-widgets", 640, 520, true);
$app->frame();
$app->heading("Widgets smoke");

// 文本扩展
$app->muted("muted text");
$app->code("echo 1;");
$app->kbd("Ctrl");
check($app->link("click me") === false, "link-bool");
$app->icon("home", 16, 0xFF3B82F6);
$app->tooltip("tip");
$app->gap(8);

// 卡片
$app->cardBegin();
$app->text("inside card");
$app->cardEnd();

// 徽章头像
$app->badge("NEW");
$app->avatar("Ada");
$app->avatarGroup(["A", "B", "C", "D"], 3, 32);

// 进度反馈
$app->progress(0.42);
$app->progressRing("CPU", 0.66, 48);
$app->spinner(24);
$app->skeleton(20);
$app->loader();
check($app->emptyState("Nothing here") === false, "emptyState-bool");
$app->metric("Requests", "1.2k");
$app->meter("Disk", 60, 100, "%");

// 按钮族
check($app->iconButton("gear") === false, "iconButton-bool");
check($app->copyButton("cb1", "hello") === false, "copyButton-bool");
check($app->holdButton("Hold me") === false, "holdButton-bool");
check($app->confirmButton("Delete") === false, "confirmButton-bool");
$app->buttonGroupBegin();
$app->button("One");
$app->button("Two");
$app->buttonGroupEnd();
check($app->splitButton("Export", ["CSV", "PDF"]) === -1, "splitButton-int");
check($app->statusButton("Build", 2) === false, "statusButton-bool");
check($app->reactions("rx1", [
    ["icon" => "\u{1F44D}", "count" => 3, "active" => false],
    ["icon" => "\u{2764}", "count" => 1],
]) === -1, "reactions-int");

// 文本特效
$app->shimmerText("Loading...");
check($app->typewriter("tw1", "hello world") === false, "typewriter-bool");
$app->textReveal("tr1", "revealed");
$app->wordRotate("wr1", ["alpha", "beta", "gamma"], 2.0);
$app->numberRoll("nr1", "42");
$app->gradientText("gradient");
$app->counter("cnt1", 1234);
$app->ticker("tk1", [
    ["label" => "BTC", "value" => "67,240", "delta" => 1.2, "lower_is_better" => false],
    ["label" => "Latency", "value" => "12ms", "delta" => -0.4, "lower_is_better" => true],
]);

$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();
echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
