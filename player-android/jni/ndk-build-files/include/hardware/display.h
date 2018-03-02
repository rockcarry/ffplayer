/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_DISPLAY_INTERFACE_H
#define ANDROID_DISPLAY_INTERFACE_H

#include <hardware/hardware.h>

#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/types.h>

__BEGIN_DECLS

/**
 * The id of this module
 */
#define DISPLAY_HARDWARE_MODULE_ID "display"

/**
 * Name of the graphics device to open
 */
#define DISPLAY_HARDWARE_DISPLAY0 "display0"


enum
{
    DISPLAY_TVDAC_NONE              = 0,
    DISPLAY_TVDAC_YPBPR             = 1,
    DISPLAY_TVDAC_CVBS              = 2,
    DISPLAY_TVDAC_SVIDEO            = 3,
    DISPLAY_TVDAC_ALL               = 4,
};

enum
{
    /*output type definition*/
    DISPLAY_DEVICE_NONE             = 0,
	DISPLAY_DEVICE_LCD				= 1,
	DISPLAY_DEVICE_TV				= 2,
	DISPLAY_DEVICE_HDMI				= 3,
	DISPLAY_DEVICE_VGA				= 4,
};


enum
{
	DISPLAY_MODE_SINGLE				= 0,
	DISPLAY_MODE_DUALLCD			= 1,
	DISPLAY_MODE_DUALDIFF			= 2,
	DISPLAY_MODE_DUALSAME			= 3,
	DISPLAY_MODE_DUALSAME_TWO_VIDEO	= 4,
	DISPLAY_MODE_SINGLE_VAR_FE      = 5,
	DISPLAY_MODE_SINGLE_VAR_BE      = 6,
	DISPLAY_MODE_SINGLE_FB_VAR      = 7,
	DISPLAY_MODE_SINGLE_VAR_GPU     = 8,
};

enum
{
    DISPLAY_TVFORMAT_480I                = 0,
    DISPLAY_TVFORMAT_576I                = 1,
    DISPLAY_TVFORMAT_480P                = 2,
    DISPLAY_TVFORMAT_576P                = 3,
    DISPLAY_TVFORMAT_720P_50HZ           = 4,
    DISPLAY_TVFORMAT_720P_60HZ           = 5,
    DISPLAY_TVFORMAT_1080I_50HZ          = 6,
    DISPLAY_TVFORMAT_1080I_60HZ          = 7,
    DISPLAY_TVFORMAT_1080P_24HZ          = 8,
    DISPLAY_TVFORMAT_1080P_50HZ          = 9,
    DISPLAY_TVFORMAT_1080P_60HZ          = 0xa,
    DISPLAY_TVFORMAT_PAL                 = 0xb,
    DISPLAY_TVFORMAT_PAL_SVIDEO          = 0xc,
    DISPLAY_TVFORMAT_PAL_CVBS_SVIDEO     = 0xd,
    DISPLAY_TVFORMAT_NTSC                = 0xe,
    DISPLAY_TVFORMAT_NTSC_SVIDEO         = 0xf,
    DISPLAY_TVFORMAT_NTSC_CVBS_SVIDEO    = 0x10,
    DISPLAY_TVFORMAT_PAL_M               = 0x11,
    DISPLAY_TVFORMAT_PAL_M_SVIDEO        = 0x12,
    DISPLAY_TVFORMAT_PAL_M_CVBS_SVIDEO   = 0x13,
    DISPLAY_TVFORMAT_PAL_NC              = 0x14,
    DISPLAY_TVFORMAT_PAL_NC_SVIDEO       = 0x15,
    DISPLAY_TVFORMAT_PAL_NC_CVBS_SVIDEO  = 0x16,
    DISPLAY_VGA_H1680_V1050    			 = 0x17,
    DISPLAY_VGA_H1440_V900     			 = 0x18,
    DISPLAY_VGA_H1360_V768     			 = 0x19,
    DISPLAY_VGA_H1280_V1024    			 = 0x1a,
    DISPLAY_VGA_H1024_V768     			 = 0x1b,
    DISPLAY_VGA_H800_V600      			 = 0x1c,
    DISPLAY_VGA_H640_V480      			 = 0x1d,
    DISPLAY_VGA_H1440_V900_RB  			 = 0x1e,//not support yet
    DISPLAY_VGA_H1680_V1050_RB 			 = 0x1f,//not support yet
    DISPLAY_VGA_H1920_V1080_RB 			 = 0x20,
    DISPLAY_VGA_H1920_V1080    			 = 0x21,
    DISPLAY_VGA_H1280_V720     			 = 0x22,

