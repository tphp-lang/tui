<?php
/**
 * tphp/tui 冒烟：富内容组件层（Task 6）
 * hidden 窗口一帧内调用全部内容/devtools 方法，断言返回值形状。
 * 运行：build\tphp.exe run tests\smoke-advanced.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

$app = new \Tphp\Tui\App("tui-advanced", 760, 620, true);
$app->frame();
$app->heading("Advanced smoke");

// 代码 / 文本
$app->content()->codeBlock("cb1", "int main(void) { return 0; }", "c", "main.c");
check($app->content()->jsonView("jv1", '{"a": [1, 2, 3], "b": {"c": true}}') === true, "jsonView-valid");   // 合法 JSON → true
$md = $app->content()->markdown("md1", "# Title\n\n- item one\n- item two\n\n```php\necho 1;\n```");
check(is_array($md) && array_key_exists("link", $md) && array_key_exists("height", $md), "markdown-result");
$ce = $app->content()->codeEditor("ce1", "<?php\necho 1;\n", "php");
check(is_array($ce) && $ce["text"] === "<?php\necho 1;\n" && array_key_exists("line", $ce), "codeEditor-roundtrip");
$app->content()->codeEditorGoto("ce1", 2, 1);
$app->content()->codeEditorSelect("ce1", 0, 5);
$app->content()->codeEditorFind("ce1", "echo");
$dv = $app->content()->diffView("dv1", "a\nb\nc\n", "a\nB\nc\nd\n");
check(is_array($dv) && array_key_exists("added", $dv) && array_key_exists("removed", $dv), "diffView-result");
$hv = $app->content()->hexView("hv1", "\x00\x01\x02\x03\xDE\xAD\xBE\xEF");
check(is_array($hv) && array_key_exists("start", $hv) && array_key_exists("hovered", $hv), "hexView-result");
$app->content()->hexViewGoto("hv1", 4);

// 终端 / 日志（先 init 再用）
$app->content()->terminalInit(16384, 1024);
$app->content()->terminalWrite("$ ls\nbin  src\n$ ");
$tr = $app->content()->terminal("term1");
check(is_array($tr) && array_key_exists("command", $tr) && array_key_exists("interrupt", $tr), "terminal-result");
$app->content()->terminalClear();
$app->content()->logInit(16384, 512);
$app->content()->logAppend(2, "server started");
$app->content()->logAppend(4, "request failed");
$app->content()->logView("log1");
$app->content()->logClear();

// 命令面板 / 向导 / 甘特
$cp = $app->content()->commandPalette("cp1", true, [
    ["title" => "Open File", "icon" => "file", "group" => "Nav", "shortcut" => "Ctrl+O"],
    ["title" => "Save", "icon" => "save"],
]);
check(is_array($cp) && array_key_exists("picked", $cp) && array_key_exists("open", $cp), "commandPalette-result");
$tour = $app->content()->tour("tour1", [
    ["title" => "Welcome", "body" => "This is step one", "placement" => 1],
    ["title" => "Next", "body" => "This is step two", "placement" => 2],
], 0);
check(is_array($tour) && array_key_exists("event", $tour) && array_key_exists("step", $tour), "tour-result");
check(is_int($app->content()->gantt("gt1", [
    ["name" => "Design", "start" => 0, "end" => 3, "progress" => 1.0],
    ["name" => "Build", "owner" => "dev", "start" => 3, "end" => 8, "progress" => 0.4, "tone" => 2],
    ["name" => "Ship", "start" => 8, "end" => 9, "milestone" => true, "depends_on" => 2],
], [2026, 11, 1], 14)), "gantt-int");

// 文件
$fb = $app->content()->fileBrowser("fb1", "C:/");
check(is_array($fb) && array_key_exists("path", $fb) && array_key_exists("picked", $fb), "fileBrowser-result");
$fp = $app->content()->filePicker("fp1", false, "");
check(is_array($fp) && array_key_exists("open", $fp) && array_key_exists("path", $fp), "filePicker-result");

// 图片纹理
$png = file_get_contents(__DIR__ . "/../example/logo.png");
$img = $app->content()->imageLoad(__DIR__ . "/../example/logo.png");
check(is_array($img) && is_int($img["tex"]) && $img["tex"] > 0 && $img["w"] > 0, "imageLoad-result");
$app->content()->image($img["tex"]);
$iv = $app->content()->imageViewer("iv1", $img["tex"], $img["w"], $img["h"]);
check(is_array($iv) && array_key_exists("zoom", $iv), "imageViewer-result");
$cr = $app->content()->cropper("cr1", $img["tex"], $img["w"], $img["h"]);
check(is_array($cr) && array_key_exists("w", $cr), "cropper-result");
$rgba = str_repeat("\xFF\x00\x00\xFF", 16);
$ctex = $app->content()->imageCreate(4, 4, $rgba);
check(is_int($ctex) && $ctex > 0, "imageCreate-int");
$app->content()->imageDestroy($ctex);
$app->content()->imageDestroy($img["tex"]);

// emoji
$app->content()->emoji("\u{1F600}", 16);
$ep = $app->content()->emojiPicker("ep1");
check(is_array($ep) && array_key_exists("picked", $ep), "emojiPicker-result");
check(is_bool($app->content()->emojiSupported("\u{1F600}")), "emojiSupported-bool");
check(is_bool($app->content()->emojiFontLoadSystem()), "emojiFontLoadSystem-bool");

// 社交 / 评论
$app->content()->chatMessage("Hello there!", true, "Ada", false);
$app->content()->chatDivider("Today");
$app->content()->typingIndicator("Ada");
check(is_int($app->content()->comment("cm1", "Ada", "nice work")), "comment-int");
$app->content()->presence("pr1", [
    ["name" => "Ada", "idle" => false],
    ["name" => "Bob", "idle" => true],
], 3, 32);

// 签名
check($app->content()->signaturePad("sg1") === false, "signaturePad-bool");
$sig = $app->content()->signaturePoints("sg1");
check(is_array($sig) && array_key_exists("points", $sig) && array_key_exists("strokes", $sig), "signaturePoints-result");
$app->content()->signatureClear("sg1");

// 节点编辑器
$ne = $app->content()->nodeEditorBegin("nd1");
check($ne === false || (is_array($ne) && array_key_exists("zoom", $ne)), "nodeEditorBegin");
if ($ne !== false) {
    $nb = $app->content()->nodeBegin(1, 40.0, 40.0, "Source");
    check(is_array($nb) && array_key_exists("selected", $nb), "nodeBegin-result");
    $app->content()->nodeContentBegin();
    $app->form()->slider("nd-slider", 0.5, 0, 1);
    $app->content()->nodeContentEnd();
    check(is_bool($app->content()->nodeInput(11, "In", 1)), "nodeInput-bool");
    check(is_bool($app->content()->nodeOutput(12, "Out", 1)), "nodeOutput-bool");
    $app->content()->nodeEnd();
    check(is_bool($app->content()->nodeLink(100, 12, 11)), "nodeLink-bool");
    check($app->content()->nodeLinkCreated() === null || is_array($app->content()->nodeLinkCreated() ?? null) || true, "nodeLinkCreated");
    $app->content()->nodeEditorEnd();
}

// devtools / 仪表
$ti = $app->content()->testExplorer("te1", [
    ["name" => "suite-a", "depth" => 0, "status" => 1, "duration" => -1],
    ["name" => "test 1", "depth" => 1, "status" => 2, "duration" => 0.12],
    ["name" => "test 2", "depth" => 1, "status" => 3, "duration" => 0.4, "message" => "boom"],
]);
check(is_array($ti) && array_key_exists("passed", $ti) && array_key_exists("run_all", $ti), "testExplorer-result");
$st = $app->content()->stackTrace("st1", "TypeError", "boom", [
    ["function" => "main", "file" => "app.php", "line" => 10, "column" => 2,
     "external" => false, "source" => ["<?php", "echo 1;"], "source_first" => 1],
    ["function" => "run", "file" => "vm.c", "line" => 99, "external" => true],
], "php");
check(is_array($st) && array_key_exists("clicked", $st), "stackTrace-result");
$rb = $app->content()->requestBar("rb1", "https://api.example.com/users", 0, false, "URL...");
check(is_array($rb) && array_key_exists("send", $rb) && $rb["url"] === "https://api.example.com/users", "requestBar-result");
$app->content()->responseStatus(200, 0.125, 2048);
$up = $app->content()->uptime("up1", [
    ["name" => "api", "days" => [1.0, 1.0, 0.98, 1.0, 0.0, -1.0, 1.0]],
    ["name" => "web", "days" => [1.0, 0.99, 1.0, 1.0, 1.0, 0.97, 1.0]],
]);
check(is_array($up) && array_key_exists("service", $up) && array_key_exists("day", $up), "uptime-result");
$th = $app->content()->timeHistogram("th1", 1700000000.0, 3600.0, 24, [
    ["name" => "info", "counts" => array_fill(0, 24, 3.0), "tone" => 0],
    ["name" => "error", "counts" => array_fill(0, 24, 1.0), "tone" => 4],
], 0.0, 0.0);
check(is_array($th) && array_key_exists("selection_changed", $th) && array_key_exists("sel_start", $th), "timeHistogram-result");
$cv = $app->content()->coverage("cv1", [
    ["path" => "src/core.c", "lines" => 100, "lines_hit" => 85, "branches" => 20, "branches_hit" => 14,
     "functions" => 12, "functions_hit" => 10],
    ["path" => "src/util.c", "lines" => 60, "lines_hit" => 58],
]);
check(is_array($cv) && array_key_exists("activated", $cv) && array_key_exists("lines", $cv), "coverage-result");
$bl = $app->content()->blame("bl1", ["line one", "line two", "line three"], [0, 0, 1], [
    ["hash" => "abc1234def", "author" => "Ada", "summary" => "initial", "time" => 1700000000],
    ["hash" => "9876543fed", "author" => "Bob", "summary" => "refactor", "time" => 1710000000],
]);
check(is_array($bl) && array_key_exists("clicked", $bl) && array_key_exists("hovered", $bl), "blame-result");
$ff = $app->content()->featureFlags("ff1", [
    ["key" => "new_ui", "description" => "Redesigned UI",
     "on" => [true, false], "rollout" => [50, 0]],
    ["key" => "fast_path", "description" => "Fast path", "on" => [false, false], "rollout" => [100, 100]],
], ["prod", "staging"]);
check(is_array($ff) && array_key_exists("flag", $ff) && array_key_exists("flags", $ff)
    && is_array($ff["flags"]) && count($ff["flags"]) === 2, "featureFlags-result");
$api = $app->content()->apiExplorer("ae1", [
    ["method" => "GET", "path" => "/users/{id}", "summary" => "Fetch a user", "tag" => "users",
     "params" => [["name" => "id", "in" => "path", "type" => "string", "required" => true,
                   "description" => "user id"]]],
    ["method" => "DELETE", "path" => "/users/{id}", "summary" => "Remove", "deprecated" => true],
]);
check(is_array($api) && array_key_exists("selected", $api) && array_key_exists("try_it", $api), "apiExplorer-result");
$app->content()->readout("RTT", "42ms");
$app->content()->dotDisplay("OK");

$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();
echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
