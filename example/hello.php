<?php
// tphp/tui 最小示例（对应 CUI examples/hello.c）
// 运行：build\tphp.exe run example/hello.php —— 点击计数，关窗退出

use Tphp\Tui\App;
use Tphp\Tui\ButtonVariant;
use Tphp\Tui\FontRole;


$app = new App("Hello", 420, 300);

// 中文字形：内嵌字体是拉丁子集，装一个含 CJK 的系统字体并设为回退
$cjk = $app->fontLoadSystem();          // -1 = 本机没找到
if ($cjk >= 0) {
    foreach ([FontRole::Body, FontRole::Medium, FontRole::Display, FontRole::Mono] as $role) {
        $app->fontAddFallback($role, $cjk);
    }
}

$notifications = true;
$clicks = 0;

while ($app->frame()) {
    $app->layout()->columnBegin(32, 16);
    $app->heading("你好，CUI 界面", 2);
    $app->secondary("中文与 English 混排都不再是豆腐块。");
    $notifications = $app->widgets()->toggle("启用通知", $notifications);
    $app->layout()->rowBegin(8);
    if ($app->widgets()->button("点我", ButtonVariant::Primary)) {
        $clicks++;
    }
    $app->text("已点击 {$clicks} 次");
    $app->layout()->boxEnd();               // 关 row（row/column 都用 boxEnd 闭合）
    $app->layout()->boxEnd();               // 关 column
}

echo "bye\n";

