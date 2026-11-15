/*
 * tphp/tui - src/tui_form.c - Task 3 表单控件域
 *
 * 两类语义：
 *   - buffer 往返型（input/textarea/search/otp/tag）：值由 PHP 每帧持有，
 *     C 侧只在调用期间借阅缓冲 → 拷进 cap 缓冲、调 CUI、取回字符串；
 *   - 值型（slider/checkbox/select/...）：C 侧原地改 + 返回 changed，
 *     PHP 拿「新值」继续持有状态（与 toggle 同款约定）。
 *
 * input 族支持可选关联数组 opts（label/placeholder/password/disabled/
 * clearable/icon/hint/error/prefix/suffix），其余控件 opts 走 NULL 默认。
 * 日期 PHP 形态 = [year, month, day] 三元组数组。
 */
#include "tui_util.h"

#include <string.h>

#define RAISE(vm, cls, ...) tphp_vm_raise(vm, cls, __VA_ARGS__)

#define WHO_INPUT   "Tphp\\Tui\\App::input()"
#define WHO_TEXTARA "Tphp\\Tui\\App::textarea()"
#define WHO_SEARCH  "Tphp\\Tui\\App::search()"

/* ---- 关联数组读取（本域局部） ---- */

static TVal *row_get(TArr *row, const char *k) {
    TVal key = TV_STR(tstr_new(k, strlen(k)));
    TVal *p = tarr_get(row, key);
    tval_release(&key);
    return p;
}

static const char *opt_row_str(VM *vm, TArr *row, const char *k,
                               const char *who, const char *fkey) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return NULL;
    if (v->t != T_STR)
        RAISE(vm, "TypeError", "%s(): opts[\"%s\"] must be a string", who, fkey);
    return tstr_val((TStr *)v->u.p);
}

static bool opt_row_bool(VM *vm, TArr *row, const char *k, bool def,
                         const char *who, const char *fkey) {
    TVal *v = row_get(row, k);
    if (!v) return def;
    if (v->t != T_TRUE && v->t != T_FALSE)
        RAISE(vm, "TypeError", "%s(): opts[\"%s\"] must be a bool", who, fkey);
    return v->t == T_TRUE;
}

/* input 族可选 opts 关联数组 → cui_input_opts（未给的键保持 NULL/默认） */
static void fill_input_opts(VM *vm, TVal *v, cui_input_opts *o, const char *who) {
    memset(o, 0, sizeof *o);
    if (!v) return;                       /* 未传 opts → NULL 语义 */
    if (v->t != T_ARR)
        RAISE(vm, "TypeError", "%s(): argument opts must be of type array, %s given",
              who, tphp_val_type_name(*v));
    TArr *r = (TArr *)v->u.p;
    o->label = opt_row_str(vm, r, "label", who, "label");
    o->placeholder = opt_row_str(vm, r, "placeholder", who, "placeholder");
    o->icon = opt_row_str(vm, r, "icon", who, "icon");
    o->hint = opt_row_str(vm, r, "hint", who, "hint");
    o->error = opt_row_str(vm, r, "error", who, "error");
    o->prefix = opt_row_str(vm, r, "prefix", who, "prefix");
    o->suffix = opt_row_str(vm, r, "suffix", who, "suffix");
    o->password = opt_row_bool(vm, r, "password", false, who, "password");
    o->disabled = opt_row_bool(vm, r, "disabled", false, who, "disabled");
    o->clearable = opt_row_bool(vm, r, "clearable", false, who, "clearable");
    o->read_only = opt_row_bool(vm, r, "read_only", false, who, "read_only");
}

/* ---- buffer 往返助手 ---- */

/* PHP string → 堆缓冲（cap 含结尾 NUL）→ CUI 编辑 → 取回新串 → 释放。
 * 返回值是新堆串（TV_STR 交给 VM），出错路径先释放再 raise。 */
