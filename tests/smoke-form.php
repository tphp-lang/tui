<?php
/**
 * tphp/tui 冒烟：表单控件层（Task 3）
 * hidden 窗口一帧内调用全部表单方法；断言 idle 态返回值与 buffer 往返。
 * 运行：build\tphp.exe run tests\smoke-form.php
 */
$fail = 0;
function check(bool $cond, string $name): void {
    global $fail;
    if ($cond) { echo "PASS $name\n"; }
    else { echo "FAIL $name\n"; $fail++; }
}

$app = new \Tphp\Tui\App("tui-form", 680, 560, true);
$app->frame();
$app->heading("Form smoke");

// buffer 往返（值由 PHP 持有，idle 帧原样返回）
check($app->form()->input("f-name", "Alice", 512, ["label" => "Name", "placeholder" => "Enter"]) === "Alice", "input-roundtrip");
check($app->form()->textarea("f-bio", "hello", 4096) === "hello", "textarea-roundtrip");
check($app->form()->search("f-q", "query", 256) === "query", "search-roundtrip");
check($app->form()->otpInput("f-otp", "123456", 6) === "123456", "otp-roundtrip");
check($app->form()->tagInput("f-tags", "a,b", 512) === "a,b", "tag-roundtrip");
check($app->form()->moneyInput("f-money", 12.5) === 12.5, "money-roundtrip");

// 值型（idle 帧返回新值 = 传入值；CUI 内部 float32 存储，用 epsilon 比较）
check($app->form()->number("f-num", 5.0, 0, 10) === 5.0, "number-value");
check(abs($app->form()->slider("f-slider", 0.3, 0.0, 1.0) - 0.3) < 1e-6, "slider-value");
$rs = $app->form()->rangeSlider("f-range", 2.0, 8.0, 0.0, 10.0);
check(is_array($rs) && count($rs) === 2 && $rs[0] === 2.0 && $rs[1] === 8.0, "rangeSlider-pair");
check($app->form()->rulerSlider("f-ruler", 3.0, 0, 10) === 3.0, "rulerSlider-value");
$hs = $app->form()->histogramSlider("f-hist", [1, 2, 3, 4, 5], 0.0, 4.0, 0.0, 4.0);
check(is_array($hs) && count($hs) === 2, "histogramSlider-pair");
check($app->form()->knob("f-knob", 0.5, 0, 1) === 0.5, "knob-value");
check(abs($app->form()->fader("f-fader", 0.7, 0, 1) - 0.7) < 1e-6, "fader-value");
check($app->form()->scrub("f-scrub", 1.0, 0, 4) === 1.0, "scrub-value");
check($app->form()->levelMeter("f-lvl", [0.1, 0.5, 0.9]) === false, "levelMeter-bool");

// 勾选/选择
check($app->form()->checkbox("f-chk", true) === true, "checkbox-value");
check($app->form()->radio("f-radio", 0, 1) === 0, "radio-value");
check($app->form()->radioGroup("f-rg", ["A", "B", "C"], 1) === 1, "radioGroup-index");
check($app->form()->segmented("f-seg", ["Day", "Week", "Month"], 0) === 0, "segmented-index");
check($app->form()->chip("f-chip", false) === false, "chip-value");
check($app->form()->rating("f-rate", 3, 5) === 3, "rating-value");
$tg = $app->form()->toggleGroup("f-tg", ["Bold", "Italic"], [true, false]);
check(is_array($tg) && $tg[0] === true && $tg[1] === false, "toggleGroup-values");
check($app->form()->select("f-sel", ["One", "Two"], 0) === 0, "select-index");
$ms = $app->form()->multiSelect("f-ms", ["A", "B", "C"], [true, false, true]);
check(is_array($ms) && $ms[0] === true && $ms[1] === false && $ms[2] === true, "multiSelect-values");

// 日期时间（[y,m,d] / [h,m] 往返）
$d = $app->form()->dateToday();
check(is_array($d) && count($d) === 3 && $d[0] > 2000, "dateToday");
$cd = $app->form()->calendar("f-cal", [2026, 11, 15]);
check($cd === [2026, 11, 15], "calendar-roundtrip");
$dp = $app->form()->datePicker("f-dp", [2026, 1, 2]);
check($dp === [2026, 1, 2], "datePicker-roundtrip");
$dr = $app->form()->dateRangePicker("f-dr", [2026, 1, 1], [2026, 1, 7]);
check(is_array($dr) && count($dr) === 2 && $dr[0] === [2026, 1, 1], "dateRange-roundtrip");
$rc = $app->form()->rangeCalendar("f-rc", [2026, 1, 1], [2026, 1, 7], 1);
check(is_array($rc) && count($rc) === 2, "rangeCalendar");
$tp = $app->form()->timePicker("f-tp", 9, 30);
check($tp === [9, 30], "timePicker-roundtrip");
$td = $app->form()->timeDial("f-td", 9, 30, 64);
check($td === [9, 30], "timeDial-roundtrip");
check($app->form()->wheelPicker("f-wp", ["Mon", "Tue", "Wed"], 1) === 1, "wheelPicker-index");
check($app->form()->colorPicker("f-cp", 0x3B82F6FF) === 0x3B82F6FF, "colorPicker-roundtrip");

$app->quit();
check($app->frame() === false, "loop-stopped");
$app->destroy();
echo $fail === 0 ? "SMOKE=OK\n" : "SMOKE=FAIL\n";
exit($fail === 0 ? 0 : 1);
