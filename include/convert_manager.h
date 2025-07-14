
#ifndef _CONVERT_MANAGER_H
#define _CONVERT_MANAGER_H

#include <config.h>
#include <video_manager.h>
#include <linux/videodev2.h>

typedef struct VideoConvert {
    char *name;                    // 转换器名称，如"YUV_TO_RGB"
    int (*isSupport)(int iPixelFormatIn, int iPixelFormatOut);  // 判断是否支持格式转换
    int (*Convert)(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut);  // 执行转换
    int (*ConvertExit)(PT_VideoBuf ptVideoBufOut);  // 转换后资源清理
    struct VideoConvert *ptNext;   // 链表指向下一个转换器
} T_VideoConvert, *PT_VideoConvert;

// 获取支持指定格式的转换器，失败返回NULL
PT_VideoConvert GetVideoConvertForFormats(int iPixelFormatIn, int iPixelFormatOut);

// 初始化视频转换系统，返回0成功，非0失败
int VideoConvertInit(void);

// 初始化YUV到RGB转换模块
int Yuv2RgbInit(void);

// 初始化MJPEG到RGB转换模块
int Mjpeg2RgbInit(void);

// 初始化RGB格式转换模块（如不同RGB布局转换）
int Rgb2RgbInit(void);

// 注册视频转换器到系统，返回0成功，非0失败
int RegisterVideoConvert(PT_VideoConvert ptVideoConvert);

#endif /* _CONVERT_MANAGER_H */