    DISPLAY_TVFORMAT_1080P_25HZ          = 0x23,
    DISPLAY_TVFORMAT_1080P_30HZ          = 0x24,
    DISPLAY_TVFORMAT_1080P_24HZ_3D_FP    = 0x25,
    DISPLAY_TVFORMAT_720P_50HZ_3D_FP     = 0x26,
    DISPLAY_TVFORMAT_720P_60HZ_3D_FP     = 0x27,

    DISPLAY_DEFAULT                      = 0xFF
};

enum
{
	DISPLAY_OUTPUT_WIDTH			= 0,
	DISPLAY_OUTPUT_HEIGHT			= 1,
	DISPLAY_FBWIDTH                 = 2,
	DISPLAY_FBHEIGHT                = 3,
	DISPLAY_OUTPUT_PIXELFORMAT      = 4,
	DISPLAY_OUTPUT_FORMAT			= 5,
	DISPLAY_OUTPUT_TYPE				= 6,
	DISPLAY_OUTPUT_ISOPEN           = 7,
	DISPLAY_OUTPUT_HOTPLUG          = 8,
	DISPLAY_APP_WIDTH               = 9,
	DISPLAY_APP_HEIGHT              = 10,
	DISPLAY_VALID_WIDTH            = 11,
	DISPLAY_VALID_HEIGHT           = 12
};


enum
{
	DISPLAY_FORMAT_ARGB8888 = 0,
	DISPLAY_FORMAT_PYUV420UVC = 1,
};


struct display_rect_t
{
    uint32_t            x;
    uint32_t            y;
    uint32_t            width;
    uint32_t            height;
};

struct display_modepara_t
{
    int                 d0type;
    int                 d1type;
    int                 d0format;
    int                 d1format;
    int                 d0pixelformat;
    int                 d1pixelformat;
    int 			    masterdisplay;
};

/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 */
struct display_module_t
{
    struct hw_module_t common;
};

/**
 * Every device data structure must begin with hw_device_t
 * followed by module specific public methods and attributes.
 */
struct display_device_t
{
    struct hw_device_t common;

    /**
     * Set a display parameter.
     *
     * @param dev from open
     * @param name one for the DISPLAY_NAME_xxx
     * @param value one of the DISPLAY_VALUE_xxx
     *
     * @return 0 if successful
     */
    int (*changemode)			(struct display_device_t *dev, int displayno, int value0,int value1);

	int (*setdisplayparameter)	(struct display_device_t *dev, int displayno, int value0,int value1);

	int (*setdisplaymode)		(struct display_device_t *dev, int mode,struct display_modepara_t *para);

	int (*opendisplay)			(struct display_device_t *dev, int displayno);

	int (*closedisplay)			(struct display_device_t *dev, int displayno);

    int (*gethdmimaxmode)       (struct display_device_t *dev);

    int (*gethdmistatus)        (struct display_device_t *dev);

    int (*gettvdacstatus)       (struct display_device_t *dev);

    int (*request_modelock)     (struct display_device_t *dev);

    int (*release_modelock)     (struct display_device_t *dev);

    /*copy master fb to slave fb,dstbufno is the dst buf index*/
    int (*copysrcfbtodstfb)     (struct display_device_t *dev,int src_fbid,int src_bufno,int dst_fbid,int dst_bufno);

    int (*pandisplay)           (struct display_device_t *dev,int fb_id,int bufno);
    /**
     * Get a static display information.
     *
     * @param dev from open
     * @param name one of the DISPLAY_STATIC_xxx
     *
     * @return value or -EINVAL if error
     */
    int (*getdisplayparameter)  (struct display_device_t *dev, int displayno, int param);

