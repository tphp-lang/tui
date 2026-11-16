<?php
/**
 * tphp/tui 示例：待办清单（列表 + 输入 + 勾选计数）
 * 运行：build\tphp.exe run example/todo.php
 */
$items = ["读一章 TinyPHP 设计文档", "写 tui 扩展的测试"];
$states = [false, false];
$draft = "";

$app = new \Tphp\Tui\App("tui todo", 480, 440);
$app->setTheme(1);

while ($app->frame()) {
    $done = 0;
    for ($i = 0; $i < count($states); $i++) {
        if ($states[$i]) $done++;
    }
    $app->heading("TODO");
    $app->secondary(count($items) . " 项 · " . $done . " 完成");

    $draft = $app->form()->input("new-item", $draft, 256, ["label" => "新任务", "placeholder" => "输入后点添加"]);
    if ($app->widgets()->button("添加") && trim($draft) !== "") {
        $items[] = trim($draft);
        $states[] = false;
        $draft = "";
    }

    $app->separator();

    $keep_items = [];
    $keep_states = [];
    for ($i = 0; $i < count($items); $i++) {
        $states[$i] = $app->widgets()->toggle($items[$i], $states[$i]);
        if (!$app->widgets()->button("删 " . ($i + 1))) {
            $keep_items[] = $items[$i];
            $keep_states[] = $states[$i];
        }
    }
    $items = $keep_items;
    $states = $keep_states;

    if (count($items) === 0) {
        $app->widgets()->emptyState("没有任务，加一条吧");
    }

    $app->widgets()->gap(8);
    if ($app->widgets()->button("清空")) {
        $items = [];
        $states = [];
    }
}

$app->destroy();
