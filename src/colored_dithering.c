#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

#define MAX(x,y) (((x)>(y))?(x):(y))
#define MIN(x,y) (((x)<(y))?(x):(y))
#define RGB_FORMATTED(x) ((x>255)?255:((x<0)?0:x))
#define THRESHOLD(x,y) ((x>=y)?255:0)
#define	CLAMP(x,xmin,xmax) (x) = MAX((xmin),(x)); (x) = MIN((xmax),(x))
#define _(x)	x

#define PLUG_IN_NAME "xavatu-colored-dithering"
#define PLUG_IN_VERSION "Dec. 2022, 0.0"
#define KEY_VALS "method"


/// предварительно подсчитанные матрицы Байера
const	int BAYER_PATTERN_2X2[2][2]		=	 	//	2x2 Bayer Dithering Matrix. Color levels: 5
{
    {	 51, 206	},
    {	153, 102	}
};

const	int BAYER_PATTERN_3X3[3][3]		=	 	//	3x3 Bayer Dithering Matrix. Color levels: 10
{
    {	 181, 231, 131	},
    {	  50,  25, 100	},
    {	 156,  75, 206	}
};

const	int BAYER_PATTERN_4X4[4][4]		=	 	//	4x4 Bayer Dithering Matrix. Color levels: 17
{
    {	 15, 195,  60, 240	},
    {	135,  75, 180, 120	},
    {	 45, 225,  30, 210	},
    {	165, 105, 150,  90	}

};

const	int BAYER_PATTERN_8X8[8][8]		=	 	//	8x8 Bayer Dithering Matrix. Color levels: 65
{
    {	  0, 128,  32, 160,   8, 136,  40, 168	},
    {	192,  64, 224,  96, 200,  72, 232, 104	},
    {	 48, 176,  16, 144,  56, 184,  24, 152	},
    {	240, 112, 208,  80, 248, 120, 216,  88	},
    {	 12, 140,  44, 172,   4, 132,  36, 164	},
    {	204,  76, 236, 108, 196,  68, 228, 100	},
    {	 60, 188,  28, 156,  52, 180,  20, 148	},
    {	252, 124, 220,  92, 244, 116, 212,  84	}
};

const	int	BAYER_PATTERN_16X16[16][16]	=	 	//	16x16 Bayer Dithering Matrix.  Color levels: 256
{
    {	  0, 191,  48, 239,  12, 203,  60, 251,   3, 194,  51, 242,  15, 206,  63, 254	},
    {	127,  64, 175, 112, 139,  76, 187, 124, 130,  67, 178, 115, 142,  79, 190, 127	},
    {	 32, 223,  16, 207,  44, 235,  28, 219,  35, 226,  19, 210,  47, 238,  31, 222	},
    {	159,  96, 143,  80, 171, 108, 155,  92, 162,  99, 146,  83, 174, 111, 158,  95	},
    {	  8, 199,  56, 247,   4, 195,  52, 243,  11, 202,  59, 250,   7, 198,  55, 246	},
    {	135,  72, 183, 120, 131,  68, 179, 116, 138,  75, 186, 123, 134,  71, 182, 119	},
    {	 40, 231,  24, 215,  36, 227,  20, 211,  43, 234,  27, 218,  39, 230,  23, 214	},
    {	167, 104, 151,  88, 163, 100, 147,  84, 170, 107, 154,  91, 166, 103, 150,  87	},
    {	  2, 193,  50, 241,  14, 205,  62, 253,   1, 192,  49, 240,  13, 204,  61, 252	},
    {	129,  66, 177, 114, 141,  78, 189, 126, 128,  65, 176, 113, 140,  77, 188, 125	},
    {	 34, 225,  18, 209,  46, 237,  30, 221,  33, 224,  17, 208,  45, 236,  29, 220	},
    {	161,  98, 145,  82, 173, 110, 157,  94, 160,  97, 144,  81, 172, 109, 156,  93	},
    {	 10, 201,  58, 249,   6, 197,  54, 245,   9, 200,  57, 248,   5, 196,  53, 244	},
    {	137,  74, 185, 122, 133,  70, 181, 118, 136,  73, 184, 121, 132,  69, 180, 117	},
    {	 42, 233,  26, 217,  38, 229,  22, 213,  41, 232,  25, 216,  37, 228,  21, 212	},
    {	169, 106, 153,  90, 165, 102, 149,  86, 168, 105, 152,  89, 164, 101, 148,  85	}
};


void query(void);
void run(const gchar *name, int nparams, const GimpParam *param, int *nreturn_vals, GimpParam **return_vals);
static gint dithering_dialog(void);


GimpPlugInInfo PLUG_IN_INFO =
{
    NULL, /* init_proc */
    NULL, /* quit_proc */
    query,        /* query_proc */
    run   /* run_proc */
};

enum
{
    _2x2,
    _3x3,
    _4x4,
    _8x8,
    _16x16
};

typedef struct
{
    gint run;
    gint method;
    gint ranges;
} Interface;

static Interface INTERFACE =
{
    FALSE,
    0,
    255
};

MAIN()

