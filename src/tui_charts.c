/*
 * tphp/tui - src/tui_charts.c - Task 5 图表与数据可视化域
 *
 * 图表全是单次调用组件（非 scope）：结构体数组在调用内构造、用完即释放，
 * 无跨调用借阅。PHP 形态（记档于 doc/coverage.md）：
 *   series  = [['name'=>'a','values'=>[1.0,2.0],'color'=>0xRRGGBBAA?], ...]
 *   items   = [['label'=>'x','value'=>1.0,'color'=>?], ...]
 *   scatter = [['name'=>,'points'=>[[x,y],...],'sizes'=>[...]?], ...]
 *   candles = [['open'=>,'high'=>,'low'=>,'close'=>,'volume'=>0], ...]
 *   links   = [['from'=>0,'to'=>1,'value'=>3.5], ...]
 *   dots    = [['values'=>[...],'name'=>?,'accent'=>false], ...]
 *   requests= [['name'=>,'method'=>,'status'=>,'type'=>,'size'=>,'start'=>,
 *               'timing'=>[6 floats]], ...]
 * opts 关联数组（line/bar/donut 高频字段）：labels/area/no_legend/no_grid/
 * stacked/smooth/format/height/min/max。
 * 两趟执行：先全量校验再分配（raise 不泄漏）。
 */
#include "tui_util.h"

#include <string.h>

#define RAISE(vm, cls, ...) tphp_vm_raise(vm, cls, __VA_ARGS__)

/* ---- 行读取小工具 ---- */

static TVal *row_get(TArr *row, const char *k) {
    TVal key = TV_STR(tstr_new(k, strlen(k)));
    TVal *p = tarr_get(row, key);
    tval_release(&key);
    return p;
}

static TArr *rows_of(VM *vm, TVal *v, const char *who, const char *pname) {
    if (v->t != T_ARR)
        RAISE(vm, "TypeError", "%s(): argument %s must be of type array, %s given",
              who, pname, tphp_val_type_name(*v));
    return (TArr *)v->u.p;
}

/* 堆拷贝（strdup 在严格 -std=c11 下不可移植，自实现） */
static char *dup_s(const char *s) {
    size_t n = strlen(s);
    char *p = tm_malloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

static TArr *row_arr(VM *vm, TVal e, const char *who, const char *pname, int pos) {
    if (e.t != T_ARR)
        RAISE(vm, "TypeError",
              "%s(): array element %d of argument %s must be of type array, %s given",
              who, pos, pname, tphp_val_type_name(e));
    return (TArr *)e.u.p;
}

static const char *row_req_str(VM *vm, TArr *row, const char *k,
                               const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t != T_STR)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be a string",
              who, pname, pos - 1, k);
    return tstr_val((TStr *)v->u.p);
}

static const char *row_opt_str(VM *vm, TArr *row, const char *k, const char *who,
                               const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return NULL;
    if (v->t != T_STR)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be a string",
              who, pname, pos - 1, k);
    return tstr_val((TStr *)v->u.p);
}

static double row_req_num(VM *vm, TArr *row, const char *k,
                          const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || (v->t != T_INT && v->t != T_FLOAT))
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be numeric",
              who, pname, pos - 1, k);
    return tval_to_float(*v);
}

static double row_opt_num(VM *vm, TArr *row, const char *k, double def,
                          const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return def;
    if (v->t != T_INT && v->t != T_FLOAT)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be numeric",
              who, pname, pos - 1, k);
    return tval_to_float(*v);
}

static int64_t row_opt_int(VM *vm, TArr *row, const char *k, int64_t def,
                           const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return def;
    if (v->t != T_INT)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be an int",
              who, pname, pos - 1, k);
    return v->u.i;
}

static bool row_opt_bool(VM *vm, TArr *row, const char *k, bool def,
                         const char *who, const char *pname, int pos) {
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return def;
    if (v->t != T_TRUE && v->t != T_FALSE)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be a bool",
              who, pname, pos - 1, k);
    return v->t == T_TRUE;
}

/* 行内可选颜色 0xRRGGBBAA → cui_color（未给/0 → 全零 = 调色板默认） */
static cui_color row_opt_color(VM *vm, TArr *row, const char *k,
                               const char *who, const char *pname, int pos) {
    cui_color c = { 0, 0, 0, 0 };
    TVal *v = row_get(row, k);
    if (!v || v->t == T_NULL) return c;
    if (v->t != T_INT)
        RAISE(vm, "TypeError", "%s(): %s[%d][\"%s\"] must be an int color",
              who, pname, pos - 1, k);
    uint32_t u = (uint32_t)v->u.i;
    c.r = (uint8_t)((u >> 24) & 0xFF);
    c.g = (uint8_t)((u >> 16) & 0xFF);
    c.b = (uint8_t)((u >> 8) & 0xFF);
    c.a = (uint8_t)(u & 0xFF);
    if (c.a == 0 && (c.r | c.g | c.b)) c.a = 255;
    return c;
}

