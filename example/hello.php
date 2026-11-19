<?php
// tphp/tui 最小示例（对应 CUI examples/hello.c）
// 运行：..\TinyPHP.rt\build\tphp.exe run example/hello.php —— 点击计数，关窗退出

use Tphp\Tui\App;
use Tphp\Tui\ButtonVariant;


$app = new App("Hello", 420, 300);

// 中文与 emoji 默认可显：缺字时引擎自动装入系统 CJK 字体，无需手动挂回退

$notifications = true;
$clicks = 0;

while ($app->frame()) {
    $app->layout()->columnBegin(32, 16);
    $app->heading("你好，CUI 界面", 2);
    $app->secondary("中文与 English 混排都不再是豆腐块，emoji 也直接可用 🎉。");
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

