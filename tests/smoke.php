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
        echo "FAIL $name want=", var_export($want, true),
             " got=", var_export($got, true), "\n";
    }
}

check("VERSION", "0.1.0", \Tphp\Tui\VERSION);
check("BUTTON_PRIMARY", 2, \Tphp\Tui\BUTTON_PRIMARY);
check("class_exists", true, class_exists(\Tphp\Tui\App::class));

// hidden 窗口：给测试用，不抢焦点
$app = new \Tphp\Tui\App("tui-smoke", 420, 300, true);

check("frame1", true, $app->frame());

// 一帧完整绘制（对应 CUI examples/hello.c 的组件面）
$app->columnBegin(32, 16);
$app->heading("Hello, CUI", 2);
$app->secondary("native window driven from PHP");
$on = $app->toggle("Notifications", true);
$app->rowBegin(8);
$clicked = $app->button("Click me", \Tphp\Tui\BUTTON_PRIMARY);
$app->text("Clicked 0 times");
$app->boxEnd();
$app->label("label");
$app->separator();
$app->spacer();
$app->boxEnd();

check("toggle keeps value (idle)", true, $on);
check("button idle returns false", false, $clicked);
echo "drawn\n";

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