void query(void)
{
    /// аргументы плагина
    static GimpParamDef args[] =
    {
        { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
        { GIMP_PDB_IMAGE, "image", "Input image (unused)" },
        { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
        { GIMP_PDB_INT8, "method", "matrix size 0=2x2/1=3x3/2=4x4/4=8x8/5=16x16" },
        { GIMP_PDB_INT8, "ranges", "color ranges 0...255"}
    };

    static GimpParamDef *return_vals  = NULL;
    static int        nargs = sizeof(args) / sizeof(args[0]);
    static int        nreturn_vals = 0;

    /// установка процедуры в Gimp в качестве плагина
    gimp_install_procedure(
        PLUG_IN_NAME,
        "123",
        "123",
        "Eldar Dautov",
        "xavatu@gmail.com",
        PLUG_IN_VERSION,
        "<Image>/Filters/colored-dithering",
        "RGB*",
        GIMP_PLUGIN,
        nargs,
        nreturn_vals,
        args,
        return_vals);
}


int comp (guchar *elem1, guchar *elem2)
{
    int average1 = ((int) elem1[0] + (int) elem1[1] + (int) elem1[2])/3;
    int average2 = ((int) elem2[0] + (int) elem2[1] + (int) elem2[2])/3;

    if (average1 > average2) return  1;
    if (average1 < average2) return -1;
    return 0;
}


/// функция запуска плагина
void run(const gchar *name, int nparams, const GimpParam *param,
         int *nreturn_vals, GimpParam **return_vals)
{
    static GimpParam values[1];

    gint sel_x1, sel_y1, sel_x2, sel_y2;
    gint img_height, img_width, img_bpp, img_has_alpha;

    GimpDrawable        *drawable;
    GimpPixelRgn        dest_rgn, src_rgn, *pr;
    GimpRunMode         run_mode;
    GimpPDBStatusType   status;

    double progress, max_progress;

    guchar  *dest_row, *src_row, *dest, *src;
    int  r0, g0, b0, a=0, r1, g1, b1;
    gint row, col;

    *nreturn_vals = 1;
    *return_vals  = values;

    status = GIMP_PDB_SUCCESS;

    if (param[0].type!=GIMP_PDB_INT32)      status=GIMP_PDB_CALLING_ERROR;
    if (param[2].type!=GIMP_PDB_DRAWABLE)   status=GIMP_PDB_CALLING_ERROR;
    if (param[3].type!=GIMP_PDB_INT8)       INTERFACE.method=_3x3;
    else                                    INTERFACE.method = param[3].data.d_int8;
    if (param[4].type!=GIMP_PDB_INT8)       INTERFACE.ranges=1;
    else                                    INTERFACE.ranges = param[4].data.d_int8;



    run_mode = (GimpRunMode) param[0].data.d_int32;
    drawable = gimp_drawable_get(param[2].data.d_drawable);

    img_width     = gimp_drawable_width(drawable->drawable_id);
    img_height    = gimp_drawable_height(drawable->drawable_id);
    img_bpp       = gimp_drawable_bpp(drawable->drawable_id);
    img_has_alpha = gimp_drawable_has_alpha(drawable->drawable_id);
    gimp_drawable_mask_bounds(drawable->drawable_id, &sel_x1, &sel_y1, &sel_x2, &sel_y2);

    max_progress = (sel_x2-sel_x1)*(sel_y2-sel_y1);

    if (run_mode == GIMP_RUN_INTERACTIVE)
    {
        gimp_get_data(PLUG_IN_NAME, &INTERFACE.method);
        /// если интерактивный запуск, то выводим диалоговое окно
        if (! dithering_dialog())
            status = GIMP_PDB_EXECUTION_ERROR;
    }

    if (status == GIMP_PDB_SUCCESS)
    {
        gimp_tile_cache_ntiles((drawable->width + gimp_tile_width() - 1) / gimp_tile_width());

        if (strcmp(PLUG_IN_NAME,name) == 0 && status!=GIMP_PDB_CALLING_ERROR)
        {
            gimp_tile_cache_ntiles((drawable->width + gimp_tile_width() - 1) / gimp_tile_width());
            {
                /// прогресс-бар
                gimp_progress_init("Dithering...");
                progress = 0;
                int X, Y, x, y;
                int bayer_value, div=256/INTERFACE.ranges, corr;

                gimp_pixel_rgn_init(&dest_rgn, drawable, sel_x1, sel_y1, (sel_x2-sel_x1), (sel_y2-sel_y1), TRUE, TRUE);
                gimp_pixel_rgn_init(&src_rgn, drawable, sel_x1, sel_y1, (sel_x2-sel_x1), (sel_y2-sel_y1), FALSE, FALSE);

                for (pr = (GimpPixelRgn *) gimp_pixel_rgns_register(2, &src_rgn, &dest_rgn);
                        pr != NULL; pr = (GimpPixelRgn *) gimp_pixel_rgns_process(pr))
                {

                    dest_row = dest_rgn.data;
                    src_row = src_rgn.data;
                    int j = 0;
                    X = src_rgn.x;
                    Y = src_rgn.y;

                    for (row = 0; row < dest_rgn.h; row++)
                    {
                        dest = dest_row;
                        src = src_row;

                        for (col = 0; col < dest_rgn.w; col++)
                        {
                            /// получаем значение пикселей в 3-х каналах
                            r0 = *src++;
                            g0 = *src++;
                            b0 = *src++;
                            if (img_has_alpha)	a = *src++;

                            x = j % src_rgn.w + X;
                            y = j / src_rgn.w + Y;

                            switch (INTERFACE.method)
                            {
                            case _2x2:
                                bayer_value =  BAYER_PATTERN_2X2[x%2][y%2];
                                break;
                            case _3x3:
                                bayer_value =  BAYER_PATTERN_3X3[x%3][y%3];
                                break;
                            case _4x4:
                                bayer_value =  BAYER_PATTERN_4X4[x%4][y%4];
                                break;
                            case _8x8:
                                bayer_value =  BAYER_PATTERN_8X8[x%8][y%8];
                                break;
                            case _16x16:
                                bayer_value =  BAYER_PATTERN_16X16[x%16][y%16];
                                break;
                            }

                            corr = bayer_value / INTERFACE.ranges;
                            r1 = (r0 + corr) / div;
                            CLAMP(r1, 0, INTERFACE.ranges);
                            g1 = (g0 + corr) / div;
                            CLAMP(g1, 0, INTERFACE.ranges);
                            b1 = (b0 + corr) / div;
                            CLAMP(b1, 0, INTERFACE.ranges);

                            r1 *= div;
                            CLAMP(r1, 0, 255);
                            g1 *= div;
                            CLAMP(g1, 0, 255);
                            b1 *= div;
                            CLAMP(b1, 0, 255);

                            *dest++ = RGB_FORMATTED(r1);
                            *dest++ = RGB_FORMATTED(g1);
                            *dest++ = RGB_FORMATTED(b1);

                            /// альфа-канал оставляем без изменений
                            if (img_has_alpha)	*dest++ = (guchar) a;
                            j++;
                        }
                        dest_row += dest_rgn.rowstride;
                        src_row += src_rgn.rowstride;
                    }

                    /// обновляем прогресс-бар
                    progress += dest_rgn.w * dest_rgn.h;
                    gimp_progress_update((double) progress / max_progress);
                }
            }
        }

        /// обновляем изображение
        gimp_drawable_flush(drawable);
        gimp_drawable_merge_shadow(drawable->drawable_id, TRUE);
        gimp_drawable_update(drawable->drawable_id, sel_x1, sel_y1, (sel_x2-sel_x1), (sel_y2-sel_y1));
        gimp_displays_flush();
    }

    values[0].type = GIMP_PDB_STATUS;
    values[0].data.d_status = status;
    gimp_drawable_detach(drawable);
}


static gint
dithering_dialog (void)
{
    GtkWidget *dialog;
    GtkWidget *frame;
    GtkWidget *table;
    GtkWidget *main_vbox;
    GtkWidget *vbox;
    GtkObject *adj;
    gboolean   run;

    gimp_ui_init (KEY_VALS, FALSE);

    /// создаем новое диалоговое окно
    dialog = gimp_dialog_new(_(PLUG_IN_NAME), KEY_VALS,
                             NULL, 0,
                             NULL, "nothing",

                             GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                             GTK_STOCK_OK,     GTK_RESPONSE_OK,

                             NULL);

    main_vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 6);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), main_vbox,
                        FALSE, FALSE, 0);

    gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
                        GTK_SIGNAL_FUNC (gtk_main_quit),
                        NULL);

    table = gtk_table_new (2, 2, FALSE);
    gtk_table_set_col_spacings (GTK_TABLE (table), 6);
    gtk_table_set_row_spacings (GTK_TABLE (table), 6);
    gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);

    /// диалог выбора метода псевдотонирования
    frame = gimp_radio_group_new2 (TRUE, _("MODE"),
                                   gimp_radio_button_update,
                                   &INTERFACE.method,               (gpointer) INTERFACE.method,
                                   _("2x2 (5 color levels)"),       (gpointer) _2x2, NULL,
                                   _("3x3 (10 color levels)"),      (gpointer) _3x3, NULL,
                                   _("4x4 (17 color levels)"),      (gpointer) _4x4, NULL,
                                   _("8x8 (65 color levels)"),      (gpointer) _8x8, NULL,
                                   _("16x16 (256 color levels)"),   (gpointer) _16x16, NULL,
                                   NULL);
    gtk_table_attach (GTK_TABLE (table), frame, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    adj = gimp_scale_entry_new (GTK_TABLE (table), 0, 0,
                                _("ranges:"), 0, 0,
                                INTERFACE.ranges, 0, 255.0, 1.0, 0.1, 2,
                                TRUE, 0, 0,
                                NULL, NULL);
    gtk_signal_connect (GTK_OBJECT (adj), "value_changed",
                        GTK_SIGNAL_FUNC (gimp_int_adjustment_update),
                        &INTERFACE.ranges);

    /// вывод диалогового окна
    gtk_widget_show(frame);
    gtk_widget_show (table);
    gtk_widget_show (main_vbox);

    run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

    return run;
}