static TVal buf_roundtrip(VM *vm, cui_ctx *ui, int (*fn)(cui_ctx *, const char *,
                          char *, size_t, const cui_input_opts *),
                          const char *id, const char *value, size_t cap,
                          const cui_input_opts *o, const char *who) {
    if (cap < 2)
        RAISE(vm, "ValueError", "%s(): cap must be >= 2", who);
    if (strlen(value) + 1 > cap)
        RAISE(vm, "ValueError", "%s(): value is longer than cap (%d)",
              who, (int)cap);
    char *buf = tm_malloc(cap);
    size_t len = strlen(value);
    memcpy(buf, value, len + 1);
    fn(ui, id, buf, cap, o);
    TStr *s = tstr_new(buf, strlen(buf));
    tm_free(buf);
    return TV_STR(s);
}

/* ---- buffer 往返型 ---- */

static TVal nm_input(VM *vm, void *self, TVal *args, int n) {
    const char *id = tui_arg_str(vm, args, n, 0, WHO_INPUT);
    const char *val = tui_arg_str(vm, args, n, 1, WHO_INPUT);
    int64_t cap = tui_opt_int(vm, args, n, 2, 512, WHO_INPUT, "cap");
    cui_input_opts o;
    fill_input_opts(vm, n >= 3 ? &args[3] : NULL, &o, WHO_INPUT);
    return buf_roundtrip(vm, tui_need_ui(vm, (TObj *)self, WHO_INPUT),
                         cui_input_ex, id, val, (size_t)cap,
                         n >= 3 ? &o : NULL, WHO_INPUT);
}

static TVal nm_textarea(VM *vm, void *self, TVal *args, int n) {
    const char *id = tui_arg_str(vm, args, n, 0, WHO_TEXTARA);
    const char *val = tui_arg_str(vm, args, n, 1, WHO_TEXTARA);
    int64_t cap = tui_opt_int(vm, args, n, 2, 4096, WHO_TEXTARA, "cap");
    cui_input_opts o;
    fill_input_opts(vm, n >= 3 ? &args[3] : NULL, &o, WHO_TEXTARA);
    return buf_roundtrip(vm, tui_need_ui(vm, (TObj *)self, WHO_TEXTARA),
                         cui_textarea_ex, id, val, (size_t)cap,
                         n >= 3 ? &o : NULL, WHO_TEXTARA);
}

static TVal nm_search(VM *vm, void *self, TVal *args, int n) {
    const char *id = tui_arg_str(vm, args, n, 0, WHO_SEARCH);
    const char *val = tui_arg_str(vm, args, n, 1, WHO_SEARCH);
    int64_t cap = tui_opt_int(vm, args, n, 2, 256, WHO_SEARCH, "cap");
    cui_input_opts o;
    fill_input_opts(vm, n >= 3 ? &args[3] : NULL, &o, WHO_SEARCH);
    return buf_roundtrip(vm, tui_need_ui(vm, (TObj *)self, WHO_SEARCH),
                         cui_search_ex, id, val, (size_t)cap,
                         n >= 3 ? &o : NULL, WHO_SEARCH);
}

static TVal nm_otp_input(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::otpInput()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *code = tui_arg_str(vm, args, n, 1, who);
    int64_t len = tui_opt_int(vm, args, n, 2, 6, who, "length");
    if (len < 1 || len > 32)
        RAISE(vm, "ValueError", "%s(): length must be in 1..32", who);
    if ((int64_t)strlen(code) > len)
        RAISE(vm, "ValueError", "%s(): code is longer than length", who);
    char *buf = tm_malloc((size_t)len + 1);
    memcpy(buf, code, strlen(code) + 1);
    cui_otp_input(tui_need_ui(vm, (TObj *)self, who), id, buf, (int)len);
    TStr *s = tstr_new(buf, strlen(buf));
    tm_free(buf);
    return TV_STR(s);
}