/* 统计行数组元素个数 */
static int count_rows(TArr *rows) {
    int cnt = 0;
    for (uint32_t i = 0; i < rows->dused; i++)
        if (rows->data[i].t != T_UNDEF) cnt++;
    return cnt;
}

/* 平坦数值数组 → float[]（含校验） */
static float *flat_floats(VM *vm, TVal *v, int *out_n,
                          const char *who, const char *pname) {
    int n = 0;
    double *d = tui_floats(vm, v, who, pname, &n);
    float *f = NULL;
    if (n) {
        f = tm_malloc(sizeof(float) * (size_t)n);
        for (int i = 0; i < n; i++) f[i] = (float)d[i];
    }
    tui_arr_free(d);
    *out_n = n;
    return f;
}

/* ---- chart opts（line/bar/donut 高频字段） ---- */

typedef struct {
    cui_chart_opts o;
    char **labels; int nlabels;
    char *format;
} chart_opts_hold;

/* opts 关联数组 → 借阅结构；用 chart_opts_free 回收（labels/format 为拷贝） */
static void chart_opts_build(VM *vm, TVal *v, chart_opts_hold *h,
                             const char *who) {
    memset(h, 0, sizeof *h);
    if (!v) return;
    if (v->t != T_ARR)
        RAISE(vm, "TypeError", "%s(): argument opts must be of type array, %s given",
              who, tphp_val_type_name(*v));
    TArr *r = (TArr *)v->u.p;
    TVal *lv = row_get(r, "labels");
    if (lv) {
        h->labels = tui_strs(vm, lv, who, "opts.labels", &h->nlabels);
    }
    TVal *fv = row_get(r, "format");
    if (fv) {
        if (fv->t != T_STR)
            RAISE(vm, "TypeError", "%s(): opts[\"format\"] must be a string", who);
        h->format = tm_malloc((size_t)((TStr *)fv->u.p)->len + 1);
        memcpy(h->format, ((TStr *)fv->u.p)->c, ((TStr *)fv->u.p)->len + 1);
    }
    TVal *av = row_get(r, "area");
    if (av && av->t != T_TRUE && av->t != T_FALSE)
        RAISE(vm, "TypeError", "%s(): opts[\"area\"] must be a bool", who);
    TVal *nv = row_get(r, "no_legend");
    if (nv && nv->t != T_TRUE && nv->t != T_FALSE)
        RAISE(vm, "TypeError", "%s(): opts[\"no_legend\"] must be a bool", who);
    TVal *ng = row_get(r, "no_grid");
    if (ng && ng->t != T_TRUE && ng->t != T_FALSE)
        RAISE(vm, "TypeError", "%s(): opts[\"no_grid\"] must be a bool", who);
    TVal *sv = row_get(r, "stacked");
    if (sv && sv->t != T_TRUE && sv->t != T_FALSE)
        RAISE(vm, "TypeError", "%s(): opts[\"stacked\"] must be a bool", who);
    TVal *sm = row_get(r, "smooth");
    if (sm && sm->t != T_TRUE && sm->t != T_FALSE)
        RAISE(vm, "TypeError", "%s(): opts[\"smooth\"] must be a bool", who);
    TVal *hv = row_get(r, "height");
    if (hv && hv->t != T_INT && hv->t != T_FLOAT)
        RAISE(vm, "TypeError", "%s(): opts[\"height\"] must be numeric", who);
    TVal *mn = row_get(r, "min");
    TVal *mx = row_get(r, "max");
    if ((mn && mn->t != T_INT && mn->t != T_FLOAT) ||
        (mx && mx->t != T_INT && mx->t != T_FLOAT))
        RAISE(vm, "TypeError", "%s(): opts min/max must be numeric", who);
    h->o.area = av && av->t == T_TRUE;
    h->o.no_legend = nv && nv->t == T_TRUE;
    h->o.no_grid = ng && ng->t == T_TRUE;
    h->o.stacked = sv && sv->t == T_TRUE;
    h->o.smooth = sm && sm->t == T_TRUE;
    h->o.height = hv ? (float)tval_to_float(*hv) : 0;   /* 0 → CUI 默认 220 */
    h->o.min = mn ? (float)tval_to_float(*mn) : 0;
    h->o.max = mx ? (float)tval_to_float(*mx) : 0;
    h->o.labels = (const char *const *)h->labels;
    h->o.label_count = h->nlabels;
    h->o.format = h->format;
}

static void chart_opts_free(chart_opts_hold *h) {
    tui_strs_free(h->labels, h->nlabels);
    tm_free(h->format);
    memset(h, 0, sizeof *h);
}

/* opts 是否全空（空则传 NULL，走 CUI 默认） */
static const cui_chart_opts *chart_opts_ptr(chart_opts_hold *h) {
    return (h->o.label_count || h->o.format || h->o.area || h->o.no_legend ||
            h->o.no_grid || h->o.stacked || h->o.smooth || h->o.height ||
            h->o.min || h->o.max) ? &h->o : NULL;
}

