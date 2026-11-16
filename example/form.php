<?php
/**
 * tphp/tui 示例：表单控件全集演示
 * 运行：..\TinyPHP.rt\build\tphp.exe run example/form.php
 */
$name = "";
$bio = "";
$age = 25.0;
$agree = false;
$role = 0;
$size = 0.5;
$tags = "php,tui";
$rating = 3;
$theme = 0;

$app = new \Tphp\Tui\App("tui form", 560, 720);

while ($app->frame()) {
    $app->heading("Form gallery");
    $app->secondary("值由 PHP 持有：每帧传入、取回新值");

    $app->label("文本");
    $name = $app->form()->input("f-name", $name, 256, ["label" => "姓名", "placeholder" => "请输入"]);
    $bio = $app->form()->textarea("f-bio", $bio, 4096, ["label" => "简介"]);
    $tags = $app->form()->tagInput("f-tags", $tags, 512);

    $app->label("数值");
    $age = $app->form()->number("f-age", $age, 0, 120);
    $size = $app->form()->slider("f-size", $size, 0.0, 1.0);
    $lohi = $app->form()->rangeSlider("f-lohi", 20.0, 80.0, 0.0, 100.0);
    $app->widgets()->muted("范围: " . $lohi[0] . " ~ " . $lohi[1]);

    $app->label("选择");
    $agree = $app->form()->checkbox("我已阅读协议", $agree);
    $role = $app->form()->radioGroup("f-role", ["开发", "设计", "产品"], $role);
    $rating = $app->form()->rating("f-rate", $rating, 5);
    $theme = $app->form()->segmented("f-theme", ["浅色", "深色"], $theme);

    $app->label("日期时间");
    $d = $app->form()->datePicker("f-date", [2026, 11, 15]);
    $app->widgets()->muted("选中: " . $d[0] . "-" . $d[1] . "-" . $d[2]);

    $app->widgets()->gap(8);
    if ($app->widgets()->button("提交")) {
        $app->layout()->toast("已提交: " . $name . " / age=" . (int)$age . " / agree=" . ($agree ? "Y" : "N"));
    }
}

$app->destroy();
