#include "../lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include "lv_examples/lv_apps/demo/demo.h"
#include "lv_examples/lv_apps/benchmark/benchmark.h"
#include "common/loadbmp.h"
#include <unistd.h>
#include <linux/fb.h>
#include <hi_type.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <hifb.h>
#include <sys/mman.h>
#include <hi_comm_vo.h>
#include <mpi_vo.h>
#include <fcntl.h>
#include <pthread.h>

typedef struct hiPTHREAD_HIFB_SAMPLE {
    int fd;
    int layer;
    int ctrlkey;
} PTHREAD_HIFB_SAMPLE_INFO;

typedef enum {
    HIFB_LAYER_0 = 0x0,
    HIFB_LAYER_1,
    HIFB_LAYER_2,
    HIFB_LAYER_3,
    HIFB_LAYER_4,
    HIFB_LAYER_CURSOR_0,
    HIFB_LAYER_CURSOR_1,
    //HIFB_LAYER_CURSOR,
            HIFB_LAYER_ID_BUTT
} HIFB_LAYER_ID_E;
#define SAMPLE_CURSOR_PATH        "./res/cursor.bmp"
#define SAMPLE_IMAGE_PATH        "./res/%d.bmp"
#define SAMPLE_IMAGE_WIDTH     184
#define SAMPLE_IMAGE_HEIGHT    144
#define SAMPLE_VIR_SCREEN_WIDTH        SAMPLE_IMAGE_WIDTH            /*virtual screen width*/
#define SAMPLE_VIR_SCREEN_HEIGHT    SAMPLE_IMAGE_HEIGHT*2        /*virtual screen height*/
static struct fb_bitfield g_r16 = {10, 5, 0};
static struct fb_bitfield g_g16 = {5, 5, 0};
static struct fb_bitfield g_b16 = {0, 5, 0};
static struct fb_bitfield g_a16 = {15, 1, 0};
#define SAMPLE_IMAGE_NUM       20
static VO_DEV VoDev = 0;

HI_S32 SAMPLE_HIFB_LoadBmp(const char *filename, HI_U8 *pAddr) {
    OSD_SURFACE_S Surface;
    OSD_BITMAPFILEHEADER bmpFileHeader;
    OSD_BITMAPINFO bmpInfo;

    if (GetBmpInfo(filename, &bmpFileHeader, &bmpInfo) < 0) {
        printf("GetBmpInfo err!\n");
        return HI_FAILURE;
    }

    Surface.enColorFmt = OSD_COLOR_FMT_RGB1555;

    CreateSurfaceByBitMap(filename, &Surface, pAddr);

    return HI_SUCCESS;
}

HIFB_POINT_S point = {0, 0};
int fd = 0;

bool mouse_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    bool a = evdev_read(data);
    printf("x:%i,y:%i\n", data->point.x, data->point.y);
    point.s32XPos = data->point.x;
    point.s32YPos = data->point.y;
    ioctl(fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &point);
    return a;
}


