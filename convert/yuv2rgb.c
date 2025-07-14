
#include <convert_manager.h>
#include <stdlib.h>
#include "color.h"

static int isSupportYuv2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    //输入格式一定需要是 YUYV
    if (iPixelFormatIn != V4L2_PIX_FMT_YUYV)
        return 0;
    
    // 输出格式一定要是RGB565和RGB32
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}


//将 YUV422 格式 的图像数据（YUYV 格式）转换成 RGB565 格式
//input_ptr：指向 YUYV 格式图像数据的输入缓冲区。
//output_ptr：指向 RGB565 格式图像数据的输出缓冲区。
//image_width 和 image_height：图像的宽度和高度。
static unsigned int Pyuv422torgb565(unsigned char * input_ptr, unsigned char * output_ptr, 
                                    unsigned int image_width, unsigned int image_height)
{
	unsigned int i, size;
	unsigned char Y, Y1, U, V;//Y, Y1：表示两个像素的亮度分量。U, V：表示色度分量，共享一对 UV 对两个 Y。
	unsigned char *buff = input_ptr;// 临时指针，遍历输入 YUYV 数据。
	unsigned char *output_pt = output_ptr;// 输出缓冲区写指针。

    unsigned int r, g, b;//分别存储每个像素转换后的 RGB 值。
    unsigned int color;//用于构造 RGB565 格式的 16 位颜色值。
    
    //YUYV 格式：每 4 字节 存两个像素（Y0 U Y1 V）
	size = image_width * image_height /2;
	for (i = size; i > 0; i--) {
		/* bgr instead rgb ?? */
        //Y 是第一个像素亮度，Y1 是第二个像素亮度。
        //U 和 V 是两个像素共享的色度值。
        //buff += 4：跳到下两个像素。
		Y = buff[0] ;
		U = buff[1] ;
		Y1 = buff[2];
		V = buff[3];
		buff += 4;

        //第一个像素：Y、U、V → RGB → RGB565
		r = R_FROMYV(Y,V);
		g = G_FROMYUV(Y,U,V); //b
		b = B_FROMYU(Y,U); //v

        /* 把r,g,b三色构造为rgb565的16位值 */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;

         //写入输出缓冲区（小端）
        *output_pt++ = color & 0xff;// 低字节
        *output_pt++ = (color >> 8) & 0xff;// 高字节
			
        //第二个像素
		r = R_FROMYV(Y1,V);
		g = G_FROMYUV(Y1,U,V); //b
		b = B_FROMYU(Y1,U); //v
		
        /* 把r,g,b三色构造为rgb565的16位值 */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;
        //写入输出缓冲区（小端）
        *output_pt++ = color & 0xff;    // 低字节
        *output_pt++ = (color >> 8) & 0xff; // 高字节
	}
	
	return 0;
} 

//将 YUV422 格式 的图像数据（YUYV 格式）转换成 RGB32 格式
//input_ptr：指向 YUYV 格式图像数据的输入缓冲区。
//output_ptr：指向 RGB565 格式图像数据的输出缓冲区。
//image_width 和 image_height：图像的宽度和高度。
static unsigned int Pyuv422torgb32(unsigned char * input_ptr, unsigned char * output_ptr, 
                                    unsigned int image_width, unsigned int image_height)
{
	unsigned int i, size;
	unsigned char Y, Y1, U, V;
	unsigned char *buff = input_ptr;
	unsigned int *output_pt = (unsigned int *)output_ptr;

    unsigned int r, g, b;
    unsigned int color;

	size = image_width * image_height /2;
	for (i = size; i > 0; i--) {
		/* bgr instead rgb ?? */
		Y = buff[0] ;
		U = buff[1] ;
		Y1 = buff[2];
		V = buff[3];
		buff += 4;

        r = R_FROMYV(Y,V);
		g = G_FROMYUV(Y,U,V); //b
		b = B_FROMYU(Y,U); //v
		/* rgb888 */
		color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;
			
		r = R_FROMYV(Y1,V);
		g = G_FROMYUV(Y1,U,V); //b
		b = B_FROMYU(Y1,U); //v
		color = (r << 16) | (g << 8) | b;
        *output_pt++ = color;
	}
	
	return 0;
} 

//将 YUYV (YUV422) 图像格式转换成 RGB565 或 RGB32 格式，并将结果存入 ptVideoBufOut。
static int Yuv2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
    //取出输入输出图像中的像素数据结构指针，方便后续访问
    PT_PixelDatas ptPixelDatasIn  = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    //输出图像的宽和高与输入图像一致。
    ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
    ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
    
    //输出为 RGB565
    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
    {
        //设置参数
        ptPixelDatasOut->iBpp = 16;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        //分配内存
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        //格式转化
        Pyuv422torgb565(ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->aucPixelDatas, ptPixelDatasOut->iWidth, ptPixelDatasOut->iHeight);
        return 0;
    }
    //输出为 RGB32
    else if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
    {
        //设置参数
        ptPixelDatasOut->iBpp = 32;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        //分配内存
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        
        //格式转化
        Pyuv422torgb32(ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->aucPixelDatas, ptPixelDatasOut->iWidth, ptPixelDatasOut->iHeight);
        return 0;
    }
    
    return -1;
}

static int Yuv2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas)
    {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0;
}

/* 构造 */
static T_VideoConvert g_tYuv2RgbConvert = {
    .name        = "yuv2rgb",
    .isSupport   = isSupportYuv2Rgb,
    .Convert     = Yuv2RgbConvert,
    .ConvertExit = Yuv2RgbConvertExit,
};

extern void initLut(void);

/* 注册 */
int Yuv2RgbInit(void)
{
    initLut();
    return RegisterVideoConvert(&g_tYuv2RgbConvert);
}

