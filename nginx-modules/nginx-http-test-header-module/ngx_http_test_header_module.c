#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static ngx_int_t ngx_http_test_header_filter_init(ngx_conf_t *cf);

static ngx_int_t ngx_http_test_header_var_in(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);

static ngx_int_t ngx_http_test_header_var_out(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);    

static ngx_int_t ngx_http_test_header_add_vars(ngx_conf_t *cf);


typedef struct {
    ngx_str_t   name;
} ngx_http_test_header_loc_conf_t;

typedef struct {
    ngx_str_t value;
} ngx_http_test_header_ctx_t;


static void *ngx_http_test_header_create_loc_conf(ngx_conf_t *cf) {
    ngx_http_test_header_loc_conf_t *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_test_header_loc_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    return conf;
}

static char *ngx_http_test_header_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child) {
    ngx_http_test_header_loc_conf_t *prev = parent;
    ngx_http_test_header_loc_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->name, prev->name, "");

    return NGX_CONF_OK;
}

static ngx_command_t ngx_http_test_header_commands[] = {
    { ngx_string("test_header_name"), 
        NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
        ngx_conf_set_str_slot,
        NGX_HTTP_LOC_CONF_OFFSET,
        offsetof(ngx_http_test_header_loc_conf_t, name), 
        NULL },
    
    ngx_null_command
};


static ngx_http_variable_t  ngx_http_test_header_module_vars[] = {
    { ngx_string("test_header_in"), NULL, ngx_http_test_header_var_in, 
        0, NGX_HTTP_VAR_NOCACHEABLE, 0 },
    { ngx_string("test_header_out"), NULL, ngx_http_test_header_var_out, 
        0, NGX_HTTP_VAR_NOCACHEABLE, 0 },
    ngx_http_null_variable
};

static ngx_http_module_t ngx_http_test_header_module_ctx = {
    ngx_http_test_header_add_vars,          /* preconfiguration */
    ngx_http_test_header_filter_init,       /* postconfiguration */

    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    ngx_http_test_header_create_loc_conf,   /* create location configuration */
    ngx_http_test_header_merge_loc_conf     /* merge location configuration */
};


ngx_module_t ngx_http_test_header_module = {
    NGX_MODULE_V1,
    &ngx_http_test_header_module_ctx,      /* module context */
    ngx_http_test_header_commands,         /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;

static ngx_int_t ngx_http_test_header_filter(ngx_http_request_t *r) {
    ngx_table_elt_t *header;
    ngx_http_test_header_loc_conf_t *lcf;
    u_char *p;
    ngx_uint_t ident;
    ngx_http_test_header_ctx_t *ctx;

    if (r->headers_out.status != NGX_HTTP_OK) {
        return ngx_http_next_header_filter(r);
    }

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_test_header_module);
    if (lcf->name.len == 0) {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_test_header_module);
    if (ctx == NULL) {
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_test_header_ctx_t));
        if (ctx == NULL) {
            return NGX_ERROR;
        }
        ngx_http_set_ctx(r, ctx, ngx_http_test_header_module);
    }
    if (ctx->value.data != NULL) {
        return ngx_http_next_header_filter(r);
    }

    ident = ngx_random();
    p = ngx_pnalloc(r->pool, NGX_INT64_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    ctx->value.data = p;
    ctx->value.len = ngx_snprintf(p, NGX_INT64_LEN, "%ui", ident) - p;

    header = ngx_list_push(&r->headers_out.headers);
    if (header == NULL) {
        return NGX_ERROR;
    }
    header->key = lcf->name;
    header->hash = 1;
    header->value = ctx->value;

    return ngx_http_next_header_filter(r);
}

static ngx_int_t ngx_http_test_header_filter_init(ngx_conf_t *cf) {
    ngx_http_next_header_filter = ngx_http_top_header_filter;

    ngx_http_top_header_filter = ngx_http_test_header_filter;

    return NGX_OK;
}

static ngx_int_t ngx_http_test_header_add_vars(ngx_conf_t *cf) {
    ngx_http_variable_t  *var, *v;

    for (v = ngx_http_test_header_module_vars; v->name.len; ++v) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }
        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}

static ngx_int_t ngx_http_test_header_var_in(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data) {
    ngx_list_part_t *part;
    ngx_table_elt_t *header;
    ngx_uint_t i;
    ngx_http_test_header_loc_conf_t *lcf;
    lcf = ngx_http_get_module_loc_conf(r, ngx_http_test_header_module);

    if (lcf->name.len == 0) {
        return NGX_OK;
    }

    part = &r->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* no condition */; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break; 
            }
            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (ngx_strcasecmp(header[i].key.data, lcf->name.data) == 0) {
            v->len = header[i].value.len;
            v->data = header[i].value.data;
            v->valid = 1;
            v->no_cacheable = 0;
            v->not_found = 0;

            return NGX_OK;
        }
    }

    v->not_found = 1;

    return NGX_OK;
}


static ngx_int_t ngx_http_test_header_var_out(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data) {
    ngx_http_test_header_ctx_t *ctx;
    ctx = ngx_http_get_module_ctx(r, ngx_http_test_header_module);
    if (ctx == NULL || ctx->value.data == NULL) { 
        v->not_found = 1;
        return NGX_OK;
    }
    v->len = ctx->value.len;
    v->data = ctx->value.data;
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}