/* ---- series / items / scatter / candles / links / dots 转换 ---- */

/* series=[['name'=>,'values'=>[...],'color'=>?]] */
static void series_fill(VM *vm, TArr *rows, const char *pname, const char *who,
                        cui_series **out, char ***out_names,
                        float ***out_vals, int *out_n, int **out_counts) {
    int cnt = count_rows(rows);
    cui_series *s = cnt ? tm_malloc(sizeof(cui_series) * (size_t)cnt) : NULL;
    char **names = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt) : NULL;
    float **vals = cnt ? tm_malloc(sizeof(float *) * (size_t)cnt) : NULL;
    int *counts = cnt ? tm_malloc(sizeof(int) * (size_t)cnt) : NULL;
    /* 第一趟：校验 */
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, pname, pos + 1);
        row_opt_str(vm, row, "name", who, pname, pos + 1);
        TVal *vv = row_get(row, "values");
        if (!vv)
            RAISE(vm, "TypeError", "%s(): %s[%d][\"values\"] is required",
                  who, pname, pos);
        int n = 0;
        double *d = tui_floats(vm, vv, who, pname, &n);   /* 元素类型在此校验 */
        tui_arr_free(d);
        row_opt_color(vm, row, "color", who, pname, pos + 1);
        pos++;
    }
    /* 第二趟：填充（无 raise 点） */
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *nm = row_opt_str(vm, row, "name", who, pname, k + 1);
        names[k] = nm ? dup_s(nm) : dup_s("");
        int n = 0;
        vals[k] = flat_floats(vm, row_get(row, "values"), &n, who, pname);
        counts[k] = n;
        s[k].name = names[k];
        s[k].values = vals[k];
        s[k].count = n;
        s[k].color = row_opt_color(vm, row, "color", who, pname, k + 1);
        k++;
    }
    *out = s;
    *out_names = names;
    *out_vals = vals;
    *out_n = cnt;
    *out_counts = counts;
}

static void series_free(cui_series *s, char **names, float **vals,
                        int *counts, int n) {
    for (int i = 0; i < n; i++) {
        tm_free(names[i]);
        tm_free(vals[i]);
    }
    tm_free(names);
    tm_free(vals);
    tm_free(counts);
    tm_free(s);
}

/* items=[['label'=>,'value'=>,'color'=>?]] */
static void items_fill(VM *vm, TArr *rows, const char *pname, const char *who,
                       cui_viz_item **out, char ***out_labels, int *out_n) {
    int cnt = count_rows(rows);
    cui_viz_item *items = cnt ? tm_malloc(sizeof(cui_viz_item) * (size_t)cnt) : NULL;
    char **labels = cnt ? tm_malloc(sizeof(char *) * (size_t)cnt) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, pname, pos + 1);
        row_req_str(vm, row, "label", who, pname, pos + 1);
        row_req_num(vm, row, "value", who, pname, pos + 1);
        row_opt_color(vm, row, "color", who, pname, pos + 1);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        labels[k] = dup_s(row_req_str(vm, row, "label", who, pname, k + 1));
        items[k].label = labels[k];
        items[k].value = (float)row_req_num(vm, row, "value", who, pname, k + 1);
        items[k].color = row_opt_color(vm, row, "color", who, pname, k + 1);
        k++;
    }
    *out = items;
    *out_labels = labels;
    *out_n = cnt;
}

static void items_free(cui_viz_item *items, char **labels, int n) {
    for (int i = 0; i < n; i++) tm_free(labels[i]);
    tm_free(labels);
    tm_free(items);
}

/* ---- 各图表方法 ---- */

static TVal nm_line_chart(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::lineChart()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "series");
    cui_series *s = NULL; char **names = NULL; float **vals = NULL;
    int *counts = NULL, sc = 0;
    chart_opts_hold h;
    chart_opts_build(vm, n >= 3 ? &args[2] : NULL, &h, who);
    series_fill(vm, rows, "series", who, &s, &names, &vals, &sc, &counts);
    cui_line_chart_ex(tui_need_ui(vm, (TObj *)self, who), id, s, sc,
                      chart_opts_ptr(&h));
    series_free(s, names, vals, counts, sc);
    chart_opts_free(&h);
    return TV_NULL;
}

static TVal nm_bar_chart(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::barChart()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "series");
    cui_series *s = NULL; char **names = NULL; float **vals = NULL;
    int *counts = NULL, sc = 0;
    chart_opts_hold h;
    chart_opts_build(vm, n >= 3 ? &args[2] : NULL, &h, who);
    series_fill(vm, rows, "series", who, &s, &names, &vals, &sc, &counts);
    cui_bar_chart_ex(tui_need_ui(vm, (TObj *)self, who), id, s, sc,
                     chart_opts_ptr(&h));
    series_free(s, names, vals, counts, sc);
    chart_opts_free(&h);
    return TV_NULL;
}

