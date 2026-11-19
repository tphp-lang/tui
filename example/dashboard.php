<?php

/**
 * tphp/tui 示例：仪表盘（指标 + 图表 + 表格）
 * 运行：..\TinyPHP.rt\build\tphp.exe run example/dashboard.php
 */

use Tphp\Tui\App;

$week = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
$visits = [120.0, 180.0, 150.0, 210.0, 260.0, 190.0, 170.0];
$signups = [8.0, 12.0, 9.0, 15.0, 18.0, 11.0, 10.0];
$rows = [
    ["api-gw", "99.98%", "42ms"],
    ["auth", "99.95%", "18ms"],
    ["search", "99.90%", "120ms"],
    ["billing", "100.0%", "65ms"],
];

$app = new App("tui dashboard", 820, 640);
$app->setTheme(1);

while ($app->frame()) {
    $app->heading("Dashboard");
    $app->secondary("本周概览 · " . date("Y-m-d"));

    // 指标行
    $app->layout()->boxBegin(0, 8);
    $app->widgets()->metric("访问量", "1,280");
    $app->widgets()->metric("转化率", "4.7%");
    $app->widgets()->metric("在线", "132");
    $app->layout()->boxEnd();

    $app->separator();

    // 折线 + 柱状
    $app->charts()->lineChart("c-visit", [
        ["name" => "visits", "values" => $visits],
        ["name" => "signups", "values" => $signups],
    ], ["height" => 180]);
    $app->charts()->barChart("c-bar", [
        ["name" => "visits", "values" => $visits],
    ], ["height" => 140]);

    // 环形 + 仪表 + sparkline
    $app->charts()->donutChart("c-donut", [55.0, 30.0, 15.0], ["Web", "App", "API"], []);
    $app->charts()->gauge("CPU", 0.63, 110);
    $app->charts()->sparkline($visits, 140, 32, 0x22C55EFF);

    $app->separator();

    // 表格
    $cols = [
        ["title" => "服务", "sortable" => true],
        ["title" => "可用率", "align" => 2],
        ["title" => "延迟", "align" => 2],
    ];
    if ($app->layout()->tableBegin("t-svc", $cols)) {
        for ($i = 0; $i < count($rows); $i++) {
            $app->layout()->tableRow(false);
            $app->layout()->tableCell();
            $app->layout()->tableCellText($rows[$i][0]);
            $app->layout()->tableCell();
            $app->layout()->tableCellText($rows[$i][1]);
            $app->layout()->tableCell();
            $app->layout()->tableCellText($rows[$i][2]);
        }
        $app->layout()->tableEnd();
    }
}

$app->destroy();
