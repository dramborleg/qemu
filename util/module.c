/*
 * QEMU Module Infrastructure
 *
 * Copyright IBM, Corp. 2009
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "qemu-common.h"
#include <gmodule.h>
#include "qapi/error.h"
#include "qemu/queue.h"
#include "qemu/module.h"

typedef struct ModuleEntry
{
    void (*init)(void);
    QTAILQ_ENTRY(ModuleEntry) node;
    module_init_type type;
} ModuleEntry;

typedef QTAILQ_HEAD(, ModuleEntry) ModuleTypeList;

static ModuleTypeList init_type_list[MODULE_INIT_MAX];

static ModuleTypeList dso_init_list;

static void init_lists(void)
{
    static int inited;
    int i;

    if (inited) {
        return;
    }

    for (i = 0; i < MODULE_INIT_MAX; i++) {
        QTAILQ_INIT(&init_type_list[i]);
    }

    QTAILQ_INIT(&dso_init_list);

    inited = 1;
}


static ModuleTypeList *find_type(module_init_type type)
{
    ModuleTypeList *l;

    init_lists();

    l = &init_type_list[type];

    return l;
}

void register_module_init(void (*fn)(void), module_init_type type)
{
    ModuleEntry *e;
    ModuleTypeList *l;

    e = g_malloc0(sizeof(*e));
    e->init = fn;
    e->type = type;

    l = find_type(type);

    QTAILQ_INSERT_TAIL(l, e, node);
}

void register_dso_module_init(void (*fn)(void), module_init_type type)
{
    ModuleEntry *e;

    init_lists();

    e = g_malloc0(sizeof(*e));
    e->init = fn;
    e->type = type;

    QTAILQ_INSERT_TAIL(&dso_init_list, e, node);
}

void module_call_init(module_init_type type)
{
    ModuleTypeList *l;
    ModuleEntry *e;

    l = find_type(type);

    QTAILQ_FOREACH(e, l, node) {
        e->init();
    }
}

#ifdef CONFIG_MODULES
static GModule *module_load_file(const char *fname, Error **errp)
{
    GModule *g_module;
    void (*sym)(void);
    const char *dsosuf = HOST_DSOSUF;
    int len = strlen(fname);
    int suf_len = strlen(dsosuf);
    ModuleEntry *e, *next;

    if (len <= suf_len || strcmp(&fname[len - suf_len], dsosuf)) {
        error_setg(errp, "Module has wrong suffix\n");
        return NULL;
    }
    if (access(fname, F_OK)) {
        error_setg(errp, "Module does not exist\n");
        return NULL;
    }

    assert(QTAILQ_EMPTY(&dso_init_list));

    g_module = g_module_open(fname, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
    if (!g_module) {
        error_setg(errp, "Failed to open module: %s\n", g_module_error());
        return NULL;
    }
    if (!g_module_symbol(g_module, DSO_STAMP_FUN_STR, (gpointer *)&sym)) {
        error_setg(errp, "Failed to initialize module: %s\n", fname);
        /* Print some info if this is a QEMU module (but from different build),
         * this will make debugging user problems easier. */
        if (g_module_symbol(g_module, "qemu_module_dummy", (gpointer *)&sym)) {
            error_append_hint(errp, "Note: only modules from the same build \
                              can be loaded.\n");
        }
        g_module_close(g_module);
        g_module = NULL;
    } else {
        QTAILQ_FOREACH(e, &dso_init_list, node) {
            register_module_init(e->init, e->type);
        }
    }

    QTAILQ_FOREACH_SAFE(e, &dso_init_list, node, next) {
        QTAILQ_REMOVE(&dso_init_list, e, node);
        g_free(e);
    }

    return g_module;
}
#endif

GModule *module_load_one(const char *prefix, const char *lib_name)
{
#ifdef CONFIG_MODULES
    Error *local_err = NULL;
    GModule *g_module = NULL;
    char *fname = NULL;
    char *exec_dir;
    char *dirs[3];
    int i = 0;

    if (!g_module_supported()) {
        fprintf(stderr, "Module is not supported by system.\n");
        return NULL;
    }

    exec_dir = qemu_get_exec_dir();
    dirs[i++] = g_strdup_printf("%s", CONFIG_QEMU_MODDIR);
    dirs[i++] = g_strdup_printf("%s/..", exec_dir ? : "");
    dirs[i++] = g_strdup_printf("%s", exec_dir ? : "");
    assert(i == ARRAY_SIZE(dirs));
    g_free(exec_dir);
    exec_dir = NULL;

    for (i = 0; i < ARRAY_SIZE(dirs); i++) {
        fname = g_strdup_printf("%s/%s%s%s",
                dirs[i], prefix, lib_name, HOST_DSOSUF);
        g_module = module_load_file(fname, &local_err);
        g_free(fname);
        fname = NULL;
        /* Try loading until loaded a module file */
        if (!local_err) {
            break;
        }
        error_free(local_err);
        local_err = NULL;
    }

    for (i = 0; i < ARRAY_SIZE(dirs); i++) {
        g_free(dirs[i]);
    }

    return g_module;
#else
    return NULL;
#endif
}
