<?php
/**
 * tphp/tui 阶段 B.4 专测：Layout 容器 depth 配平检测
 * 验证「columnBegin 后漏写 boxEnd」在下一帧 frame() 入口抛 Error。
 * 注意：depth 失配意味着 CUI node 栈已失衡，该 App 不可恢复——每个场景用独立实例。
 * 运行：build\tphp.exe run tests\smoke-depth.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

// --- 1. 漏写 boxEnd → 下一帧抛 Error ---
$app = new \Tphp\Tui\App("depth", 480, 360, true);
$app->frame();
$app->layout()->columnBegin(8, 8);
$app->text("未闭合");
// 故意漏掉 $app->layout()->boxEnd();

$thrown = false;
$msg = "";
try {
    $app->frame();
} catch (\Error $e) {
    $thrown = true;
    $msg = $e->getMessage();
}
check($thrown, "leaked-container-throws");
check(strpos($msg, "unclosed Layout container") !== false, "error-mentions-depth");
echo "msg=$msg\n";
$app->destroy();   // 失衡上下文直接销毁，不继续 frame

// --- 2. 正确配对不误报（独立实例）---
$app2 = new \Tphp\Tui\App("depth-ok", 480, 360, true);
$ok = true;
try {
    $app2->frame();
    $app2->layout()->columnBegin(8, 8);
    $app2->text("配对正确");
    $app2->layout()->boxEnd();
    $app2->frame();
} catch (\Error $e) { $ok = false; }
check($ok, "balanced-no-false-positive");
$app2->quit();
$app2->destroy();

// --- 3. 多 end 抛错（独立实例）---
$app3 = new \Tphp\Tui\App("depth-over", 480, 360, true);
$over = false;
try {
    $app3->frame();
    $app3->layout()->boxEnd();   // 无对应 begin
    $app3->frame();
} catch (\Error $e) { $over = true; }
check($over, "extra-end-throws");
$app3->destroy();

echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
