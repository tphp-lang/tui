<?php
/**
 * tphp/tui 冒烟：App 补全 + 交互测试面（Task 7）
 *
 * 实测机制（见 doc/coverage.md「测试面」）：
 *   - 每段（两次 frame() 之间）布局只含本段调用的组件；rect 在 present 时写入
 *     组件 state；last_id 每段开始清零 → 读 lastRect 前必须先调用目标组件；
 *   - 输入按「上一段布局」的 rect 命中（prev-rect 模型）；
 *   - frame() 阻塞等事件，simulate* 注入即事件，用于泵帧；
 *   - 点击成立时序（探针实测）：hover 落段 → down →（下段）→ up 与组件调用
 *     同段 → 该次调用返回 true。
 * 运行：build\tphp.exe run tests\smoke-interaction.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

$app = new \Tphp\Tui\App("tui-interaction", 480, 360, true);

// F1：intro（本段含 heading+button）
$app->frame();
$app->heading("Interaction smoke");
$app->button("Hit me");
$app->simulateMouse(1, 1);   // 注入以唤醒 frame()
$app->frame();

// --- 查询/主题（无副作用；fileDialog/openUrl 只注册不调用，见 coverage.md） ---
$dpi = $app->dpiScale();
check(is_float($dpi) && $dpi > 0, "dpiScale-positive");
$sz = $app->size();
check(is_array($sz) && $sz["w"] > 0 && $sz["h"] > 0, "size-positive");
$bk = $app->backend();
check(is_string($bk) && strlen($bk) > 0, "backend-string");
check(is_array($app->droppedFiles()), "droppedFiles-array");
$app->setTheme(1);
$app->setAccent(2);
$app->setReducedMotion(true);
$app->setTheme(0);
$app->setReducedMotion(false);

// --- 过渡到 button-only 布局段（每段只画 button，rect 才稳定） ---
$app->button("Hit me");
$app->simulateMouse(1, 1);
$app->frame();
$app->button("Hit me");
$r = $app->lastRect();                 // 上一段（button-only）的 rect
check(is_array($r) && $r["w"] > 0 && $r["h"] > 0, "lastRect-size");
$cx = $r["x"] + $r["w"] / 2;
$cy = $r["y"] + $r["h"] / 2;

// --- 注入点击按钮（探针时序） ---
$hit = false;
for ($i = 0; $i < 6 && !$hit; $i++) {
    $app->simulateMouse($cx, $cy);
    $app->frame();                     // 悬停落段
    $app->button("Hit me");
    $app->simulateMouseButton(0, true);
    $app->button("Hit me");            // 见 down → pressed
    $app->frame();
    $app->button("Hit me");
    $app->simulateMouseButton(0, false);
    $b = $app->button("Hit me");       // 见 up → clicked（实测 true）
    $app->frame();
    $hit = ($b === true);
    $app->button("Hit me");
    $r = $app->lastRect();
    if ($r["w"] > 0) { $cx = $r["x"] + $r["w"] / 2; $cy = $r["y"] + $r["h"] / 2; }
}
check($hit === true, "button-click-via-inject");

// --- toggle 翻转：另起全新 App 实例（同窗第二段阶梯会因残留输入状态失效，实测） ---
$app->destroy();
$app = new \Tphp\Tui\App("tui-interaction2", 480, 360, true);
$app->frame();
$app->toggle("Flag", false);
$app->simulateMouse(1, 1);
$app->frame();
$app->toggle("Flag", false);
$tr = $app->lastRect();
check($tr["w"] > 0, "toggle-rect");
$tx = $tr["x"] + $tr["w"] / 2;
$ty = $tr["y"] + $tr["h"] / 2;
$flipped = false;
for ($i = 0; $i < 6 && !$flipped; $i++) {
    $app->simulateMouse($tx, $ty);
    $app->frame();
    $app->toggle("Flag", false);
    $app->simulateMouseButton(0, true);
    $app->toggle("Flag", false);
    $app->frame();
    $app->toggle("Flag", false);
    $app->simulateMouseButton(0, false);
    $v = $app->toggle("Flag", false);
    $app->frame();
    $flipped = ($v === true);
    $app->toggle("Flag", false);
    $t2 = $app->lastRect();
    if ($t2["w"] > 0) { $tx = $t2["x"] + $t2["w"] / 2; $ty = $t2["y"] + $t2["h"] / 2; }
}
check($flipped === true, "toggle-flip-via-inject");

// --- 键盘/文本注入（验证不抛异常） ---
$app->simulateKey(65, true);
$app->simulateKey(65, false);
$app->simulateText("hi");
check(true, "key-text-inject");

// --- 截图：RGBA 自洽 + PNG 落盘合法 ---
$shot = $app->screenshotRgba();
check(is_array($shot) && $shot["w"] > 0 && $shot["h"] > 0
    && strlen($shot["data"]) === $shot["w"] * $shot["h"] * 4, "screenshotRgba-size");
$pngPath = __DIR__ . "/../shot-test.png";
$ok = $app->savePng($pngPath);
check($ok === true, "savePng-true");
if (file_exists($pngPath)) {
    $all = (string)file_get_contents($pngPath);
    check(substr($all, 0, 8) === "\x89PNG\r\n\x1a\n", "png-magic");
    unlink($pngPath);
} else {
    check(false, "png-magic");
}

$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();
echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
