/*
 * tphp/tui - src/tui_util.c - Task 1 参数助手层实现（详见 tui_util.h）
 *
 * 数组助手一律两趟执行：第一趟全量类型校验 + 计数，第二趟才分配转换。
 * 原因：tphp_vm_raise 是 noreturn（长跳转展开），先分配再校验会在抛
 * 异常时泄漏已分配内存；两趟结构保证第二趟不可能再抛，天然无泄漏。
 */
#include "tui_util.h"
#include "builtin.h"    /* need_int（strict：仅 T_INT） */

#include <string.h>

/* ---- 参数读取（必需） ---- */

const char *tui_arg_str(VM *vm, TVal *args, int n, int i, const char *who) {
    if (n <= i || args[i].t != T_STR)
        tphp_vm_raise(vm, "TypeError", "%s() expects argument %d to be a string",
                      who, i + 1);
    return tstr_val((TStr *)args[i].u.p);
}

int64_t tui_arg_int(VM *vm, TVal *args, int n, int i, const char *who,
                    const char *pname) {
    if (n <= i)
        tphp_vm_raise(vm, "ArgumentCountError", "%s() expects at least %d argument(s)",
                      who, i + 1);
    return need_int(vm, &args[i], who, pname);
}

bool tui_arg_bool(VM *vm, TVal *args, int n, int i, const char *who,
                  const char *pname) {
    if (n <= i)
        tphp_vm_raise(vm, "ArgumentCountError", "%s() expects at least %d argument(s)",
                      who, i + 1);
    if (args[i].t != T_TRUE && args[i].t != T_FALSE)
        tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type bool, %s given",
                      who, pname, tphp_val_type_name(args[i]));
    return args[i].t == T_TRUE;
}

double tui_arg_float(VM *vm, TVal *args, int n, int i, const char *who,
                     const char *pname) {
    if (n <= i)
        tphp_vm_raise(vm, "ArgumentCountError", "%s() expects at least %d argument(s)",
                      who, i + 1);
    if (args[i].t == T_FLOAT) return args[i].u.d;
    if (args[i].t == T_INT) return (double)args[i].u.i;
    tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type float, %s given",
                  who, pname, tphp_val_type_name(args[i]));
}

/* ---- 参数读取（可选：缺省给默认值，给了但类型不对抛 TypeError） ---- */

int64_t tui_opt_int(VM *vm, TVal *args, int n, int i, int64_t def,
                    const char *who, const char *pname) {
    if (n <= i) return def;
    return need_int(vm, &args[i], who, pname);   /* strict：非 int 抛 TypeError */
}

bool tui_opt_bool(VM *vm, TVal *args, int n, int i, bool def,
                  const char *who, const char *pname) {
    if (n <= i) return def;
    if (args[i].t != T_TRUE && args[i].t != T_FALSE)
        tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type bool, %s given",
                      who, pname, tphp_val_type_name(args[i]));
    return args[i].t == T_TRUE;
}

const char *tui_opt_str(VM *vm, TVal *args, int n, int i, const char *def,
                        const char *who, const char *pname) {
    if (n <= i) return def;
    if (args[i].t != T_STR)
        tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type string, %s given",
                      who, pname, tphp_val_type_name(args[i]));
    return tstr_val((TStr *)args[i].u.p);
}

double tui_opt_float(VM *vm, TVal *args, int n, int i, double def,
                     const char *who, const char *pname) {
    if (n <= i) return def;
    if (args[i].t == T_FLOAT) return args[i].u.d;
    if (args[i].t == T_INT) return (double)args[i].u.i;
    tphp_vm_raise(vm, "TypeError", "%s(): argument %s must be of type float, %s given",
                  who, pname, tphp_val_type_name(args[i]));
}

/* ---- App 句柄（props["app"] = TV_INT 地址） ---- */

cui_app *tui_app_of(TObj *o) {
    int s = tphp_class_find_prop(o->cls, "app");
    if (s < 0) return NULL;
    TVal v = o->props[s];
    if (v.t != T_INT || v.u.i == 0) return NULL;
    return (cui_app *)(intptr_t)v.u.i;
}

cui_app *tui_need_app(VM *vm, TObj *o, const char *who) {
    cui_app *a = tui_app_of(o);
    if (!a)
        tphp_vm_raise(vm, "Error", "%s: window is already destroyed", who);
    return a;
}

cui_ctx *tui_need_ui(VM *vm, TObj *o, const char *who) {
    return cui_app_ui(tui_need_app(vm, o, who));
}

/* ---- 类注册 ---- */

