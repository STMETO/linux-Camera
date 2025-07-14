
#ifndef _CONVERT_MANAGER_H
#define _CONVERT_MANAGER_H

#include <config.h>
#include <video_manager.h>
#include <linux/videodev2.h>

typedef struct VideoConvert {
    char *name;                    // ת�������ƣ���"YUV_TO_RGB"
    int (*isSupport)(int iPixelFormatIn, int iPixelFormatOut);  // �ж��Ƿ�֧�ָ�ʽת��
    int (*Convert)(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut);  // ִ��ת��
    int (*ConvertExit)(PT_VideoBuf ptVideoBufOut);  // ת������Դ����
    struct VideoConvert *ptNext;   // ����ָ����һ��ת����
} T_VideoConvert, *PT_VideoConvert;

// ��ȡ֧��ָ����ʽ��ת������ʧ�ܷ���NULL
PT_VideoConvert GetVideoConvertForFormats(int iPixelFormatIn, int iPixelFormatOut);

// ��ʼ����Ƶת��ϵͳ������0�ɹ�����0ʧ��
int VideoConvertInit(void);

// ��ʼ��YUV��RGBת��ģ��
int Yuv2RgbInit(void);

// ��ʼ��MJPEG��RGBת��ģ��
int Mjpeg2RgbInit(void);

// ��ʼ��RGB��ʽת��ģ�飨�粻ͬRGB����ת����
int Rgb2RgbInit(void);

// ע����Ƶת������ϵͳ������0�ɹ�����0ʧ��
int RegisterVideoConvert(PT_VideoConvert ptVideoConvert);

#endif /* _CONVERT_MANAGER_H */

