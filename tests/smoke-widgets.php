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
$app->widgets()->muted("muted text");
$app->widgets()->code("echo 1;");
$app->widgets()->kbd("Ctrl");
check($app->widgets()->link("click me") === false, "link-bool");
$app->widgets()->icon("home", 16, 0xFF3B82F6);
$app->widgets()->tooltip("tip");
$app->widgets()->gap(8);

// 卡片
$app->widgets()->cardBegin();
$app->text("inside card");
$app->widgets()->cardEnd();

// 徽章头像
$app->widgets()->badge("NEW");
$app->widgets()->avatar("Ada");
$app->widgets()->avatarGroup(["A", "B", "C", "D"], 3, 32);

// 进度反馈
$app->widgets()->progress(0.42);
$app->widgets()->progressRing("CPU", 0.66, 48);
$app->widgets()->spinner(24);
$app->widgets()->skeleton(20);
$app->widgets()->loader();
check($app->widgets()->emptyState("Nothing here") === false, "emptyState-bool");
$app->widgets()->metric("Requests", "1.2k");
$app->widgets()->meter("Disk", 60, 100, "%");

// 按钮族
check($app->widgets()->iconButton("gear") === false, "iconButton-bool");
check($app->widgets()->copyButton("cb1", "hello") === false, "copyButton-bool");
check($app->widgets()->holdButton("Hold me") === false, "holdButton-bool");
check($app->widgets()->confirmButton("Delete") === false, "confirmButton-bool");
$app->widgets()->buttonGroupBegin();
$app->widgets()->button("One");
$app->widgets()->button("Two");
$app->widgets()->buttonGroupEnd();
check($app->widgets()->splitButton("Export", ["CSV", "PDF"]) === -1, "splitButton-int");
check($app->widgets()->statusButton("Build", 2) === false, "statusButton-bool");
check($app->widgets()->reactions("rx1", [
    ["icon" => "\u{1F44D}", "count" => 3, "active" => false],
    ["icon" => "\u{2764}", "count" => 1],
]) === -1, "reactions-int");

// 文本特效
$app->widgets()->shimmerText("Loading...");
check($app->widgets()->typewriter("tw1", "hello world") === false, "typewriter-bool");
$app->widgets()->textReveal("tr1", "revealed");
$app->widgets()->wordRotate("wr1", ["alpha", "beta", "gamma"], 2.0);
$app->widgets()->numberRoll("nr1", "42");
$app->widgets()->gradientText("gradient");
$app->widgets()->counter("cnt1", 1234);
$app->widgets()->ticker("tk1", [
    ["label" => "BTC", "value" => "67,240", "delta" => 1.2, "lower_is_better" => false],
    ["label" => "Latency", "value" => "12ms", "delta" => -0.4, "lower_is_better" => true],
]);

$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();
echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