void tui_cls_add_native(TClass *cls, const char *name, void *fn,
                        bool is_static) {
    TMethod *ms = tm_realloc(cls->methods,
                             sizeof(TMethod) * (size_t)(cls->n_methods + 1));
    TMethod *m = &ms[cls->n_methods];
    memset(m, 0, sizeof *m);
    m->name = tstr_intern(name, strlen(name));
    m->visibility = 0;            /* CVIS_PUBLIC */
    m->native = fn;
    m->is_static = is_static;
    cls->methods = ms;
    cls->n_methods++;
}

/* ---- 数组转换 ----
 * 迭代口径（array.c）：data[0..dused) 为插入序，T_UNDEF = unset 空洞跳过。
 * 第一趟校验 + 计数，第二趟分配转换（第二趟不再有抛出点）。 */

static TArr *tui_arr_of(VM *vm, TVal *v, const char *who, const char *pname) {
    if (v->t != T_ARR)
        tphp_vm_raise(vm, "TypeError",
                      "%s(): argument %s must be of type array, %s given",
                      who, pname, tphp_val_type_name(*v));
    return (TArr *)v->u.p;
}

/* 元素类型错误：位置 1 起、指明参数名 */
static void tui_elem_type_err(VM *vm, const char *who, const char *pname,
                              int pos, const char *want, TVal e) {
    tphp_vm_raise(vm, "TypeError",
                  "%s(): array element %d of argument %s must be of type %s, %s given",
                  who, pos, pname, want, tphp_val_type_name(e));
}

char **tui_strs(VM *vm, TVal *v, const char *who, const char *pname, int *n) {
    TArr *a = tui_arr_of(vm, v, who, pname);
    /* 第一趟：全量类型校验 + 计数 */
    int cnt = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_STR) tui_elem_type_err(vm, who, pname, cnt + 1, "string", e);
        cnt++;
    }
    *n = cnt;
    if (!cnt) return NULL;
    /* 第二趟：逐项拷贝（tstr_val 是 TStr 内部缓冲，必须拷贝才能跨帧持有） */
    char **out = tm_malloc(sizeof(char *) * (size_t)cnt);
    int k = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        TStr *s = (TStr *)e.u.p;
        char *c = tm_malloc((size_t)s->len + 1);
        memcpy(c, s->c, s->len);
        c[s->len] = '\0';
        out[k++] = c;
    }
    return out;
}

double *tui_floats(VM *vm, TVal *v, const char *who, const char *pname, int *n) {
    TArr *a = tui_arr_of(vm, v, who, pname);
    /* 第一趟：校验（允许 int/float，其余 TypeError）+ 计数 */
    int cnt = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_INT && e.t != T_FLOAT)
            tui_elem_type_err(vm, who, pname, cnt + 1, "int|float", e);
        cnt++;
    }
    *n = cnt;
    if (!cnt) return NULL;
    /* 第二趟：转换（tval_to_float：int 安全升格，float 原样） */
    double *out = tm_malloc(sizeof(double) * (size_t)cnt);
    int k = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        out[k++] = tval_to_float(e);
    }
    return out;
}

int64_t *tui_ints(VM *vm, TVal *v, const char *who, const char *pname, int *n) {
    TArr *a = tui_arr_of(vm, v, who, pname);
    /* 第一趟：校验（仅 int；float 不做静默截断）+ 计数 */
    int cnt = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_INT) tui_elem_type_err(vm, who, pname, cnt + 1, "int", e);
        cnt++;
    }
    *n = cnt;
    if (!cnt) return NULL;
    /* 第二趟：直取（已校验为 T_INT，无抛出点） */
    int64_t *out = tm_malloc(sizeof(int64_t) * (size_t)cnt);
    int k = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        out[k++] = e.u.i;
    }
    return out;
}

bool *tui_bools(VM *vm, TVal *v, const char *who, const char *pname, int *n) {
    TArr *a = tui_arr_of(vm, v, who, pname);
    /* 第一趟：校验（仅 bool）+ 计数 */
    int cnt = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        if (e.t != T_TRUE && e.t != T_FALSE)
            tui_elem_type_err(vm, who, pname, cnt + 1, "bool", e);
        cnt++;
    }
    *n = cnt;
    if (!cnt) return NULL;
    /* 第二趟：直取 */
    bool *out = tm_malloc(sizeof(bool) * (size_t)cnt);
    int k = 0;
    for (uint32_t i = 0; i < a->dused; i++) {
        TVal e = a->data[i];
        if (e.t == T_UNDEF) continue;
        out[k++] = (e.t == T_TRUE);
    }
    return out;
}

void tui_strs_free(char **a, int n) {
    if (!a) return;
    for (int i = 0; i < n; i++) tm_free(a[i]);
    tm_free(a);
}

void tui_arr_free(void *p) {
    tm_free(p);   /* tm_free(NULL) 安全；空数组返回 NULL 可直接传入 */
}
