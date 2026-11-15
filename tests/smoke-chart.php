<?php
/**
 * tphp/tui 冒烟：图表与数据可视化层（Task 5）
 * hidden 窗口一帧内调用全部图表（小型真实数据），断言返回值形状。
 * 运行：build\tphp.exe run tests\smoke-chart.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

$app = new \Tphp\Tui\App("tui-chart", 760, 600, true);
$app->frame();
$app->heading("Chart smoke");

$series = [
    ["name" => "A", "values" => [1.0, 2.5, 3.0, 4.5, 5.0, 6.5, 7.0, 8.0], "color" => 0x3B82F6FF],
    ["name" => "B", "values" => [2.0, 1.5, 4.0, 3.5, 6.0, 5.5, 8.0, 7.0]],
];
$app->lineChart("lc1", $series, ["area" => true, "height" => 180]);
$app->barChart("bc1", $series, ["stacked" => false]);
$app->donutChart("dc1", [30.0, 20.0, 50.0], ["Red", "Green", "Blue"], []);
$app->sparkline([1.0, 3.0, 2.0, 5.0, 4.0], 100, 24, 0x22C55EFF);
$app->gauge("Load", 0.72, 120);
$app->radialGauge("rg1", 42.0, 0, 100);
check(is_int($app->scatterChart("sc1", [
    ["name" => "pts", "points" => [[1.0, 2.0], [3.0, 4.0], [5.0, 1.5]], "sizes" => [2.0, 3.0, 4.0]],
])), "scatter-int");
check(is_int($app->radarChart("rd1", $series, ["Speed", "Cost", "Quality", "UX", "Docs", "API", "Perf", "Test"])), "radar-int");
check(is_int($app->heatmap("hm1", [0.1, 0.2, 0.3, 0.4, 0.5, 0.6], 2, 3)), "heatmap-int");
check(is_int($app->calendarHeatmap("ch1", [2026, 10, 1], [1.0, 0.5, 0.0, 0.8, 1.0, 0.2, 0.6, 0.9, 0.3, 0.7])), "calendarHeatmap-int");

$items = [
    ["label" => "Desktop", "value" => 42.0, "color" => 0x3B82F6FF],
    ["label" => "Mobile", "value" => 28.0],
    ["label" => "Bot", "value" => 14.0],
];
check(is_int($app->treemap("tm1", $items)), "treemap-int");
check(is_int($app->funnel("fn1", $items)), "funnel-int");
check(is_int($app->sankey("sn1", ["Web", "API", "DB", "Cache"], [
    ["from" => 0, "to" => 1, "value" => 10.0],
    ["from" => 1, "to" => 2, "value" => 6.0],
    ["from" => 1, "to" => 3, "value" => 4.0],
])), "sankey-int");
check(is_int($app->waffle("wf1", $items)), "waffle-int");
check(is_int($app->splitBar("sb1", $items)), "splitBar-int");
check(is_int($app->barList("bl1", $items)), "barList-int");
check(is_int($app->activityRings("ar1", $items)), "activityRings-int");

$candles = [
    ["open" => 100.0, "high" => 108.0, "low" => 97.0, "close" => 105.0, "volume" => 1200.0],
    ["open" => 105.0, "high" => 110.0, "low" => 102.0, "close" => 103.0, "volume" => 900.0],
    ["open" => 103.0, "high" => 107.0, "low" => 99.0, "close" => 106.0, "volume" => 1500.0],
];
check(is_int($app->candlestick("cs1", $candles, ["Mon", "Tue", "Wed"])), "candlestick-int");
$wf = $app->waterfall("wf2", [
    ["name" => "GET /api", "method" => "GET", "status" => 200, "type" => "fetch",
     "size" => 4096.0, "start" => 0.0, "timing" => [0.0, 0.01, 0.02, 0.03, 0.04, 0.05]],
    ["name" => "POST /api", "method" => "POST", "status" => 201, "type" => "fetch",
     "size" => 8192.0, "start" => 0.05, "timing" => [0.05, 0.06, 0.07, 0.08, 0.09, 0.1]],
]);
check(is_array($wf) && array_key_exists("hovered", $wf) && array_key_exists("clicked", $wf), "waterfall-result");
$dot = $app->dotChart("dt1", [
    ["values" => [1.0, 3.0, 2.0, 5.0, 4.0], "name" => "r1"],
    ["values" => [2.0, 2.0, 3.0, 4.0, 3.5], "name" => "r2", "accent" => true],
]);
check(is_array($dot) && array_key_exists("hovered", $dot) && array_key_exists("clicked", $dot), "dotChart-result");
$app->timelineItem("Deploy finished");

$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();
echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
