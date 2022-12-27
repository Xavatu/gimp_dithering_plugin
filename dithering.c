#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "libgimp/gimp.h"
#include "libgimp/gimpui.h"

#define GET_MAX(x,y) (((x)>(y))?(x):(y))
#define GET_MIN(x,y) (((x)<(y))?(x):(y))
#define RGB_FORMATTED(x) ((x>255)?255:((x<0)?0:x))
#define THRESHOLD(x,y) ((x>=y)?255:0)
#define _(x)	x

#define PLUG_IN_NAME "xavatu-dithering"
#define PLUG_IN_VERSION "Dec. 2022, 0.0"
#define KEY_VALS "method"


void query(void);
void run(const gchar *name, int nparams, const GimpParam *param, int *nreturn_vals, GimpParam **return_vals);
static gint dithering_dialog(void);


GimpPlugInInfo PLUG_IN_INFO = {
        NULL, /* init_proc */
        NULL, /* quit_proc */
        query,        /* query_proc */
        run   /* run_proc */
};

enum {
    threshold_dithering,    /// обычный порог
    random_dithering,       /// рандомный порог
    shift_dithering         /// сдвиговый (с распространением ошибки)
};

typedef struct
{
    gint run;
    gint method;
} Interface;

static Interface INTERFACE =
{
        FALSE
};

MAIN()

void query(void) {
    /// аргументы плагина
    static GimpParamDef args[] = {
            { GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive" },
            { GIMP_PDB_IMAGE, "image", "Input image (unused)" },
            { GIMP_PDB_DRAWABLE, "drawable", "Input drawable" },
            { GIMP_PDB_INT8, "method", "method 0=threshold/1=random/2=shift" }
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
            "<Image>/Filters/dithering",
            "RGB*",
            GIMP_PLUGIN,
            nargs,
            nreturn_vals,
            args,
            return_vals);
}


/// функция запуска плагина
void run(const gchar *name, int nparams, const GimpParam *param,
         int *nreturn_vals, GimpParam **return_vals) {
    static GimpParam values[1];

    gint sel_x1, sel_y1, sel_x2, sel_y2;
    gint img_height, img_width, img_bpp, img_has_alpha;

    GimpDrawable        *drawable;
    GimpPixelRgn        dest_rgn, src_rgn, *pr;
    GimpRunMode         run_mode;
    GimpPDBStatusType   status;

    double progress, max_progress;

    guchar * dest_row, *src_row, *dest, *src;
    double  r0, g0, b0, a=0, r1, g1, b1;
    gint row, col;

    *nreturn_vals = 1;
    *return_vals  = values;

    status = GIMP_PDB_SUCCESS;

    if (param[0].type!=GIMP_PDB_INT32)      status=GIMP_PDB_CALLING_ERROR;
    if (param[2].type!=GIMP_PDB_DRAWABLE)   status=GIMP_PDB_CALLING_ERROR;
    if (param[3].type!=GIMP_PDB_INT8)       INTERFACE.method=threshold_dithering;
    else INTERFACE.method = param[3].data.d_int8;

    run_mode = (GimpRunMode) param[0].data.d_int32;

    drawable = gimp_drawable_get(param[2].data.d_drawable);

    img_width     = gimp_drawable_width(drawable->drawable_id);
    img_height    = gimp_drawable_height(drawable->drawable_id);
    img_bpp       = gimp_drawable_bpp(drawable->drawable_id);
    img_has_alpha = gimp_drawable_has_alpha(drawable->drawable_id);
    gimp_drawable_mask_bounds(drawable->drawable_id, &sel_x1, &sel_y1, &sel_x2, &sel_y2);

    max_progress = (sel_x2-sel_x1)*(sel_y2-sel_y1);

    if (run_mode == GIMP_RUN_INTERACTIVE) {
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
                int error = 0;

                gimp_pixel_rgn_init(&dest_rgn, drawable, sel_x1, sel_y1, (sel_x2-sel_x1), (sel_y2-sel_y1), TRUE, TRUE);
                gimp_pixel_rgn_init(&src_rgn, drawable, sel_x1, sel_y1, (sel_x2-sel_x1), (sel_y2-sel_y1), FALSE, FALSE);

                for (pr = (GimpPixelRgn *) gimp_pixel_rgns_register(2, &src_rgn, &dest_rgn);
                     pr != NULL;
                     pr = (GimpPixelRgn *) gimp_pixel_rgns_process(pr)) {

                    dest_row = dest_rgn.data;
                    src_row = src_rgn.data;
                    for (row = 0; row < dest_rgn.h; row++) {
                        dest = dest_row;
                        src = src_row;
                        for (col = 0; col < dest_rgn.w; col++) {
                            /// получаем значение пикселей в 3-х каналах
                            r0 = *src++;
                            g0 = *src++;
                            b0 = *src++;
                            if (img_has_alpha)	a = *src++;

                            /// выбираем среднее значение из трех каналов по формуле AVERAGE = (MIN(R,G,B) + MAX(R,G,B)) / 2
                            r1 = (GET_MIN(GET_MIN(r0, g0), b0) + GET_MAX(GET_MAX(r0, g0), b0)) / 2;

                            switch (INTERFACE.method) {
                                case threshold_dithering:
                                    /// применяем порог со значением 127
                                    r1 = THRESHOLD(r1, 127);
                                    *dest++ = r1;
                                    *dest++ = r1;
                                    *dest++ = r1;
                                    break;
                                case random_dithering:
                                    /// рандомно выбираем значение в диапазоне (0, 255)
                                    /// и если оно больше текущего, то значение 0, иначе 255
                                    /// (порог со значением r1)
                                    g1 = THRESHOLD(r1, random() % 256);
                                    *dest++ = g1;
                                    *dest++ = g1;
                                    *dest++ = g1;
                                    break;
                                case shift_dithering:
                                    /// получаем новое значения путем суммирования со значением ошибки
                                    /// применяем порог со значением 127 и считаем новое значение ошибки
                                    /// current = pixel + error
                                    /// error = current - result
                                    b1 = RGB_FORMATTED(r1 + error);
                                    g1 = b1;
                                    b1 = THRESHOLD(b1, 127);
                                    error = (int) g1 - b1;
                                    *dest++ = b1;
                                    *dest++ = b1;
                                    *dest++ = b1;
                                    break;
                            }

                            /// альфа-канал оставляем без изменений
                            if (img_has_alpha)	*dest++ = (guchar) a;
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

    /// диалог выбора метода Дизеринга
    frame = gimp_radio_group_new2 (TRUE, _(KEY_VALS),
                                   gimp_radio_button_update,
                                   &INTERFACE.method, (gpointer) INTERFACE.method,
                                   _("threshold"),  (gpointer) threshold_dithering, NULL,
                                   _("random"),     (gpointer) random_dithering, NULL,
                                   _("shift"),      (gpointer) shift_dithering, NULL,

                                   NULL);
    gtk_table_attach (GTK_TABLE (table), frame, 0, 1, 1, 2,
                      GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 0, 0);

    /// вывод диалогового окна
    gtk_widget_show(frame);
    gtk_widget_show (table);
    gtk_widget_show (main_vbox);

    run = (gimp_dialog_run (GIMP_DIALOG (dialog)) == GTK_RESPONSE_OK);

    return run;
}