static TVal nm_tag_input(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::tagInput()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    const char *val = tui_arg_str(vm, args, n, 1, who);
    int64_t cap = tui_opt_int(vm, args, n, 2, 512, who, "cap");
    if (cap < 2)
        RAISE(vm, "ValueError", "%s(): cap must be >= 2", who);
    if ((int64_t)strlen(val) + 1 > cap)
        RAISE(vm, "ValueError", "%s(): value is longer than cap (%d)", who, (int)cap);
    char *buf = tm_malloc((size_t)cap);
    memcpy(buf, val, strlen(val) + 1);
    cui_tag_input_ex(tui_need_ui(vm, (TObj *)self, who), id, buf, (size_t)cap, NULL);
    TStr *s = tstr_new(buf, strlen(buf));
    tm_free(buf);
    return TV_STR(s);
}

/* ---- 值型：float 家族（返回新值） ---- */

static TVal nm_number(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::number()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    double v = tui_arg_float(vm, args, n, 1, who, "value");
    double lo = tui_arg_float(vm, args, n, 2, who, "min");
    double hi = tui_arg_float(vm, args, n, 3, who, "max");
    cui_number_ex(tui_need_ui(vm, (TObj *)self, who), id, &v, lo, hi, NULL);
    return TV_FLOAT(v);
}

static TVal nm_slider(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::slider()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    float v = (float)tui_arg_float(vm, args, n, 1, who, "value");
    float lo = (float)tui_arg_float(vm, args, n, 2, who, "min");
    float hi = (float)tui_arg_float(vm, args, n, 3, who, "max");
    cui_slider_ex(tui_need_ui(vm, (TObj *)self, who), label, &v, lo, hi, NULL);
    return TV_FLOAT(v);
}

static TVal nm_range_slider(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::rangeSlider()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    float lo = (float)tui_arg_float(vm, args, n, 1, who, "lo");
    float hi = (float)tui_arg_float(vm, args, n, 2, who, "hi");
    float min = (float)tui_arg_float(vm, args, n, 3, who, "min");
    float max = (float)tui_arg_float(vm, args, n, 4, who, "max");
    cui_range_slider(tui_need_ui(vm, (TObj *)self, who), label, &lo, &hi, min, max);
    TArr *a = tarr_new();
    tarr_append(a, TV_FLOAT(lo));
    tarr_append(a, TV_FLOAT(hi));
    return TV_ARR(a);
}

static TVal nm_ruler_slider(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::rulerSlider()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    float v = (float)tui_arg_float(vm, args, n, 1, who, "value");
    float lo = (float)tui_arg_float(vm, args, n, 2, who, "min");
    float hi = (float)tui_arg_float(vm, args, n, 3, who, "max");
    cui_ruler_slider_ex(tui_need_ui(vm, (TObj *)self, who), label, &v, lo, hi, NULL);
    return TV_FLOAT(v);
}

static TVal nm_histogram_slider(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::histogramSlider()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    int bc = 0;
    double *bins_d = tui_floats(vm, &args[1], who, "bins", &bc);
    float *bins = NULL;
    if (bc) {
        bins = tm_malloc(sizeof(float) * (size_t)bc);
        for (int i = 0; i < bc; i++) bins[i] = (float)bins_d[i];
    }
    tui_arr_free(bins_d);
    if (bc < 2) {
        tui_arr_free(bins);
        RAISE(vm, "ValueError", "%s(): bins needs at least 2 entries", who);
    }
    float lo = (float)tui_arg_float(vm, args, n, 2, who, "lo");
    float hi = (float)tui_arg_float(vm, args, n, 3, who, "hi");
    float min = (float)tui_arg_float(vm, args, n, 4, who, "min");
    float max = (float)tui_arg_float(vm, args, n, 5, who, "max");
    cui_histogram_slider_ex(tui_need_ui(vm, (TObj *)self, who), label,
                            bins, bc, &lo, &hi, min, max, NULL);
    tui_arr_free(bins);
    TArr *a = tarr_new();
    tarr_append(a, TV_FLOAT(lo));
    tarr_append(a, TV_FLOAT(hi));
    return TV_ARR(a);
}