static TVal nm_donut_chart(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::donutChart()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int vc = 0;
    float *values = flat_floats(vm, &args[1], &vc, who, "values");
    int lc = 0;
    char **labels = tui_strs(vm, &args[2], who, "labels", &lc);
    if (lc != vc) {
        tui_arr_free(values);
        tui_strs_free(labels, lc);
        RAISE(vm, "ValueError", "%s(): labels length (%d) must match values (%d)",
              who, lc, vc);
    }
    chart_opts_hold h;
    chart_opts_build(vm, n >= 4 ? &args[3] : NULL, &h, who);
    cui_donut_chart_ex(tui_need_ui(vm, (TObj *)self, who), id,
                       values, (const char *const *)labels, vc,
                       chart_opts_ptr(&h));
    tui_arr_free(values);
    tui_strs_free(labels, lc);
    chart_opts_free(&h);
    return TV_NULL;
}

static TVal nm_sparkline(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::sparkline()";
    int vc = 0;
    float *values = flat_floats(vm, &args[0], &vc, who, "values");
    double width = tui_opt_float(vm, args, n, 1, 100, who, "width");
    double height = tui_opt_float(vm, args, n, 2, 24, who, "height");
    int64_t color = tui_opt_int(vm, args, n, 3, 0, who, "color");
    cui_color c = { 0, 0, 0, 0 };
    if (color) {
        uint32_t u = (uint32_t)color;
        c.r = (uint8_t)((u >> 24) & 0xFF);
        c.g = (uint8_t)((u >> 16) & 0xFF);
        c.b = (uint8_t)((u >> 8) & 0xFF);
        c.a = (uint8_t)(u & 0xFF);
        if (c.a == 0) c.a = 255;
    }
    cui_size w = { CUI_SIZE_FIXED, (float)width };
    cui_sparkline(tui_need_ui(vm, (TObj *)self, who), values, vc, w,
                  (float)height, c);
    tui_arr_free(values);
    return TV_NULL;
}

static TVal nm_gauge(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::gauge()";
    const char *label = tui_arg_str(vm, args, n, 0, who);
    double v = tui_arg_float(vm, args, n, 1, who, "value");
    double size = tui_opt_float(vm, args, n, 2, 120, who, "size");
    cui_gauge(tui_need_ui(vm, (TObj *)self, who), label, (float)v, (float)size);
    return TV_NULL;
}

static TVal nm_radial_gauge(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::radialGauge()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    double v = tui_arg_float(vm, args, n, 1, who, "value");
    double lo = tui_arg_float(vm, args, n, 2, who, "min");
    double hi = tui_arg_float(vm, args, n, 3, who, "max");
    cui_radial_gauge_ex(tui_need_ui(vm, (TObj *)self, who), id,
                        (float)v, (float)lo, (float)hi, NULL);
    return TV_NULL;
}