    int (*setmasterdisplay)     (struct display_device_t *dev, int displayno);

    int (*getmasterdisplay)     (struct display_device_t *dev);

    int (*getmaxwidthdisplay)   (struct display_device_t *dev);

    /*获取当前屏幕正在显示的BUF ID*/
    int (*getdisplaybufid)      (struct display_device_t *dev, int displayno);

    /*获取当前显示设备模式*/
    int (*getdisplaymode)      	(struct display_device_t *dev);

    /*设置当前显示区域大小百分比*/
    int (*setdisplayareapercent)(struct display_device_t *dev, int displayno,int percent);

    /*获取当前显示区域大小百分比*/
    int (*getdisplayareapercent)(struct display_device_t *dev, int displayno);

    int (*setdisplaybrightness)	(struct display_device_t *dev, int displayno,int bright);
    int (*getdisplaybrightness)	(struct display_device_t *dev, int displayno);
    int (*setdisplaycontrast)	(struct display_device_t *dev, int displayno,int contrast);
    int (*getdisplaycontrast)	(struct display_device_t *dev, int displayno);
    int (*setdisplaysaturation)	(struct display_device_t *dev, int displayno,int saturation);
    int (*getdisplaysaturation)	(struct display_device_t *dev, int displayno);
    int (*setdisplayhue)	    (struct display_device_t *dev, int displayno,int hue);
    int (*getdisplayhue)	    (struct display_device_t *dev, int displayno);

    /*判断某种hdmi制式是否被电视支持*/
    int (*issupporthdmimode)	(struct display_device_t *dev,int mode);

    /*判断电视是否支持3D输出*/
    int (*issupport3dmode)		(struct display_device_t *dev);

    int (*getdisplaycount)	    (struct display_device_t *dev);
	int (*setdisplaybacklightmode)(struct display_device_t *dev, int mode);

    /*设置硬件光标位置*/
    int (*sethwcursorpos)       (struct display_device_t *dev,int displayno,int posx,int posy);

    /*获取硬件光标X位置*/
    int (*gethwcursorposx)      (struct display_device_t *dev,int displayno);

    /*获取硬件光标Y位置*/
    int (*gethwcursorposy)      (struct display_device_t *dev,int displayno);

    /*显示硬件光标*/
    int (*hwcursorshow)         (struct display_device_t *dev,int displayno);

    /*隐藏硬件光标*/
    int (*hwcursorhide)         (struct display_device_t *dev,int displayno);

    /*获取硬件光标用户空间地址*/
    int (*hwcursorgetvaddr)     (struct display_device_t *dev,int displayno);

    /*获取硬件光标用户空间物理地址*/
    int (*hwcursorgetpaddr)     (struct display_device_t *dev,int displayno);

    /*初始化硬件光标*/
    int (*hwcursorinit)         (struct display_device_t *dev,int displayno);

    int (*setOrientation)       (struct display_device_t *dev,int orientation);

    /*申请display buffer*/
    int (*requestdispbuf)		(struct display_device_t *dev,int width,int height,int format,int buf_num);

    /*释放display buffer*/
    int (*releasedispbuf)		(struct display_device_t *dev,int buf_hdl);

    /*转化framebuffer的内容到指定的display buffer*/
    int (*convertfb)			(struct display_device_t *dev,int srcfb_id,int srcfb_bufno,int dst_buf_hdl, int dst_bufno,int width,int height,int format);

    /*获取display buffer的地址*/
    int (*getdispbufaddr)		(struct display_device_t *dev,int buf_hdl,int bufno,int width,int height,int format);
};


/** convenience API for opening and closing a device */

static inline int display_open(const struct hw_module_t* module,
        struct display_device_t** device)
{
    return module->methods->open(module,
            DISPLAY_HARDWARE_DISPLAY0, (struct hw_device_t**)device);
}

static inline int display_close(struct display_device_t* device)
{
    return device->common.close(&device->common);
}


__END_DECLS

#endif  // ANDROID_DISPLAY_INTERFACE_H

