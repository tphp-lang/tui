<?php
// tphp/tui 冒烟测试（自断言，exit code 0 = 全过）
// 前置：build\tphp.exe ext --add <本包目录> 链入后，用新起的进程运行
//   build\tphp.exe run tests/smoke.php
// 覆盖：常量注册 / 建窗(隐藏) / 一帧绘制（布局+文本+控件）/ quit 退出 /
//       destroy 幂等 / 销毁后再调用抛 Error（不静默）

$fail = 0;
function check(string $name, $want, $got): void
{
    global $fail;
    if ($want === $got) {
        echo "PASS $name\n";
    } else {
        $fail++;
        echo "FAIL $name want=", print_r($want, true),
             " got=", print_r($got, true), "\n";
    }
}

check("VERSION", "0.1.0", \Tphp\Tui\VERSION);
check("class_exists", true, class_exists(\Tphp\Tui\App::class));

// --- 枚举 ButtonVariant（替代历史 BUTTON_* 常量） ---
// 注意：本运行时无 enum_exists() / ReflectionEnum，用 class_exists + 值语义断言
check("enum_class_exists", true, class_exists(\Tphp\Tui\ButtonVariant::class));
check("case_primary_value", 2, \Tphp\Tui\ButtonVariant::Primary->value);
check("case_count", 7, count(\Tphp\Tui\ButtonVariant::cases()));
check("case_name", "Primary", \Tphp\Tui\ButtonVariant::Primary->name);
check("case_identity", true, \Tphp\Tui\ButtonVariant::Primary === \Tphp\Tui\ButtonVariant::Primary);
check("from", \Tphp\Tui\ButtonVariant::Primary, \Tphp\Tui\ButtonVariant::from(2));
check("tryFrom_bad", null, \Tphp\Tui\ButtonVariant::tryFrom(99));
check("instanceof", true, \Tphp\Tui\ButtonVariant::Danger instanceof \Tphp\Tui\ButtonVariant);
check("danger_value", 5, \Tphp\Tui\ButtonVariant::Danger->value);

// --- 枚举 FontRole（替代历史 FONT_* 常量） ---
check("fontrole_exists", true, class_exists(\Tphp\Tui\FontRole::class));
check("fontrole_body", 0, \Tphp\Tui\FontRole::Body->value);
check("fontrole_icons", 4, \Tphp\Tui\FontRole::Icons->value);
check("fontrole_count", 5, count(\Tphp\Tui\FontRole::cases()));
check("fontrole_name", "Display", \Tphp\Tui\FontRole::Display->name);
check("fontrole_from", \Tphp\Tui\FontRole::Mono, \Tphp\Tui\FontRole::from(3));

// hidden 窗口：给测试用，不抢焦点
$app = new \Tphp\Tui\App("tui-smoke", 420, 300, true);

check("frame1", true, $app->frame());

// 一帧完整绘制（对应 CUI examples/hello.c 的组件面）
$app->layout()->columnBegin(32, 16);
$app->heading("Hello, CUI", 2);
$app->secondary("native window driven from PHP");
$on = $app->widgets()->toggle("Notifications", true);
$app->layout()->rowBegin(8);
$clicked = $app->widgets()->button("Click me", \Tphp\Tui\ButtonVariant::Primary);
$app->text("Clicked 0 times");
$app->layout()->boxEnd();
$app->label("label");
$app->separator();
$app->spacer();
$app->layout()->boxEnd();

check("toggle keeps value (idle)", true, $on);
check("button idle returns false", false, $clicked);
echo "drawn\n";

// --- 字体 API（CJK 支持） ---
// fontLoadSystem 在无中文字体的机器上返回 -1，属预期，不算失败
$cjk = $app->fontLoadSystem();
check("fontLoadSystem returns id or -1", true, $cjk >= -1);
if ($cjk >= 0) {
    $app->fontAddFallback(\Tphp\Tui\FontRole::Body, $cjk);
    $app->fontAddFallback(\Tphp\Tui\FontRole::Medium, $cjk);
    echo "cjk-font={$cjk}\n";
}
try {
    $app->fontLoad("/no/such/font.ttf");
    check("fontLoad missing throws", "throws", "no-throw");
} catch (\RuntimeException $e) {
    check("fontLoad missing throws", "throws", "throws");
}
try {
    $app->fontSetRole(99, 0);
    check("fontSetRole bad role throws", "throws", "no-throw");
} catch (\ValueError $e) {
    check("fontSetRole bad role throws", "throws", "throws");
}

// --- CJK 断行：窄容器里长中文必须折成多行（cui_text.c CJK break points） ---
function ink_lines($app, $text) {
    $app->text($text);
    $s = $app->screenshotRgba();
    $w = $s["w"]; $h = $s["h"]; $d = $s["data"];
    $bg = substr($d, 0, 4);
    $lines = 0; $has = false;
    for ($y = 0; $y < $h; $y++) {
        $row = false;
        for ($x = 0; $x < $w; $x++) {
            if (substr($d, ($y * $w + $x) * 4, 4) !== $bg) { $row = true; break; }
        }
        if ($row && !$has) { $lines++; $has = true; }
        if (!$row) $has = false;
    }
    return $lines;
}
// 注意：必须装 CJK 字体后再测——无字形时汉字是等宽豆腐块，扫描行连成一片，
// 行数反而恒为 1（这正是「没字体」的另一种表征）。
$wrapApp = new \Tphp\Tui\App("wrap", 240, 400, true);
$wf = $wrapApp->fontLoadSystem();
if ($wf >= 0) {
    $wrapApp->fontAddFallback(\Tphp\Tui\FontRole::Body, $wf);
    $wrapApp->frame();
    $narrow = ink_lines($wrapApp, "中文断行测试需要把很长的一段中文正确地折成多行显示");
    check("cjk wraps into multiple lines", true, $narrow >= 2);
} else {
    echo "SKIP cjk wraps (no system CJK font)\n";
}
$wrapApp->destroy();

$app->quit();
check("frame2 after quit", false, $app->frame());

$app->destroy();
check("destroy idempotent", null, $app->destroy());
try {
    $app->frame();
    check("frame after destroy", "throws", "no-throw");
} catch (\Error $e) {
    check("frame after destroy", "throws", "throws");
}

echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);