/* scatter：points=[[x,y],...] 嵌套对；sizes 可选同长 */
static TVal nm_scatter_chart(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::scatterChart()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "series");
    int sc = count_rows(rows);
    cui_scatter_series *ss = sc ? tm_malloc(sizeof(cui_scatter_series) * (size_t)sc) : NULL;
    char **names = sc ? tm_malloc(sizeof(char *) * (size_t)sc) : NULL;
    cui_vec2 **pts = sc ? tm_malloc(sizeof(cui_vec2 *) * (size_t)sc) : NULL;
    float **sizes = sc ? tm_malloc(sizeof(float *) * (size_t)sc) : NULL;
    int *cnts = sc ? tm_malloc(sizeof(int) * (size_t)sc) : NULL;
    /* 第一趟：校验 */
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "series", pos + 1);
        row_opt_str(vm, row, "name", who, "series", pos + 1);
        TVal *pv = row_get(row, "points");
        if (!pv || pv->t != T_ARR)
            RAISE(vm, "TypeError", "%s(): series[%d][\"points\"] must be an array "
                  "of [x,y] pairs", who, pos);
        TArr *pa = (TArr *)pv->u.p;
        for (uint32_t j = 0; j < pa->dused; j++) {
            TVal pe = pa->data[j];
            if (pe.t == T_UNDEF) continue;
            if (pe.t != T_ARR)
                RAISE(vm, "TypeError", "%s(): series[%d][\"points\"][%u] must be "
                      "an [x,y] pair", who, pos, j);
            TArr *pair = (TArr *)pe.u.p;
            int pc = 0;
            for (uint32_t z = 0; z < pair->dused; z++) {
                TVal v = pair->data[z];
                if (v.t == T_UNDEF) continue;
                if (v.t != T_INT && v.t != T_FLOAT)
                    RAISE(vm, "TypeError",
                          "%s(): series[%d][\"points\"][%u] elements must be numeric",
                          who, pos, j);
                pc++;
            }
            if (pc != 2)
                RAISE(vm, "ValueError",
                      "%s(): series[%d][\"points\"][%u] must have exactly 2 numbers",
                      who, pos, j);
        }
        TVal *sv = row_get(row, "sizes");
        if (sv) {
            int n2 = 0;
            double *d = tui_floats(vm, sv, who, "sizes", &n2);
            tui_arr_free(d);
            (void)n2;   /* 长度在第二趟与 points 对齐校验 */
        }
        row_opt_color(vm, row, "color", who, "series", pos + 1);
        pos++;
    }
    /* 第二趟：填充 */
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *nm = row_opt_str(vm, row, "name", who, "series", k + 1);
        names[k] = dup_s(nm ? nm : "");
        TArr *pa = (TArr *)row_get(row, "points")->u.p;
        int pc = 0;
        for (uint32_t j = 0; j < pa->dused; j++)
            if (pa->data[j].t != T_UNDEF) pc++;
        pts[k] = tm_malloc(sizeof(cui_vec2) * (size_t)(pc ? pc : 1));
        int q = 0;
        for (uint32_t j = 0; j < pa->dused; j++) {
            TVal pe = pa->data[j];
            if (pe.t == T_UNDEF) continue;
            TArr *pair = (TArr *)pe.u.p;
            double xv = 0, yv = 0;
            int zc = 0;
            for (uint32_t z = 0; z < pair->dused; z++) {
                TVal v = pair->data[z];
                if (v.t == T_UNDEF) continue;
                double d = tval_to_float(v);
                if (zc == 0) xv = d; else yv = d;
                zc++;
            }
            pts[k][q].x = (float)xv;
            pts[k][q].y = (float)yv;
            q++;
        }
        sizes[k] = NULL;
        TVal *sv = row_get(row, "sizes");
        if (sv) {
            int n2 = 0;
            sizes[k] = flat_floats(vm, sv, &n2, who, "sizes");
            if (n2 != pc) {
                /* 交付前回收已建资源（此 raise 只可能在 PHP 数组被并发改动时） */
                for (int z = 0; z <= k; z++) {
                    tm_free(names[z]); tm_free(pts[z]); tm_free(sizes[z]);
                }
                tm_free(names); tm_free(pts); tm_free(sizes); tm_free(cnts);
                tm_free(ss);
                RAISE(vm, "ValueError", "%s(): series[%d] sizes length must match points",
                      who, k);
            }
        }
        cnts[k] = pc;
        ss[k].name = names[k];
        ss[k].points = pts[k];
        ss[k].sizes = sizes[k];
        ss[k].count = pc;
        ss[k].color = row_opt_color(vm, row, "color", who, "series", k + 1);
        k++;
    }
    int hovered = cui_scatter_chart_ex(tui_need_ui(vm, (TObj *)self, who), id,
                                       ss, sc, NULL);
    for (int i = 0; i < sc; i++) {
        tm_free(names[i]); tm_free(pts[i]); tm_free(sizes[i]);
    }
    tm_free(names); tm_free(pts); tm_free(sizes); tm_free(cnts); tm_free(ss);
    return TV_INT(hovered);
}

static TVal nm_radar_chart(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::radarChart()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "series");
    int ac = 0;
    char **axes = tui_strs(vm, &args[2], who, "axes", &ac);
    cui_series *s = NULL; char **names = NULL; float **vals = NULL;
    int *counts = NULL, sc = 0;
    series_fill(vm, rows, "series", who, &s, &names, &vals, &sc, &counts);
    int r = cui_radar_chart_ex(tui_need_ui(vm, (TObj *)self, who), id, s, sc,
                               (const char *const *)axes, ac, NULL);
    series_free(s, names, vals, counts, sc);
    tui_strs_free(axes, ac);
    return TV_INT(r);
}

static TVal nm_heatmap(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::heatmap()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int vc = 0;
    float *values = flat_floats(vm, &args[1], &vc, who, "values");
    int64_t rows = tui_arg_int(vm, args, n, 2, who, "rows");
    int64_t cols = tui_arg_int(vm, args, n, 3, who, "cols");
    if (rows < 1 || cols < 1 || rows * cols != vc) {
        tui_arr_free(values);
        RAISE(vm, "ValueError", "%s(): rows*cols (%lld) must equal values count (%d)",
              who, (long long)(rows * cols), vc);
    }
    int r = cui_heatmap_ex(tui_need_ui(vm, (TObj *)self, who), id, values,
                           (int)rows, (int)cols, NULL);
    tui_arr_free(values);
    return TV_INT(r);
}

static TVal nm_calendar_heatmap(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::calendarHeatmap()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    /* start = [y,m,d] */
    if (args[1].t != T_ARR)
        RAISE(vm, "TypeError", "%s(): argument start must be a [year,month,day] array",
              who);
    TArr *sa = (TArr *)args[1].u.p;
    int64_t sv[3] = { 0, 0, 0 };
    int sk = 0;
    for (uint32_t i = 0; i < sa->dused && sk < 3; i++) {
        TVal e = sa->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_INT)
            RAISE(vm, "TypeError", "%s(): start[%d] must be an int", who, sk);
        sv[sk++] = e.u.i;
    }
    if (sk != 3)
        RAISE(vm, "ValueError", "%s(): start must have exactly 3 elements [y,m,d]", who);
    cui_date start = { (int)sv[0], (int)sv[1], (int)sv[2] };
    int vc = 0;
    float *values = flat_floats(vm, &args[2], &vc, who, "values");
    int r = cui_calendar_heatmap_ex(tui_need_ui(vm, (TObj *)self, who), id,
                                    start, values, vc, NULL);
    tui_arr_free(values);
    return TV_INT(r);
}