static TVal nm_knob(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::knob()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    float v = (float)tui_arg_float(vm, args, n, 1, who, "value");
    float lo = (float)tui_arg_float(vm, args, n, 2, who, "min");
    float hi = (float)tui_arg_float(vm, args, n, 3, who, "max");
    cui_knob_ex(tui_need_ui(vm, (TObj *)self, who), id, &v, lo, hi, NULL);
    return TV_FLOAT(v);
}

static TVal nm_fader(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::fader()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    float v = (float)tui_arg_float(vm, args, n, 1, who, "value");
    float lo = (float)tui_arg_float(vm, args, n, 2, who, "min");
    float hi = (float)tui_arg_float(vm, args, n, 3, who, "max");
    cui_fader_ex(tui_need_ui(vm, (TObj *)self, who), id, &v, lo, hi, NULL);
    return TV_FLOAT(v);
}

static TVal nm_scrub(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::scrub()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    float v = (float)tui_arg_float(vm, args, n, 1, who, "value");
    float lo = (float)tui_arg_float(vm, args, n, 2, who, "min");
    float hi = (float)tui_arg_float(vm, args, n, 3, who, "max");
    cui_scrub_ex(tui_need_ui(vm, (TObj *)self, who), label, &v, lo, hi, NULL);
    return TV_FLOAT(v);
}

static TVal nm_level_meter(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::levelMeter()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int lc = 0;
    double *lv = tui_floats(vm, &args[1], who, "levels", &lc);
    float *levels = NULL;
    if (lc) {
        levels = tm_malloc(sizeof(float) * (size_t)lc);
        for (int i = 0; i < lc; i++) levels[i] = (float)lv[i];
    }
    tui_arr_free(lv);
    bool changed = cui_level_meter_ex(tui_need_ui(vm, (TObj *)self, who),
                                      id, levels, lc, NULL);
    tui_arr_free(levels);
    return changed ? TV_TRUE : TV_FALSE;
}

/* ---- 勾选/选择（返回新值/新索引） ---- */

static TVal nm_checkbox(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::checkbox()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    bool v = tui_arg_bool(vm, args, n, 1, who, "value");
    cui_checkbox_ex(tui_need_ui(vm, (TObj *)self, who), label, &v, NULL);
    return v ? TV_TRUE : TV_FALSE;
}

static TVal nm_radio(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::radio()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    int cur = (int)tui_arg_int(vm, args, n, 1, who, "current");
    int option = (int)tui_arg_int(vm, args, n, 2, who, "option");
    int v = cur;
    cui_radio_ex(tui_need_ui(vm, (TObj *)self, who), label, &v, option, NULL);
    return TV_INT(v);
}

static TVal nm_radio_group(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::radioGroup()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int ic = 0;
    char **items = tui_strs(vm, &args[1], who, "items", &ic);
    int idx = (int)tui_arg_int(vm, args, n, 2, who, "index");
    cui_radio_group(tui_need_ui(vm, (TObj *)self, who), id,
                    (const char *const *)items, ic, &idx);
    tui_strs_free(items, ic);
    return TV_INT(idx);
}

static TVal nm_segmented(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::segmented()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int ic = 0;
    char **items = tui_strs(vm, &args[1], who, "items", &ic);
    int idx = (int)tui_arg_int(vm, args, n, 2, who, "index");
    cui_segmented_ex(tui_need_ui(vm, (TObj *)self, who), id,
                     (const char *const *)items, ic, &idx, NULL);
    tui_strs_free(items, ic);
    return TV_INT(idx);
}

static TVal nm_chip(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::chip()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    bool v = tui_arg_bool(vm, args, n, 1, who, "selected");
    cui_chip_ex(tui_need_ui(vm, (TObj *)self, who), label, &v, NULL);
    return v ? TV_TRUE : TV_FALSE;
}

