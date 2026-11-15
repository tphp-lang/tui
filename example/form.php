<?php
/**
 * tphp/tui 示例：表单控件全集演示
 * 运行：build\tphp.exe run example/form.php
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
    $name = $app->input("f-name", $name, 256, ["label" => "姓名", "placeholder" => "请输入"]);
    $bio = $app->textarea("f-bio", $bio, 4096, ["label" => "简介"]);
    $tags = $app->tagInput("f-tags", $tags, 512);

    $app->label("数值");
    $age = $app->number("f-age", $age, 0, 120);
    $size = $app->slider("f-size", $size, 0.0, 1.0);
    $lohi = $app->rangeSlider("f-lohi", 20.0, 80.0, 0.0, 100.0);
    $app->muted("范围: " . $lohi[0] . " ~ " . $lohi[1]);

    $app->label("选择");
    $agree = $app->checkbox("我已阅读协议", $agree);
    $role = $app->radioGroup("f-role", ["开发", "设计", "产品"], $role);
    $rating = $app->rating("f-rate", $rating, 5);
    $theme = $app->segmented("f-theme", ["浅色", "深色"], $theme);

    $app->label("日期时间");
    $d = $app->datePicker("f-date", [2026, 11, 15]);
    $app->muted("选中: " . $d[0] . "-" . $d[1] . "-" . $d[2]);

    $app->gap(8);
    if ($app->button("提交")) {
        $app->toast("已提交: " . $name . " / age=" . (int)$age . " / agree=" . ($agree ? "Y" : "N"));
    }
}

$app->destroy();