static TVal nm_treemap(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::treemap()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "items");
    cui_viz_item *items = NULL; char **labels = NULL; int cnt = 0;
    items_fill(vm, rows, "items", who, &items, &labels, &cnt);
    int r = cui_treemap_ex(tui_need_ui(vm, (TObj *)self, who), id, items, cnt, NULL);
    items_free(items, labels, cnt);
    return TV_INT(r);
}

static TVal nm_funnel(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::funnel()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "items");
    cui_viz_item *items = NULL; char **labels = NULL; int cnt = 0;
    items_fill(vm, rows, "items", who, &items, &labels, &cnt);
    int r = cui_funnel_ex(tui_need_ui(vm, (TObj *)self, who), id, items, cnt, NULL);
    items_free(items, labels, cnt);
    return TV_INT(r);
}

static TVal nm_sankey(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::sankey()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    int nc = 0;
    char **nodes = tui_strs(vm, &args[1], who, "nodes", &nc);
    TArr *rows = rows_of(vm, &args[2], who, "links");
    int lc = count_rows(rows);
    cui_sankey_link *links = lc ? tm_malloc(sizeof(cui_sankey_link) * (size_t)lc) : NULL;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "links", pos + 1);
        int64_t f = row_opt_int(vm, row, "from", -1, who, "links", pos + 1);
        int64_t t = row_opt_int(vm, row, "to", -1, who, "links", pos + 1);
        row_req_num(vm, row, "value", who, "links", pos + 1);
        if (f < 0 || f >= nc || t < 0 || t >= nc) {
            tui_strs_free(nodes, nc);
            tm_free(links);
            RAISE(vm, "ValueError", "%s(): links[%d] from/to out of nodes range",
                  who, pos);
        }
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        links[k].from = (int)row_opt_int(vm, row, "from", -1, who, "links", k + 1);
        links[k].to = (int)row_opt_int(vm, row, "to", -1, who, "links", k + 1);
        links[k].value = (float)row_req_num(vm, row, "value", who, "links", k + 1);
        k++;
    }
    int r = cui_sankey_ex(tui_need_ui(vm, (TObj *)self, who), id,
                          (const char *const *)nodes, nc, links, lc, NULL);
    tui_strs_free(nodes, nc);
    tm_free(links);
    return TV_INT(r);
}

static TVal nm_waffle(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::waffle()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "items");
    cui_viz_item *items = NULL; char **labels = NULL; int cnt = 0;
    items_fill(vm, rows, "items", who, &items, &labels, &cnt);
    int r = cui_waffle_ex(tui_need_ui(vm, (TObj *)self, who), id, items, cnt, NULL);
    items_free(items, labels, cnt);
    return TV_INT(r);
}

static TVal nm_split_bar(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::splitBar()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "items");
    cui_viz_item *items = NULL; char **labels = NULL; int cnt = 0;
    items_fill(vm, rows, "items", who, &items, &labels, &cnt);
    int r = cui_split_bar_ex(tui_need_ui(vm, (TObj *)self, who), id, items, cnt, NULL);
    items_free(items, labels, cnt);
    return TV_INT(r);
}

static TVal nm_bar_list(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::barList()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "items");
    cui_viz_item *items = NULL; char **labels = NULL; int cnt = 0;
    items_fill(vm, rows, "items", who, &items, &labels, &cnt);
    int r = cui_bar_list_ex(tui_need_ui(vm, (TObj *)self, who), id, items, cnt, NULL);
    items_free(items, labels, cnt);
    return TV_INT(r);
}

static TVal nm_activity_rings(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::activityRings()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "items");
    cui_viz_item *items = NULL; char **labels = NULL; int cnt = 0;
    items_fill(vm, rows, "items", who, &items, &labels, &cnt);
    int r = cui_activity_rings_ex(tui_need_ui(vm, (TObj *)self, who),
                                  id, items, cnt, NULL);
    items_free(items, labels, cnt);
    return TV_INT(r);
}