HI_VOID *SAMPLE_HIFB_PTHREAD_RunHiFB(void *pData) {
    HI_S32 i;
    struct fb_fix_screeninfo fix;
    struct fb_var_screeninfo var;
    HI_U32 u32FixScreenStride = 0;
    unsigned char *pShowScreen;
    unsigned char *pHideScreen;
    HIFB_ALPHA_S stAlpha;
    HIFB_POINT_S stPoint = {40, 112};
    char file[12] = "/dev/fb0";
#if HICHIP == HI3531_V100
    HI_BOOL g_bCompress = HI_TRUE;
#endif
    char image_name[128];
    HI_U8 *pDst = NULL;
    HI_BOOL bShow;
    PTHREAD_HIFB_SAMPLE_INFO *pstInfo;
    HIFB_COLORKEY_S stColorKey;

    if (HI_NULL == pData) {
        return HI_NULL;
    }
    pstInfo = (PTHREAD_HIFB_SAMPLE_INFO *) pData;
    switch (pstInfo->layer) {
        case 0 :
            strcpy(file, "/dev/fb0");
            break;
        case 1 :
            strcpy(file, "/dev/fb1");
            break;
        case 2 :
            strcpy(file, "/dev/fb2");
            break;
        case 3 :
            strcpy(file, "/dev/fb3");
            break;
        case 4 :
            strcpy(file, "/dev/fb4");
            break;
        case 5 :
            strcpy(file, "/dev/fb5");
            break;
        case 6 :
            strcpy(file, "/dev/fb6");
            break;
        case 7 :
            strcpy(file, "/dev/fb7");
            break;
        default:
            strcpy(file, "/dev/fb0");
            break;
    }

    /* 1. open framebuffer device overlay 0 */
    pstInfo->fd = open(file, O_RDWR, 0);
    if (pstInfo->fd < 0) {
        printf("open %s failed!\n", file);
        return HI_NULL;
    }
#if HICHIP == HI3531_V100
    if (pstInfo->layer >= HIFB_LAYER_0 && pstInfo->layer <= HIFB_LAYER_4) {
        if (ioctl(pstInfo->fd, FBIOPUT_COMPRESSION_HIFB, &g_bCompress) < 0) {
            printf("Func:%s line:%d FBIOPUT_COMPRESSION_HIFB failed!\n",
                   __FUNCTION__, __LINE__);
            close(pstInfo->fd);
            return HI_NULL;
        }
    }
#endif
    bShow = HI_FALSE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0) {
        printf("FBIOPUT_SHOW_HIFB failed!\n");
        return HI_NULL;
    }
    /* 2. set the screen original position */
    switch (pstInfo->ctrlkey) {
        case 0: {
            stPoint.s32XPos = 100;
            stPoint.s32YPos = 100;
        }
            break;

        case 1: {
            stPoint.s32XPos = 150;
            stPoint.s32YPos = 350;
        }
            break;

        case 2: {
            stPoint.s32XPos = 384;
            stPoint.s32YPos = 100;
        }
            break;
        case 3: {
            stPoint.s32XPos = 150;
            stPoint.s32YPos = 150;
        }
            break;
        default: {
            stPoint.s32XPos = 0;
            stPoint.s32YPos = 0;
        }
    }

    if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0) {
        printf("set screen original show position failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    /* 3.set alpha */
    stAlpha.bAlphaEnable = HI_TRUE;
    stAlpha.bAlphaChannel = HI_TRUE;
    stAlpha.u8Alpha0 = 0xff;
    stAlpha.u8Alpha1 = 0xff;
    stAlpha.u8GlobalAlpha = 0x80;
    if (ioctl(pstInfo->fd, FBIOPUT_ALPHA_HIFB, &stAlpha) < 0) {
        printf("Set alpha failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    if (pstInfo->layer == HIFB_LAYER_CURSOR_0 || pstInfo->layer == HIFB_LAYER_CURSOR_1) {
        stColorKey.bKeyEnable = HI_TRUE;
        stColorKey.u32Key = 0x0;
        if (ioctl(pstInfo->fd, FBIOPUT_COLORKEY_HIFB, &stColorKey) < 0) {
            printf("FBIOPUT_COLORKEY_HIFB!\n");
            close(pstInfo->fd);
            return HI_NULL;
        }
    }
    /* 4. get the variable screen info */
    if (ioctl(pstInfo->fd, FBIOGET_VSCREENINFO, &var) < 0) {
        printf("Get variable screen info failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    /* 5. modify the variable screen info
          the screen size: IMAGE_WIDTH*IMAGE_HEIGHT
          the virtual screen size: VIR_SCREEN_WIDTH*VIR_SCREEN_HEIGHT
          (which equals to VIR_SCREEN_WIDTH*(IMAGE_HEIGHT*2))
          the pixel format: ARGB1555
    */
    usleep(4 * 1000 * 1000);
    switch (pstInfo->ctrlkey) {
        case 0: {
            var.xres_virtual = 104;
            var.yres_virtual = 200;
            var.xres = 104;
            var.yres = 100;
        }
            break;

        case 1: {
            var.xres_virtual = 100;
            var.yres_virtual = 100;
            var.xres = 100;
            var.yres = 100;
        }
            break;

        case 2: {
            var.xres_virtual = SAMPLE_VIR_SCREEN_WIDTH;
            var.yres_virtual = SAMPLE_VIR_SCREEN_HEIGHT;
            var.xres = SAMPLE_IMAGE_WIDTH;
            var.yres = SAMPLE_IMAGE_HEIGHT;
        }
            break;
        case 3: {
            var.xres_virtual = 48;
            var.yres_virtual = 48;
            var.xres = 48;
            var.yres = 48;
        }
            break;
        default: {
            var.xres_virtual = 98;
            var.yres_virtual = 128;
            var.xres = 98;
            var.yres = 64;
        }
    }

    var.transp = g_a16;
    var.red = g_r16;
    var.green = g_g16;
    var.blue = g_b16;
    var.bits_per_pixel = 16;
    var.activate = FB_ACTIVATE_NOW;

    /* 6. set the variable screeninfo */
    if (ioctl(pstInfo->fd, FBIOPUT_VSCREENINFO, &var) < 0) {
        printf("Put variable screen info failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    /* 7. get the fix screen info */
    if (ioctl(pstInfo->fd, FBIOGET_FSCREENINFO, &fix) < 0) {
        printf("Get fix screen info failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }
    u32FixScreenStride = fix.line_length;   /*fix screen stride*/

    /* 8. map the physical video memory for user use */
    pShowScreen = static_cast<unsigned char *>(mmap(HI_NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                                                    pstInfo->fd, 0));
    if (MAP_FAILED == pShowScreen) {
        printf("mmap framebuffer failed!\n");
        close(pstInfo->fd);
        return HI_NULL;
    }

    memset(pShowScreen, 0x83E0, fix.smem_len);

    /* time to paly*/
    bShow = HI_TRUE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0) {
        printf("FBIOPUT_SHOW_HIFB failed!\n");
        munmap(pShowScreen, fix.smem_len);
        return HI_NULL;
    }
    switch (pstInfo->ctrlkey) {
        case 0: {
            /*change color*/
            pHideScreen = pShowScreen + u32FixScreenStride * var.yres;
            memset(pHideScreen, 0x8000, u32FixScreenStride * var.yres);
            memset(pShowScreen, 0x7c00, u32FixScreenStride * var.yres);
            for (i = 0; i < SAMPLE_IMAGE_NUM; i++) //IMAGE_NUM
            {

                if (i % 2) {
                    var.yoffset = 0;
                } else {
                    var.yoffset = var.yres;
                }

                if (ioctl(pstInfo->fd, FBIOPAN_DISPLAY, &var) < 0) {
                    printf("FBIOPAN_DISPLAY failed!\n");
                    munmap(pShowScreen, fix.smem_len);
                    close(pstInfo->fd);
                    return HI_NULL;
                }

                usleep(1000 * 1000);
            }
        }
            break;

        case 1: {
            /*move*/
            HI_U32 u32PosXtemp;
            u32PosXtemp = stPoint.s32XPos;

            for (i = 0; i < 400; i++) {
                if (i > 200) {
                    stPoint.s32XPos = u32PosXtemp + i % 20;
                    stPoint.s32YPos--;
                } else {
                    stPoint.s32XPos = u32PosXtemp - i % 20;
                    stPoint.s32YPos++;
                }
                if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0) {
                    printf("set screen original show position failed!\n");
                    munmap(pShowScreen, fix.smem_len);
                    close(pstInfo->fd);
                    return HI_NULL;
                }

                usleep(70 * 1000);
            }
        }
            break;

        case 2: {
            /*change bmp*/
            pHideScreen = pShowScreen + u32FixScreenStride * SAMPLE_IMAGE_HEIGHT;
            memset(pShowScreen, 0, u32FixScreenStride * SAMPLE_IMAGE_HEIGHT * 2);
            for (i = 0; i < SAMPLE_IMAGE_NUM; i++) {
                stPoint.s32XPos = stPoint.s32XPos + 4;
                stPoint.s32YPos = stPoint.s32YPos - 4;
                if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0) {
                    printf("set screen original show position failed!\n");
                    munmap(pShowScreen, fix.smem_len);
                    close(pstInfo->fd);
                    return HI_NULL;
                }
                sprintf(image_name, SAMPLE_IMAGE_PATH, i % 2);
                pDst = (HI_U8 *) pHideScreen;
                SAMPLE_HIFB_LoadBmp(image_name, pDst);
                if (i % 2) {
                    var.yoffset = 0;
                    pHideScreen = pShowScreen + u32FixScreenStride * SAMPLE_IMAGE_HEIGHT;
                } else {
                    var.yoffset = SAMPLE_IMAGE_HEIGHT;
                    pHideScreen = pShowScreen;
                }
                if (ioctl(pstInfo->fd, FBIOPAN_DISPLAY, &var) < 0) {
                    printf("FBIOPAN_DISPLAY failed!\n");
                    munmap(pShowScreen, fix.smem_len);
                    close(pstInfo->fd);
                    return HI_NULL;
                }
                usleep(500 * 1000);
            }
        }
            break;

        case 3: {
            /* move cursor */
            SAMPLE_HIFB_LoadBmp(SAMPLE_CURSOR_PATH, pShowScreen);
            if (ioctl(pstInfo->fd, FBIOPAN_DISPLAY, &var) < 0) {
                printf("FBIOPAN_DISPLAY failed!\n");
                munmap(pShowScreen, fix.smem_len);
                close(pstInfo->fd);
                return (void *) HI_FALSE;
            }
            printf("show cursor\n");
            sleep(2);
            if (1) {
                fd = pstInfo->fd;
                return HI_NULL;
            }
            for (i = 0; i < 100; i++) {
                stPoint.s32XPos += 2;
                stPoint.s32YPos += 2;
                if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0) {
                    printf("set screen original show position failed!\n");
                    munmap(pShowScreen, fix.smem_len);
                    close(pstInfo->fd);
                    return (void *) HI_FALSE;
                }
                usleep(70 * 1000);
            }
            for (i = 0; i < 100; i++) {
                stPoint.s32XPos += 2;
                stPoint.s32YPos -= 2;
                if (ioctl(pstInfo->fd, FBIOPUT_SCREEN_ORIGIN_HIFB, &stPoint) < 0) {
                    printf("set screen original show position failed!\n");
                    munmap(pShowScreen, fix.smem_len);
                    close(pstInfo->fd);
                    return HI_NULL;
                }
                usleep(70 * 1000);
            }
            printf("move the cursor\n");
            sleep(1);
            HI_MPI_VO_GfxLayerUnBindDev(GRAPHICS_LAYER_HC0, VoDev);
        }
            break;
        default: {
        }
    }

    /* unmap the physical memory */
    munmap(pShowScreen, fix.smem_len);

    bShow = HI_FALSE;
    if (ioctl(pstInfo->fd, FBIOPUT_SHOW_HIFB, &bShow) < 0) {
        printf("FBIOPUT_SHOW_HIFB failed!\n");
        return HI_NULL;
    }
    close(pstInfo->fd);
    return HI_NULL;
}


void init_mouse_layer() {
    PTHREAD_HIFB_SAMPLE_INFO stInfo2;
    stInfo2.layer = 5;
    stInfo2.fd = -1;
    stInfo2.ctrlkey = 3;
    SAMPLE_HIFB_PTHREAD_RunHiFB((void *) (&stInfo2));
}


void init_mouse() {
    evdev_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);      /*Basic initialization*/
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    init_mouse_layer();
    indev_drv.read_cb = mouse_read;
/*Register the driver in LittlevGL and save the created input device object*/
    lv_indev_t *my_indev = lv_indev_drv_register(&indev_drv);

//    lv_obj_t *cursor_obj = lv_img_create(lv_scr_act(), NULL); /*Create an image for the cursor */
//    lv_img_set_src(cursor_obj, SYMBOL_GPS);                 /*For simlicity add a built in symbol not an image*/
//    lv_indev_set_cursor(my_indev, cursor_obj);
}


void my_flush_cb(struct _disp_drv_t *disp_dr, const lv_area_t *area, lv_color_t *color_p) {
    fbdev_flush(area->x1, area->y1, area->x2, area->y2, color_p, disp_dr);
}


void init_fb() {
    fbdev_init();
    /*Add a display the LittlevGL sing the frame buffer driver*/
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = my_flush_cb;      /*It flushes the internal graphical buffer to the frame buffer*/
    lv_disp_drv_register(&disp_drv);
}


void init_ui() {
    benchmark_create();
}


int main(void) {
    //初始化little_gl
    lv_init();

    //初始化fb
    init_fb();

    //初始化鼠标
    init_mouse();

    //设置界面
    init_ui();

    //ui线程
    while (1) {
        lv_tick_inc(5);
        lv_task_handler();
        usleep(5 * 1000);
    }

    return 0;
}


