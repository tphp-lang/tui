<?php
/**
 * tphp/tui 阶段 A 专测：SCOPE_IF 失配检测
 * 验证「begin 成功但漏写 End」在下一帧 frame() 入口抛 Error，不静默错位 → 随机 AV。
 * 运行：build\tphp.exe run tests\smoke-scopeif.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

// --- 1. dialogBegin(true) 后故意不调 dialogEnd → 下一帧抛 Error ---
$app = new \Tphp\Tui\App("scopeif", 480, 360, true);
$app->frame();
$open = true;
$begin = $app->dialogBegin("漏配对", $open);
check($begin === true, "dialog-begin-open");
// 故意漏掉 $app->dialogEnd();

$thrown = false;
$msg = "";
try {
    $app->frame();   // 渲染上一段前检测到未闭合 → 抛 Error
} catch (\Error $e) {
    $thrown = true;
    $msg = $e->getMessage();
}
check($thrown, "leaked-end-throws");
check(strpos($msg, "dialogBegin/dialogEnd") !== false, "error-names-pair");
echo "msg=$msg\n";

// --- 2. 抛错后标记已清，再 frame() 不再抛（catch 后可恢复） ---
$recovered = true;
try { $app->frame(); } catch (\Error $e) { $recovered = false; }
check($recovered, "recover-after-catch");

$app->quit();
$app->destroy();

// --- 3. begin 返回 false（未展开）时漏 End 属正常，不抛 ---
$app2 = new \Tphp\Tui\App("scopeif-closed", 480, 360, true);
$app2->frame();
$closed = false;
$b2 = $app2->dialogBegin("关态", $closed);
check($b2 === false, "dialog-begin-closed");
// 关态下不调 End 是允许的（g_dialog_on 本就 false）
$ok2 = true;
try { $app2->frame(); } catch (\Error $e) { $ok2 = false; }
check($ok2, "closed-begin-no-false-positive");
$app2->quit();
$app2->destroy();

echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