static TVal nm_rating(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::rating()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int v = (int)tui_arg_int(vm, args, n, 1, who, "value");
    int64_t max = tui_arg_int(vm, args, n, 2, who, "max");
    if (max < 1 || max > 20)
        RAISE(vm, "ValueError", "%s(): max must be in 1..20", who);
    cui_rating(tui_need_ui(vm, (TObj *)self, who), id, &v, (int)max);
    return TV_INT(v);
}

static TVal nm_toggle_group(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::toggleGroup()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int lc = 0;
    char **labels = tui_strs(vm, &args[1], who, "labels", &lc);
    if (n <= 2 || args[2].t != T_ARR) {
        tui_strs_free(labels, lc);
        RAISE(vm, "TypeError", "%s(): argument values must be of type array, %s given",
              who, n <= 2 ? "null" : tphp_val_type_name(args[2]));
    }
    int vc = 0;
    bool *values = tui_bools(vm, &args[2], who, "values", &vc);
    if (vc != lc) {
        tui_strs_free(labels, lc);
        tui_arr_free(values);
        RAISE(vm, "ValueError", "%s(): values length (%d) must match labels (%d)",
              who, vc, lc);
    }
    cui_toggle_group_ex(tui_need_ui(vm, (TObj *)self, who), id,
                        (const char *const *)labels, lc, values, NULL);
    tui_strs_free(labels, lc);
    TArr *a = tarr_new();
    for (int i = 0; i < vc; i++) tarr_append(a, values[i] ? TV_TRUE : TV_FALSE);
    tui_arr_free(values);
    return TV_ARR(a);
}

static TVal nm_select(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::select()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int ic = 0;
    char **items = tui_strs(vm, &args[1], who, "items", &ic);
    int idx = (int)tui_arg_int(vm, args, n, 2, who, "index");
    cui_select_ex(tui_need_ui(vm, (TObj *)self, who), id,
                  (const char *const *)items, ic, &idx, NULL);
    tui_strs_free(items, ic);
    return TV_INT(idx);
}

static TVal nm_multi_select(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::multiSelect()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int ic = 0;
    char **items = tui_strs(vm, &args[1], who, "items", &ic);
    if (n <= 2 || args[2].t != T_ARR) {
        tui_strs_free(items, ic);
        RAISE(vm, "TypeError", "%s(): argument selected must be of type array, %s given",
              who, n <= 2 ? "null" : tphp_val_type_name(args[2]));
    }
    int sc = 0;
    bool *sel = tui_bools(vm, &args[2], who, "selected", &sc);
    if (sc != ic) {
        tui_strs_free(items, ic);
        tui_arr_free(sel);
        RAISE(vm, "ValueError", "%s(): selected length (%d) must match items (%d)",
              who, sc, ic);
    }
    cui_multi_select_ex(tui_need_ui(vm, (TObj *)self, who), id,
                        (const char *const *)items, ic, sel, NULL);
    tui_strs_free(items, ic);
    TArr *a = tarr_new();
    for (int i = 0; i < sc; i++) tarr_append(a, sel[i] ? TV_TRUE : TV_FALSE);
    tui_arr_free(sel);
    return TV_ARR(a);
}

/* ---- 日期时间（PHP 形态 [y,m,d] / [hour,minute]） ---- */

/* [y,m,d] → cui_date（元素须为 int；形状不对抛 TypeError） */
static cui_date date_of(VM *vm, TVal *v, const char *who, const char *pname) {
    cui_date d = { 0, 0, 0 };
    if (v->t != T_ARR)
        RAISE(vm, "TypeError", "%s(): argument %s must be a [year,month,day] array, %s given",
              who, pname, tphp_val_type_name(*v));
    TArr *a = (TArr *)v->u.p;
    int64_t vals[3] = { 0, 0, 0 };
    int k = 0;
    for (uint32_t i = 0; i < a->dused && k < 3; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_INT)
            RAISE(vm, "TypeError", "%s(): %s[%d] must be an int", who, pname, k);
        vals[k++] = e.u.i;
    }
    if (k != 3)
        RAISE(vm, "ValueError", "%s(): %s must have exactly 3 elements [y,m,d]",
              who, pname);
    d.year = (int)vals[0];
    d.month = (int)vals[1];
    d.day = (int)vals[2];
    return d;
}

