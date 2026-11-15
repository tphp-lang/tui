<?php
// tphp/tui 最小示例（对应 CUI examples/hello.c）
// 运行：build\tphp.exe run example/hello.php —— 点击计数，关窗退出

use Tphp\Tui\App;

$app = new App("Hello", 420, 300);
$notifications = true;
$clicks = 0;

while ($app->frame()) {
    $app->columnBegin(32, 16);
    $app->heading("Hello, CUI", 2);
    $app->secondary("A native window, a few components and no dependencies.");
    $notifications = $app->toggle("Notifications", $notifications);
    $app->rowBegin(8);
    if ($app->button("Click me", \Tphp\Tui\BUTTON_PRIMARY)) {
        $clicks++;
    }
    $app->text("Clicked {$clicks} times");
    $app->boxEnd();
    $app->boxEnd();
}

echo "bye\n";
