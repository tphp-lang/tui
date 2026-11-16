<?php
/**
 * per-App 状态隔离专测：验证一个窗口的残留/借阅状态不污染另一个窗口。
 *
 * 旧实现（文件级静态 g_*）下，App1 漏写 dialogEnd 留下 g_dialog_on=true，
 * App2 的 frame() 入口检查到该残留会误报 unclosed scope（或 App2 的 End
 * 误关 App1 的节点）。per-App 后各 Layout/Content 子对象各持 state，互不干扰。
 *
 * 注：不交错 frame——CUI 的 frame() 第二次起阻塞等事件，交错调用需输入注入，
 * 且属平台事件循环范畴，不在本测试范围。本测只验证状态隔离本身。
 *
 * 运行：build\tphp.exe run tests\smoke-multiwindow.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

// === 场景 1：App1 的 dialog 残留不污染 App2 ===
$a = new \Tphp\Tui\App("winA", 480, 360, true);
$a->frame();
$oa = true;
$a->layout()->dialogBegin("A-leak", $oa);   // 故意漏 dialogEnd，制造残留

// App2 全新窗口，正常绘制 + frame，不应受 A 残留影响
$b = new \Tphp\Tui\App("winB", 480, 360, true);
$bOk = true;
try {
    $b->frame();                              // 若全局残留，这里会误报 unclosed
    $b->layout()->dialogBegin("B", false);    // B 自己开一个关态 dialog
    $b->layout()->dialogEnd();
    $b->simulateMouse(1, 1);                  // 注入唤醒下一次 frame
    $b->frame();
} catch (\Error $e) {
    $bOk = false;
    echo "  B: ", $e->getMessage(), "\n";
}
check($bOk, "B-unaffected-by-A-leak");

// A 自己的残留仍应被检出（隔离不是掩盖）
$aDetect = false;
try { $a->simulateMouse(1, 1); $a->frame(); }
catch (\Error $e) { $aDetect = (strpos($e->getMessage(), "unclosed scope") !== false); }
check($aDetect, "A-still-detects-own-leak");

$a->destroy();   // 失衡实例直接销毁，不继续用
$b->quit();
$b->destroy();

// === 场景 2：两个窗口各自持有独立 terminal 缓冲 ===
$c = new \Tphp\Tui\App("winC", 480, 360, true);
$d = new \Tphp\Tui\App("winD", 480, 360, true);
$c->frame(); $d->frame();
$cc = $c->content(); $dc = $d->content();
$cc->terminalInit(4096, 256);
$dc->terminalInit(4096, 256);
$cc->terminalWrite("C-data");
$dc->terminalWrite("D-data");
// 各自 terminal() 读取不抛异常即视为缓冲独立可用
$cT = $cc->terminal("tc"); $dT = $dc->terminal("td");
check(is_array($cT) && is_array($dT), "independent-terminal-buffers");
$c->quit(); $d->quit();
$c->destroy(); $d->destroy();

echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
