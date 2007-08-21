/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/**********************************************************
*      This software is part of the graphviz package      *
*                http://www.graphviz.org/                 *
*                                                         *
*            Copyright (c) 1994-2004 AT&T Corp.           *
*                and is licensed under the                *
*            Common Public License, Version 1.0           *
*                      by AT&T Corp.                      *
*                                                         *
*        Information and Software Systems Research        *
*              AT&T Research, Florham Park NJ             *
**********************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "const.h"

#include "gvplugin_render.h"
#include "graph.h"

typedef enum { FORMAT_VML, FORMAT_VMLZ, } format_type;

extern char *xml_string(char *str);
extern void core_init_compression(GVJ_t * job, compression_t compression);
extern void core_fini_compression(GVJ_t * job);
extern void core_fputs(GVJ_t * job, char *s);
extern void core_printf(GVJ_t * job, const char *format, ...);

char graphcoords[256];

#ifdef WIN32
static int
snprintf (char *str, int n, char *fmt, ...)
{
int ret;
va_list a;
va_start (a, fmt);
ret = _vsnprintf (str, n, fmt, a);
va_end (a);
return ret;
}
#endif

static void vml_bzptarray(GVJ_t * job, pointf * A, int n)
{
    int i;
    char *c;

    c = "m ";			/* first point */
    for (i = 0; i < n; i++) {
	core_printf(job, "%s%.0f,%.0f ", c, A[i].x, -A[i].y);
	if (i == 0)
	    c = "c ";		/* second point */
	else
	    c = "";		/* remaining points */
    }
}

static void vml_print_color(GVJ_t * job, gvcolor_t color)
{
    switch (color.type) {
    case COLOR_STRING:
	core_fputs(job, color.u.string);
	break;
    case RGBA_BYTE:
	if (color.u.rgba[3] == 0) /* transparent */
	    core_fputs(job, "none");
	else
	    core_printf(job, "#%02x%02x%02x",
		color.u.rgba[0], color.u.rgba[1], color.u.rgba[2]);
	break;
    default:
	assert(0);		/* internal error */
    }
}

static void vml_grstroke(GVJ_t * job, int filled)
{
    obj_state_t *obj = job->obj;

    core_fputs(job, "<v:stroke fillcolor=\"");
    if (filled)
	vml_print_color(job, obj->fillcolor);
    else
	core_fputs(job, "none");
    core_fputs(job, "\" strokecolor=\"");
    vml_print_color(job, obj->pencolor);
    if (obj->penwidth != PENWIDTH_NORMAL)
	core_printf(job, "\" stroke-weight=\"%g", obj->penwidth);
    if (obj->pen == PEN_DASHED) {
	core_fputs(job, "\" dashstyle=\"dash");
    } else if (obj->pen == PEN_DOTTED) {
	core_fputs(job, "\" dashstyle=\"dot");
    }
    core_fputs(job, "\" />");
}

static void vml_grstrokeattr(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    core_fputs(job, " strokecolor=\"");
    vml_print_color(job, obj->pencolor);
    if (obj->penwidth != PENWIDTH_NORMAL)
	core_printf(job, "\" stroke-weight=\"%g", obj->penwidth);
    if (obj->pen == PEN_DASHED) {
	core_fputs(job, "\" dashstyle=\"dash");
    } else if (obj->pen == PEN_DOTTED) {
	core_fputs(job, "\" dashstyle=\"dot");
    }
    core_fputs(job, "\"");
}

static void vml_grfill(GVJ_t * job, int filled)
{
    obj_state_t *obj = job->obj;

    core_fputs(job, "<v:fill color=\"");
    if (filled)
	vml_print_color(job, obj->fillcolor);
    else
	core_fputs(job, "none");
    core_fputs(job, "\" />");
}

static void vml_comment(GVJ_t * job, char *str)
{
    core_fputs(job, "      <!-- ");
    core_fputs(job, xml_string(str));
    core_fputs(job, " -->\n");
}

