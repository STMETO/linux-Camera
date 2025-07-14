
#include <convert_manager.h>
#include <stdlib.h>
#include <string.h>

// 判断是否支持从 RGB565 转换为目标格式（RGB565 或 RGB32）
static int isSupportRgb2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    // 只支持输入格式为 RGB565
    if (iPixelFormatIn != V4L2_PIX_FMT_RGB565)
        return 0;

    // 只支持输出为 RGB565 或 RGB32
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }

    // 如果输入为RGB565，输出为RGB565或RGB32就返回1
    return 1;
}

//格式转化
static int Rgb2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{   
    // 输入/输出图像像素数据结构
    PT_PixelDatas ptPixelDatasIn  = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    int x, y;
    int r, g, b;
    int color;

     // 输入数据为 RGB565（每像素16位）
    unsigned short *pwSrc = (unsigned short *)ptPixelDatasIn->aucPixelDatas;
    unsigned int *pdwDest;

    if (ptVideoBufIn->iPixelFormat != V4L2_PIX_FMT_RGB565)
    {
        return -1;
    }

    // 如果输出格式也是 RGB565，直接复制数据
    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
    {
        ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp    = 16;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;
        // 分配内存（如尚未分配）
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        
        // 数据直接拷贝
        memcpy(ptPixelDatasOut->aucPixelDatas, ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->iTotalBytes);
        return 0;
    }
    // 如果输出格式是 RGB32（每像素32位，0x00RRGGBB 格式）
    else if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
    {
        ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp    = 32;// 每像素32位
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }

        // 把输出像素缓冲区的起始地址 aucPixelDatas 强制转换为 unsigned int * 类型，并赋值给 pdwDest。
        //方便RGB32数据（四字节）赋值
        pdwDest = (unsigned int *)ptPixelDatasOut->aucPixelDatas;
        
         // 像素转换：RGB565 -> RGB32
        for (y = 0; y < ptPixelDatasOut->iHeight; y++)
        {
            for (x = 0; x < ptPixelDatasOut->iWidth; x++)
            {
                color = *pwSrc++;   //pwSrc为unsigned short类型，指针自增跨越2字节
                /* 从RGB565格式的数据中提取出R,G,B */
                //RRRRR GGGGGG BBBBB
                r = color >> 11;
                g = (color >> 5) & (0x3f); //只取低6位
                b = color & 0x1f;   //只取低5位

                /* 把r,g,b转为0x00RRGGBB的32位数据 */
                //每一个颜色位扩充为一个字节
                color = ((r << 3) << 16) | ((g << 2) << 8) | (b << 3);

                *pdwDest = color;
                pdwDest++;
            }
        }
        return 0;
    }

    return -1;
}

static int Rgb2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas)
    {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0;
}

/* 构造 */
static T_VideoConvert g_tRgb2RgbConvert = {
    .name        = "rgb2rgb",
    .isSupport   = isSupportRgb2Rgb,
    .Convert     = Rgb2RgbConvert,
    .ConvertExit = Rgb2RgbConvertExit,
};


/* 注册 */
int Rgb2RgbInit(void)
{
    return RegisterVideoConvert(&g_tRgb2RgbConvert);
}