static TVal date_tv(cui_date d) {
    TArr *a = tarr_new();
    tarr_append(a, TV_INT(d.year));
    tarr_append(a, TV_INT(d.month));
    tarr_append(a, TV_INT(d.day));
    return TV_ARR(a);
}

static TVal nm_date_today(VM *vm, void *self, TVal *args, int n) {
    (void)vm;
    (void)self;
    (void)args;
    (void)n;
    return date_tv(cui_date_today());
}

static TVal nm_calendar(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::calendar()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_date d = date_of(vm, &args[1], who, "date");
    cui_calendar(tui_need_ui(vm, (TObj *)self, who), id, &d);
    return date_tv(d);
}

static TVal nm_date_picker(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::datePicker()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_date d = date_of(vm, &args[1], who, "date");
    cui_date_picker_ex(tui_need_ui(vm, (TObj *)self, who), id, &d, NULL);
    return date_tv(d);
}

/* start/end 各 [y,m,d]；返回 [start, end] 嵌套数组 */
static TVal nm_date_range_picker(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dateRangePicker()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_date_range r;
    r.start = date_of(vm, &args[1], who, "start");
    r.end = date_of(vm, &args[2], who, "end");
    cui_date_range_picker_ex(tui_need_ui(vm, (TObj *)self, who), id, &r, NULL);
    TArr *a = tarr_new();
    tarr_append(a, date_tv(r.start));
    tarr_append(a, date_tv(r.end));
    return TV_ARR(a);
}

static TVal nm_range_calendar(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::rangeCalendar()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    cui_date_range r;
    r.start = date_of(vm, &args[1], who, "start");
    r.end = date_of(vm, &args[2], who, "end");
    int64_t months = tui_opt_int(vm, args, n, 3, 1, who, "months");
    if (months < 1 || months > 6)
        RAISE(vm, "ValueError", "%s(): months must be in 1..6", who);
    cui_range_calendar(tui_need_ui(vm, (TObj *)self, who), id, &r, (int)months);
    TArr *a = tarr_new();
    tarr_append(a, date_tv(r.start));
    tarr_append(a, date_tv(r.end));
    return TV_ARR(a);
}

static TVal nm_time_picker(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::timePicker()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int hour = (int)tui_arg_int(vm, args, n, 1, who, "hour");
    int minute = (int)tui_arg_int(vm, args, n, 2, who, "minute");
    cui_time_picker_ex(tui_need_ui(vm, (TObj *)self, who), id, &hour, &minute, NULL);
    TArr *a = tarr_new();
    tarr_append(a, TV_INT(hour));
    tarr_append(a, TV_INT(minute));
    return TV_ARR(a);
}

static TVal nm_time_dial(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::timeDial()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int hour = (int)tui_arg_int(vm, args, n, 1, who, "hour");
    int minute = (int)tui_arg_int(vm, args, n, 2, who, "minute");
    double size = tui_opt_float(vm, args, n, 3, 64, who, "size");
    cui_time_dial(tui_need_ui(vm, (TObj *)self, who), id, &hour, &minute,
                  (float)size);
    TArr *a = tarr_new();
    tarr_append(a, TV_INT(hour));
    tarr_append(a, TV_INT(minute));
    return TV_ARR(a);
}

static TVal nm_wheel_picker(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::wheelPicker()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int ic = 0;
    char **items = tui_strs(vm, &args[1], who, "items", &ic);
    int idx = (int)tui_arg_int(vm, args, n, 2, who, "index");
    cui_wheel_picker_ex(tui_need_ui(vm, (TObj *)self, who), id,
                        (const char *const *)items, ic, &idx, NULL);
    tui_strs_free(items, ic);
    return TV_INT(idx);
}

