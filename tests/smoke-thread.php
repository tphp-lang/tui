<?php
/**
 * 真 OS 线程多窗口专测（Tphp\Thread × per-App state × TLS）：
 * 每个 worker 线程是独立 runtime（类表克隆/独立堆），各自创建 CUI 窗口。
 * 验证：
 *   1. worker 线程内 tui 类可用（类表克隆含 tui、枚举查找按线程类表）
 *   2. 跨线程 per-App 状态隔离：线程 A 的 dialog 残留只污染 A 自己，
 *      线程 B 完全不受影响（B.9 TLS + B.10 per-App 的联合效果）
 *   3. terminal 缓冲 per-App 独立（跨线程）
 *   4. 主线程窗口与子线程窗口并存：wait() 回来后主窗口仍正常 frame
 *
 * 运行：..\TinyPHP.rt\build\tphp.exe run tests/smoke-thread.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

/* 两线程共用的窗口流程：leak=true 时故意留 dialog 残留 */
function window_flow(string $tag, bool $leak): array {
    $app = new \Tphp\Tui\App("tui-$tag", 200, 120, true);
    $app->frame();
    if ($leak) {
        $app->layout()->dialogBegin("leak-$tag", true);   // 故意漏 dialogEnd
    }
    $c = $app->content();
    $c->terminalInit(4096, 128);
    $c->terminalWrite("$tag-data");
    $buf = $c->terminal("t-$tag");
    /* 枚举跨线程：ButtonVariant 经当前线程类表解析（tui_opt_enum） */
    $app->widgets()->button("$tag-btn", \Tphp\Tui\ButtonVariant::Primary);
    $secondFrame = true;
    try {
        $app->simulateMouse(1, 1);
        $app->frame();                                    // 残留者在此被检出
    } catch (\Error $e) {
        $secondFrame = false;
    }
    $app->quit();
    $app->destroy();
    return ["tag" => $tag, "term" => is_array($buf), "frame2" => $secondFrame];
}

function thread_a(): array { return window_flow("A", true);  }
function thread_b(): array { return window_flow("B", false); }

// === 主线程窗口先存在 ===
$main = new \Tphp\Tui\App("tui-main", 200, 120, true);
$main->frame();

// === 两个 worker 线程并行，各自建窗 ===
$t = new \Tphp\Thread();
$t->spawn("thread_a");
$t->spawn("thread_b");
$res = $t->wait();          // 阻塞至两线程完成（期间主窗口暂停 frame）

check(is_array($res) && count($res) === 2, "wait-returns-both");
$rA = $res[0]; $rB = $res[1];

check($rA["tag"] === "A" && $rB["tag"] === "B", "worker-cls-and-window-ok");
check($rA["term"] && $rB["term"], "per-app-terminal-buffers-cross-thread");
check($rA["frame2"] === false, "A-still-detects-own-leak-cross-thread");
check($rB["frame2"] === true,  "B-unaffected-by-A-leak-cross-thread");

// === wait 回来后主窗口仍正常 ===
$mainOk = true;
try {
    $main->simulateMouse(1, 1);
    $main->frame();
    $main->layout()->dialogBegin("M", false);
    $main->layout()->dialogEnd();
    $main->simulateMouse(1, 1);
    $main->frame();
} catch (\Error $e) {
    $mainOk = false;
    echo "  main: ", $e->getMessage(), "\n";
}
check($mainOk, "main-window-still-works-after-threads");
$main->quit();
$main->destroy();

echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