static TVal nm_candlestick(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::candlestick()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "candles");
    int cnt = count_rows(rows);
    cui_candle *cs = cnt ? tm_malloc(sizeof(cui_candle) * (size_t)cnt) : NULL;
    int lc = 0;
    char **labels = NULL;
    if (n >= 3 && args[2].t != T_NULL)
        labels = tui_strs(vm, &args[2], who, "labels", &lc);
    if (lc && lc != cnt) {
        tui_strs_free(labels, lc);
        tm_free(cs);
        RAISE(vm, "ValueError", "%s(): labels length (%d) must match candles (%d)",
              who, lc, cnt);
    }
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "candles", pos + 1);
        row_req_num(vm, row, "open", who, "candles", pos + 1);
        row_req_num(vm, row, "high", who, "candles", pos + 1);
        row_req_num(vm, row, "low", who, "candles", pos + 1);
        row_req_num(vm, row, "close", who, "candles", pos + 1);
        row_opt_num(vm, row, "volume", 0, who, "candles", pos + 1);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        cs[k].open = (float)row_req_num(vm, row, "open", who, "candles", k + 1);
        cs[k].high = (float)row_req_num(vm, row, "high", who, "candles", k + 1);
        cs[k].low = (float)row_req_num(vm, row, "low", who, "candles", k + 1);
        cs[k].close = (float)row_req_num(vm, row, "close", who, "candles", k + 1);
        cs[k].volume = (float)row_opt_num(vm, row, "volume", 0, who, "candles", k + 1);
        k++;
    }
    /* labels 借阅进 opts —— 单次调用内有效 */
    cui_candlestick_opts o;
    memset(&o, 0, sizeof o);
    o.labels = (const char *const *)labels;
    int hovered = cui_candlestick_ex(tui_need_ui(vm, (TObj *)self, who), id,
                                     cs, cnt, lc ? &o : NULL);
    tm_free(cs);
    tui_strs_free(labels, lc);
    return TV_INT(hovered);
}

static TVal nm_waterfall(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::waterfall()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "requests");
    int cnt = count_rows(rows);
    cui_net_request *reqs = cnt ? tm_malloc(sizeof(cui_net_request) * (size_t)cnt) : NULL;
    char **strs = NULL;
    int nstr = 0;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "requests", pos + 1);
        row_req_str(vm, row, "name", who, "requests", pos + 1);
        row_opt_str(vm, row, "method", who, "requests", pos + 1);
        row_opt_int(vm, row, "status", 0, who, "requests", pos + 1);
        row_opt_str(vm, row, "type", who, "requests", pos + 1);
        row_opt_num(vm, row, "size", 0, who, "requests", pos + 1);
        row_opt_num(vm, row, "start", 0, who, "requests", pos + 1);
        TVal *tv = row_get(row, "timing");
        if (tv) {
            int tn = 0;
            double *d = tui_floats(vm, tv, who, "requests.timing", &tn);
            tui_arr_free(d);
            if (tn > 6) {
                for (int z = 0; z < nstr; z++) tm_free(strs[z]);
                tm_free(strs);
                tm_free(reqs);
                RAISE(vm, "ValueError", "%s(): requests[%d][\"timing\"] has at most "
                      "6 entries", who, pos);
            }
        }
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        /* 字符串拷贝进 strs 池（调用后统一释放） */
        const char *keys[] = { "name", "method", "type" };
        char *got[3] = { NULL, NULL, NULL };
        for (int z = 0; z < 3; z++) {
            const char *s = row_opt_str(vm, row, keys[z], who, "requests", k + 1);
            if (z == 0) s = row_req_str(vm, row, keys[z], who, "requests", k + 1);
            got[z] = dup_s(s ? s : "");
            strs = tm_realloc(strs, sizeof(char *) * (size_t)(nstr + 1));
            strs[nstr++] = got[z];
        }
        reqs[k].name = got[0];
        reqs[k].method = got[1][0] ? got[1] : NULL;
        reqs[k].status = (int)row_opt_int(vm, row, "status", 0, who, "requests", k + 1);
        reqs[k].type = got[2][0] ? got[2] : NULL;
        reqs[k].size = row_opt_num(vm, row, "size", 0, who, "requests", k + 1);
        reqs[k].start = row_opt_num(vm, row, "start", 0, who, "requests", k + 1);
        for (int z = 0; z < 6; z++) reqs[k].timing[z] = 0;
        TVal *tv = row_get(row, "timing");
        if (tv && tv->t == T_ARR) {
            int tn = 0;
            double *d = tui_floats(vm, tv, who, "requests.timing", &tn);
            for (int z = 0; z < tn && z < 6; z++) reqs[k].timing[z] = (float)d[z];
            tui_arr_free(d);
        }
        k++;
    }
    cui_waterfall_result r = cui_waterfall_ex(tui_need_ui(vm, (TObj *)self, who),
                                              id, reqs, cnt, NULL);
    for (int z = 0; z < nstr; z++) tm_free(strs[z]);
    tm_free(strs);
    tm_free(reqs);
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("clicked", 7)), TV_INT(r.clicked));
    tarr_set(a, TV_STR(tstr_new("activated", 9)), TV_INT(r.activated));
    tarr_set(a, TV_STR(tstr_new("hovered", 7)), TV_INT(r.hovered));
    return TV_ARR(a);
}