/* color：0xRRGGBBAA 整数往返 */
static TVal nm_color_picker(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::colorPicker()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int64_t c = tui_arg_int(vm, args, n, 1, who, "color");
    uint32_t u = (uint32_t)c;
    cui_color col = {
        (uint8_t)((u >> 24) & 0xFF), (uint8_t)((u >> 16) & 0xFF),
        (uint8_t)((u >> 8) & 0xFF), (uint8_t)(u & 0xFF)
    };
    cui_color_picker(tui_need_ui(vm, (TObj *)self, who), id, &col);
    int64_t out = ((int64_t)col.r << 24) | ((int64_t)col.g << 16)
                | ((int64_t)col.b << 8) | (int64_t)col.a;
    return TV_INT(out);
}

/* moneyInput：金额往返（double） */
static TVal nm_money_input(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::moneyInput()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    double v = tui_arg_float(vm, args, n, 1, who, "value");
    cui_money_input_ex(tui_need_ui(vm, (TObj *)self, who), id, &v, NULL);
    return TV_FLOAT(v);
}

/* ---- 注册 ---- */

void tui_register_form(TClass *cls) {
    /* buffer 往返型 */
    tui_cls_add_native(cls, "input", (void *)nm_input, false);
    tui_cls_add_native(cls, "textarea", (void *)nm_textarea, false);
    tui_cls_add_native(cls, "search", (void *)nm_search, false);
    tui_cls_add_native(cls, "otpInput", (void *)nm_otp_input, false);
    tui_cls_add_native(cls, "tagInput", (void *)nm_tag_input, false);
    tui_cls_add_native(cls, "moneyInput", (void *)nm_money_input, false);
    /* 值型 float 家族 */
    tui_cls_add_native(cls, "number", (void *)nm_number, false);
    tui_cls_add_native(cls, "slider", (void *)nm_slider, false);
    tui_cls_add_native(cls, "rangeSlider", (void *)nm_range_slider, false);
    tui_cls_add_native(cls, "rulerSlider", (void *)nm_ruler_slider, false);
    tui_cls_add_native(cls, "histogramSlider", (void *)nm_histogram_slider, false);
    tui_cls_add_native(cls, "knob", (void *)nm_knob, false);
    tui_cls_add_native(cls, "fader", (void *)nm_fader, false);
    tui_cls_add_native(cls, "scrub", (void *)nm_scrub, false);
    tui_cls_add_native(cls, "levelMeter", (void *)nm_level_meter, false);
    /* 勾选/选择 */
    tui_cls_add_native(cls, "checkbox", (void *)nm_checkbox, false);
    tui_cls_add_native(cls, "radio", (void *)nm_radio, false);
    tui_cls_add_native(cls, "radioGroup", (void *)nm_radio_group, false);
    tui_cls_add_native(cls, "segmented", (void *)nm_segmented, false);
    tui_cls_add_native(cls, "chip", (void *)nm_chip, false);
    tui_cls_add_native(cls, "rating", (void *)nm_rating, false);
    tui_cls_add_native(cls, "toggleGroup", (void *)nm_toggle_group, false);
    tui_cls_add_native(cls, "select", (void *)nm_select, false);
    tui_cls_add_native(cls, "multiSelect", (void *)nm_multi_select, false);
    /* 日期时间 */
    tui_cls_add_native(cls, "dateToday", (void *)nm_date_today, false);
    tui_cls_add_native(cls, "calendar", (void *)nm_calendar, false);
    tui_cls_add_native(cls, "datePicker", (void *)nm_date_picker, false);
    tui_cls_add_native(cls, "dateRangePicker", (void *)nm_date_range_picker, false);
    tui_cls_add_native(cls, "rangeCalendar", (void *)nm_range_calendar, false);
    tui_cls_add_native(cls, "timePicker", (void *)nm_time_picker, false);
    tui_cls_add_native(cls, "timeDial", (void *)nm_time_dial, false);
    tui_cls_add_native(cls, "wheelPicker", (void *)nm_wheel_picker, false);
    tui_cls_add_native(cls, "colorPicker", (void *)nm_color_picker, false);
}
