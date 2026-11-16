<?php
/**
 * tphp/tui 阶段 B 探针：6 类 API + 域访问器
 * 验证 $app->layout()/form()/widgets()/charts()/content() 返回对应域对象，
 * 且其上的原生方法可用（impl 复用、子对象带 app 句柄），缓存身份稳定。
 * 旧 API（$app->form()->input() 等）由既有 7 冒烟覆盖，本探针不重复。
 * 运行：build\tphp.exe run tests\smoke-domain.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

// 6 个类 + LayoutType 枚举存在
check(class_exists(\Tphp\Tui\App::class), "class-App");
check(class_exists(\Tphp\Tui\Layout::class), "class-Layout");
check(class_exists(\Tphp\Tui\Form::class), "class-Form");
check(class_exists(\Tphp\Tui\Widgets::class), "class-Widgets");
check(class_exists(\Tphp\Tui\Charts::class), "class-Charts");
check(class_exists(\Tphp\Tui\Content::class), "class-Content");
check(class_exists(\Tphp\Tui\LayoutType::class), "class-LayoutType");
check(\Tphp\Tui\LayoutType::Column->value === 0, "layouttype-column");
check(\Tphp\Tui\LayoutType::Split->value === 4, "layouttype-split");
check(count(\Tphp\Tui\LayoutType::cases()) === 5, "layouttype-count");

$app = new \Tphp\Tui\App("domain", 640, 480, true);

// 访问器返回正确类型
$layout  = $app->layout(\Tphp\Tui\LayoutType::Column);
$form    = $app->form();
$widgets = $app->widgets();
$charts  = $app->charts();
$content = $app->content();
check($layout instanceof \Tphp\Tui\Layout, "layout-type");
check($form instanceof \Tphp\Tui\Form, "form-type");
check($widgets instanceof \Tphp\Tui\Widgets, "widgets-type");
check($charts instanceof \Tphp\Tui\Charts, "charts-type");
check($content instanceof \Tphp\Tui\Content, "content-type");

// 缓存身份稳定（同一 App 多次访问返回同一实例）
check($app->form() === $form, "form-cached");
check($app->layout(\Tphp\Tui\LayoutType::Row) === $layout, "layout-cached");

$app->frame();

// 域对象上的方法可用（impl 复用，子对象带 app 句柄）
$on = $form->checkbox("新API", true);
check($on === true, "form-checkbox");
$val = $form->input("q", "abc", 64, ["label" => "搜索"]);
check(is_string($val), "form-input");
$charts->lineChart("lc1", [
    ["name" => "v", "values" => [1.0, 2.0, 3.0], "color" => 0x3B82F6FF],
], ["height" => 120]);
check(true, "charts-line");
$md = $content->markdown("md1", "# hi");
check(is_array($md), "content-markdown");
$widgets->badge("new");
check(true, "widgets-badge");

// 旧 API 仍可用（App 保留全部方法）
$old = $app->form()->checkbox("旧API", true);
check($old === true, "old-api-checkbox");

$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();

echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