/* dotChart：series=[['values'=>,'name'=>?,'accent'=>?]]，count 取首系列长度 */
static TVal nm_dot_chart(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::dotChart()";
    const char *id = tui_arg_str(vm, args, n, 0, who);
    TArr *rows = rows_of(vm, &args[1], who, "series");
    int sc = count_rows(rows);
    cui_dot_series *ds = sc ? tm_malloc(sizeof(cui_dot_series) * (size_t)sc) : NULL;
    char **names = sc ? tm_malloc(sizeof(char *) * (size_t)sc) : NULL;
    float **vals = sc ? tm_malloc(sizeof(float *) * (size_t)sc) : NULL;
    int *cnts = sc ? tm_malloc(sizeof(int) * (size_t)sc) : NULL;
    int total = 0;
    int pos = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = row_arr(vm, e, who, "series", pos + 1);
        row_opt_str(vm, row, "name", who, "series", pos + 1);
        TVal *vv = row_get(row, "values");
        if (!vv)
            RAISE(vm, "TypeError", "%s(): series[%d][\"values\"] is required", who, pos);
        int c = 0;
        double *d = tui_floats(vm, vv, who, "series.values", &c);
        tui_arr_free(d);
        row_opt_bool(vm, row, "accent", false, who, "series", pos + 1);
        if (pos == 0) total = c;
        else if (c != total)
            RAISE(vm, "ValueError", "%s(): all series must have the same values "
                  "length (%d vs %d)", who, c, total);
        pos++;
    }
    int k = 0;
    for (uint32_t i = 0; i < rows->dused; i++) {
        TVal e = rows->data[i];
        if (e.t == T_UNDEF) continue;
        TArr *row = (TArr *)e.u.p;
        const char *nm = row_opt_str(vm, row, "name", who, "series", k + 1);
        names[k] = dup_s(nm ? nm : "");
        int c = 0;
        vals[k] = flat_floats(vm, row_get(row, "values"), &c, who, "series.values");
        cnts[k] = c;
        ds[k].values = vals[k];
        ds[k].name = names[k][0] ? names[k] : NULL;
        ds[k].accent = row_opt_bool(vm, row, "accent", false, who, "series", k + 1);
        k++;
    }
    cui_dot_chart_result r = cui_dot_chart_ex(tui_need_ui(vm, (TObj *)self, who),
                                              id, ds, sc, total, NULL);
    for (int i = 0; i < sc; i++) { tm_free(names[i]); tm_free(vals[i]); }
    tm_free(names); tm_free(vals); tm_free(cnts); tm_free(ds);
    TArr *a = tarr_new();
    tarr_set(a, TV_STR(tstr_new("hovered", 7)), TV_INT(r.hovered));
    tarr_set(a, TV_STR(tstr_new("x", 1)), TV_FLOAT(r.x));
    tarr_set(a, TV_STR(tstr_new("value", 5)), TV_FLOAT(r.value));
    tarr_set(a, TV_STR(tstr_new("clicked", 7)), r.clicked ? TV_TRUE : TV_FALSE);
    return TV_ARR(a);
}

static TVal nm_timeline_item(VM *vm, void *self, TVal *args, int n) {
    const char *who = "Tphp\\Tui\\App::timelineItem()";
    const char *title = tui_arg_str(vm, args, n, 0, who);
    cui_timeline_item_ex(tui_need_ui(vm, (TObj *)self, who), title, NULL);
    return TV_NULL;
}

/* ---- 注册 ---- */

void tui_register_charts(TClass *cls) {
    tui_cls_add_native(cls, "lineChart", (void *)nm_line_chart, false);
    tui_cls_add_native(cls, "barChart", (void *)nm_bar_chart, false);
    tui_cls_add_native(cls, "donutChart", (void *)nm_donut_chart, false);
    tui_cls_add_native(cls, "sparkline", (void *)nm_sparkline, false);
    tui_cls_add_native(cls, "gauge", (void *)nm_gauge, false);
    tui_cls_add_native(cls, "radialGauge", (void *)nm_radial_gauge, false);
    tui_cls_add_native(cls, "scatterChart", (void *)nm_scatter_chart, false);
    tui_cls_add_native(cls, "radarChart", (void *)nm_radar_chart, false);
    tui_cls_add_native(cls, "heatmap", (void *)nm_heatmap, false);
    tui_cls_add_native(cls, "calendarHeatmap", (void *)nm_calendar_heatmap, false);
    tui_cls_add_native(cls, "treemap", (void *)nm_treemap, false);
    tui_cls_add_native(cls, "funnel", (void *)nm_funnel, false);
    tui_cls_add_native(cls, "sankey", (void *)nm_sankey, false);
    tui_cls_add_native(cls, "waffle", (void *)nm_waffle, false);
    tui_cls_add_native(cls, "splitBar", (void *)nm_split_bar, false);
    tui_cls_add_native(cls, "barList", (void *)nm_bar_list, false);
    tui_cls_add_native(cls, "activityRings", (void *)nm_activity_rings, false);
    tui_cls_add_native(cls, "candlestick", (void *)nm_candlestick, false);
    tui_cls_add_native(cls, "waterfall", (void *)nm_waterfall, false);
    tui_cls_add_native(cls, "dotChart", (void *)nm_dot_chart, false);
    tui_cls_add_native(cls, "timelineItem", (void *)nm_timeline_item, false);
}