static void vml_begin_job(GVJ_t * job)
{
    switch (job->render.id) {
    case FORMAT_VMLZ:
	core_init_compression(job, COMPRESSION_ZLIB);
	break;
    case FORMAT_VML:
	core_init_compression(job, COMPRESSION_NONE);
	break;
    }

    core_fputs(job, "<?xml version=\"1.1\" encoding=\"UTF-8\" ?>\n");

    core_fputs(job, "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" ");
    core_fputs(job, "\"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n");
    core_fputs(job, "<html xml:lang=\"en\" xmlns=\"http://www.w3.org/1999/xhtml\" ");
    core_fputs(job, "xmlns:v=\"urn:schemas-microsoft-com:vml\""); 
    core_fputs(job, ">"); 

    core_fputs(job, "\n<!-- Generated by ");
    core_fputs(job, xml_string(job->common->info[0]));
    core_fputs(job, " version ");
    core_fputs(job, xml_string(job->common->info[1]));
    core_fputs(job, " (");
    core_fputs(job, xml_string(job->common->info[2]));
    core_fputs(job, ")\n     For user: ");
    core_fputs(job, xml_string(job->common->user));
    core_fputs(job, " -->\n");
}

static void vml_begin_graph(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    core_fputs(job, "<head>");
    if (obj->u.g->name[0]) {
        core_fputs(job, "<title>");
	core_fputs(job, xml_string(obj->u.g->name));
        core_fputs(job, "</title>");
    }
    core_printf(job, "<!-- Pages: %d -->\n</head>\n", job->pagesArraySize.x * job->pagesArraySize.y);

    snprintf(graphcoords, sizeof(graphcoords), "style=\"width: %.0fpt; height: %.0fpt\" coordsize=\"%.0f,%.0f\" coordorigin=\"-4,-%.0f\"",
	job->width*.75, job->height*.75,
        job->width*.75, job->height*.75,
        job->height*.75 - 4);

    core_printf(job, "<body>\n<div class=\"graph\" %s>\n", graphcoords);
    core_fputs(job, "<style type=\"text/css\">\nv\\:* {\nbehavior: url(#default#VML);display:inline-block;position: absolute; left: 0px; top: 0px;\n}\n</style>\n");
/*    graphcoords[0] = '\0'; */

}

static void vml_end_graph(GVJ_t * job)
{
    core_fputs(job, "</div>\n</body>\n");
    core_fini_compression(job);
}

static void
vml_begin_anchor(GVJ_t * job, char *href, char *tooltip, char *target)
{
    core_fputs(job, "      <a");
    if (href && href[0])
	core_printf(job, " href=\"%s\"", xml_string(href));
    if (tooltip && tooltip[0])
	core_printf(job, " title=\"%s\"", xml_string(tooltip));
    if (target && target[0])
	core_printf(job, " target=\"%s\"", xml_string(target));
    core_fputs(job, ">\n");
}

static void vml_end_anchor(GVJ_t * job)
{
    core_fputs(job, "      </a>\n");
}

static void vml_textpara(GVJ_t * job, pointf p, textpara_t * para)
{
    obj_state_t *obj = job->obj;

    core_fputs(job, "        <div");
    switch (para->just) {
    case 'l':
	core_fputs(job, " style=\"text-align: left; ");
	break;
    case 'r':
	core_fputs(job, " style=\"text-align: right; ");
	break;
    default:
    case 'n':
	core_fputs(job, " style=\"text-align: center; ");
	break;
    }
    core_printf(job, "position: absolute; left: %gpx; top: %gpx;", p.x/.75, job->height - p.y/.75 - 14);
    if (para->postscript_alias) {
        core_printf(job, " font-family: '%s';", para->postscript_alias->family);
        if (para->postscript_alias->weight)
	    core_printf(job, " font-weight: %s;", para->postscript_alias->weight);
        if (para->postscript_alias->stretch)
	    core_printf(job, " font-stretch: %s;", para->postscript_alias->stretch);
        if (para->postscript_alias->style)
	    core_printf(job, " font-style: %s;", para->postscript_alias->style);
    }
    else {
        core_printf(job, " font-family: \'%s\';", para->fontname);
    }
    /* FIXME - even inkscape requires a magic correction to fontsize.  Why?  */
    core_printf(job, " font-size: %.2fpt;", para->fontsize * 0.81);
    switch (obj->pencolor.type) {
    case COLOR_STRING:
	if (strcasecmp(obj->pencolor.u.string, "black"))
	    core_printf(job, "color:%s;", obj->pencolor.u.string);
	break;
    case RGBA_BYTE:
	core_printf(job, "color:#%02x%02x%02x;",
		obj->pencolor.u.rgba[0], obj->pencolor.u.rgba[1], obj->pencolor.u.rgba[2]);
	break;
    default:
	assert(0);		/* internal error */
    }
    core_fputs(job, "\">");
    core_fputs(job, xml_string(para->str));
    core_fputs(job, "</div>\n");
}

static void vml_ellipse(GVJ_t * job, pointf * A, int filled)
{
    /* A[] contains 2 points: the center and corner. */
    
    core_fputs(job, "        <v:oval");

    vml_grstrokeattr(job);

    core_fputs(job, " style=\"position: absolute;");

    core_printf(job, " left:  %gpt; top:    %gpt;", 2*A[0].x - A[1].x+4, job->height*.75 - A[1].y-4);
    core_printf(job, " width: %gpt; height: %gpt;", 2*(A[1].x - A[0].x), 2*(A[1].y - A[0].y));
    core_fputs(job, "\">");
    vml_grstroke(job, filled);
    vml_grfill(job, filled);
    core_fputs(job, "</v:oval>\n");
}

static void
vml_bezier(GVJ_t * job, pointf * A, int n, int arrow_at_start,
	      int arrow_at_end, int filled)
{
    core_printf(job, "        <v:shape %s><!-- bezier --><v:path", graphcoords);
    core_fputs(job, " v=\"");
    vml_bzptarray(job, A, n);
    core_fputs(job, "\" />");
    vml_grstroke(job, filled);
    core_fputs(job, "</v:path>");
    vml_grfill(job, filled);
    core_fputs(job, "</v:shape>\n");
}

static void vml_polygon(GVJ_t * job, pointf * A, int n, int filled)
{
    int i;

    core_fputs(job, "        <v:shape");
    vml_grstrokeattr(job);
    core_printf(job, " %s><!-- polygon --><v:path", graphcoords);
    core_fputs(job, " v=\"");
    for (i = 0; i < n; i++)
    {
        if (i==0) core_fputs(job, "m ");
	core_printf(job, "%.0f,%.0f ", A[i].x, -A[i].y);
        if (i==0) core_fputs(job, "l ");
        if (i==n-1) core_fputs(job, "x e ");
    }
    core_fputs(job, "\">");
    vml_grstroke(job, filled);
    core_fputs(job, "</v:path>");
    vml_grfill(job, filled);
    core_fputs(job, "</v:shape>\n");
}

static void vml_polyline(GVJ_t * job, pointf * A, int n)
{
    int i;

    core_printf(job, "        <v:shape %s><!-- polyline --><v:path", graphcoords);
    core_fputs(job, " v=\"");
    for (i = 0; i < n; i++)
    {
        if (i==0) core_fputs(job, " m ");
	core_printf(job, "%.0f,%.0f ", A[i].x, -A[i].y);
        if (i==0) core_fputs(job, " l ");
        if (i==n-1) core_fputs(job, " e "); /* no x here for polyline */
    }
    core_fputs(job, "\">");
    vml_grstroke(job, 0);                 /* no fill here for polyline */
    core_fputs(job, "</v:path>");
    core_fputs(job, "</v:shape>\n");

}

/* color names from http://www.w3.org/TR/VML/types.html */
/* NB.  List must be LANG_C sorted */
static char *vml_knowncolors[] = {
    "aliceblue", "antiquewhite", "aqua", "aquamarine", "azure",
    "beige", "bisque", "black", "blanchedalmond", "blue",
    "blueviolet", "brown", "burlywood",
    "cadetblue", "chartreuse", "chocolate", "coral",
    "cornflowerblue", "cornsilk", "crimson", "cyan",
    "darkblue", "darkcyan", "darkgoldenrod", "darkgray",
    "darkgreen", "darkgrey", "darkkhaki", "darkmagenta",
    "darkolivegreen", "darkorange", "darkorchid", "darkred",
    "darksalmon", "darkseagreen", "darkslateblue", "darkslategray",
    "darkslategrey", "darkturquoise", "darkviolet", "deeppink",
    "deepskyblue", "dimgray", "dimgrey", "dodgerblue",
    "firebrick", "floralwhite", "forestgreen", "fuchsia",
    "gainsboro", "ghostwhite", "gold", "goldenrod", "gray",
    "green", "greenyellow", "grey",
    "honeydew", "hotpink", "indianred",
    "indigo", "ivory", "khaki",
    "lavender", "lavenderblush", "lawngreen", "lemonchiffon",
    "lightblue", "lightcoral", "lightcyan", "lightgoldenrodyellow",
    "lightgray", "lightgreen", "lightgrey", "lightpink",
    "lightsalmon", "lightseagreen", "lightskyblue",
    "lightslategray", "lightslategrey", "lightsteelblue",
    "lightyellow", "lime", "limegreen", "linen",
    "magenta", "maroon", "mediumaquamarine", "mediumblue",
    "mediumorchid", "mediumpurple", "mediumseagreen",
    "mediumslateblue", "mediumspringgreen", "mediumturquoise",
    "mediumvioletred", "midnightblue", "mintcream",
    "mistyrose", "moccasin",
    "navajowhite", "navy", "oldlace",
    "olive", "olivedrab", "orange", "orangered", "orchid",
    "palegoldenrod", "palegreen", "paleturquoise",
    "palevioletred", "papayawhip", "peachpuff", "peru", "pink",
    "plum", "powderblue", "purple",
    "red", "rosybrown", "royalblue",
    "saddlebrown", "salmon", "sandybrown", "seagreen", "seashell",
    "sienna", "silver", "skyblue", "slateblue", "slategray",
    "slategrey", "snow", "springgreen", "steelblue",
    "tan", "teal", "thistle", "tomato", "turquoise",
    "violet",
    "wheat", "white", "whitesmoke",
    "yellow", "yellowgreen"
};

gvrender_engine_t vml_engine = {
    vml_begin_job,
    0,				/* vml_end_job */
    vml_begin_graph,
    vml_end_graph,
    0,                          /* vml_begin_layer */
    0,                          /* vml_end_layer */
    0,                          /* vml_begin_page */
    0,                          /* vml_end_page */
    0,                          /* vml_begin_cluster */
    0,                          /* vml_end_cluster */
    0,				/* vml_begin_nodes */
    0,				/* vml_end_nodes */
    0,				/* vml_begin_edges */
    0,				/* vml_end_edges */
    0,                          /* vml_begin_node */
    0,                          /* vml_end_node */
    0,                          /* vml_begin_edge */
    0,                          /* vml_end_edge */
    vml_begin_anchor,
    vml_end_anchor,
    vml_textpara,
    0,				/* vml_resolve_color */
    vml_ellipse,
    vml_polygon,
    vml_bezier,
    vml_polyline,
    vml_comment,
    0,				/* vml_library_shape */
};

gvrender_features_t vml_features = {
    GVRENDER_DOES_TRUECOLOR
	| GVRENDER_Y_GOES_DOWN
        | GVRENDER_DOES_TRANSFORM
	| GVRENDER_DOES_LABELS
	| GVRENDER_DOES_MAPS
	| GVRENDER_DOES_TARGETS
	| GVRENDER_DOES_TOOLTIPS, /* flags */
    DEFAULT_EMBED_MARGIN,	/* default margin - points */
    4.,                         /* default pad - graph units */
    {0.,0.},                    /* default page width, height - points */
    {96.,96.},			/* default dpi */
    vml_knowncolors,		/* knowncolors */
    sizeof(vml_knowncolors) / sizeof(char *),	/* sizeof knowncolors */
    RGBA_BYTE,			/* color_type */
    NULL,                       /* device */
    "vml",                      /* imageloader for usershapes */
    NULL,                       /* formatter */
};

gvplugin_installed_t gvrender_core_vml_types[] = {
    {FORMAT_VML, "vml", 1, &vml_engine, &vml_features},
#if HAVE_LIBZ
    {FORMAT_VMLZ, "vmlz", 1, &vml_engine, &vml_features},
#endif
    {0, NULL, 0, NULL, NULL}
};
