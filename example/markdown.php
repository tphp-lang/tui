<?php
/**
 * tphp/tui 示例：Markdown / 代码块 / JSON 展示
 * 运行：build\tphp.exe run example/markdown.php
 */
$md = "# TinyPHP tui\n\n"
    . "CUI 1.4.0 的 PHP 封装，立即模式：\n\n"
    . "- **button/toggle** 返回点击与新值\n"
    . "- `input` 等 buffer 控件值由 PHP 持有\n"
    . "- 图表、表格、Markdown 开箱即用\n\n"
    . "```php\n\$app = new \\Tphp\\Tui\\App(\"demo\", 640, 480);\nwhile (\$app->frame()) { ... }\n```\n\n"
    . "详见 [doc/coverage.md](coverage.md)。";

$json = '{"name": "tphp/tui", "version": "0.1.0", "ext": {"tphp_abi": "0.0.2"}, "ok": true}';

$code = "<?php\nfunction greet(string \$who): string {\n    return \"hello \" . \$who;\n}\n";

$app = new \Tphp\Tui\App("tui markdown", 720, 640);

while ($app->frame()) {
    $app->heading("Rich content");
    $app->secondary("markdown / codeBlock / jsonView / hexView");

    $app->markdown("md-main", $md);
    $app->gap(6);
    $app->codeBlock("cb-main", $code, "php", "greet.php");
    $app->gap(6);
    $app->jsonView("jv-main", $json);
    $app->gap(6);
    $app->hexView("hv-main", "\x89PNG\r\n\x1a\n\x00\x01\x02\x03hello");
    $app->gap(6);
    $app->diffView("dv-main",
        "line one\nline two\nline three\n",
        "line one\nline 2\nline three\nline four\n");
}

$app->destroy();
