#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <disp_manager.h>
#include <video_manager.h>
#include <convert_manager.h>
#include <render.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>


/* video2lcd </dev/video0,1,...> */
int main(int argc, char **argv)
{	
    int iError;                             // 用于存储函数调用的返回错误码

    T_VideoDevice tVideoDevice;             // 保存视频设备的结构体实例（如 /dev/video0）
    PT_VideoConvert ptVideoConvert;         // 指向格式转换操作结构体的指针（用于视频格式转换）
    int iPixelFormatOfVideo;                // 摄像头原始图像的像素格式
    int iPixelFormatOfDisp;                 // 显示设备（LCD）的像素格式

    PT_VideoBuf ptVideoBufCur;              // 当前要显示的视频缓冲区指针
    T_VideoBuf tVideoBuf;                   // 原始图像帧缓冲区（从摄像头读取的）
    T_VideoBuf tConvertBuf;                 // 格式转换后的图像缓冲区（转换为 LCD 支持的格式）
    T_VideoBuf tZoomBuf;                    // 缩放后的图像缓冲区（用于适配 LCD 分辨率）
    T_VideoBuf tFrameBuf;                   // LCD 显示所用的最终图像帧缓冲区

    int iLcdWidth;                          // LCD 的宽度（像素）
    int iLcdHeigt;                          // LCD 的高度（像素）
    int iLcdBpp;                            // LCD 每个像素的位数（Bits Per Pixel）

    int iTopLeftX;                          // 图像居中显示时左上角 X 坐标
    int iTopLeftY;                          // 图像居中显示时左上角 Y 坐标

    float k;                                // 图像的宽高比，用于缩放计算

    //判断参数
    if (argc != 2)
    {
        printf("Usage:\n");
        printf("%s </dev/video0,1,...>\n", argv[0]);
        return -1;
    }
    
    /* 一系列的初始化 */
	/* 注册显示设备 */
	DisplayInit();

	/* 可能可支持多个显示设备: 选择和初始化指定的显示设备 */
	SelectAndInitDefaultDispDev("fb");
    
    /* 获得当前显示设备的分辨率 */
    GetDispResolution(&iLcdWidth, &iLcdHeigt, &iLcdBpp);

    /* 获得显存 */
    GetVideoBufForDisplay(&tFrameBuf);

    /* 获得当前显示设备的显示格式 */
    iPixelFormatOfDisp = tFrameBuf.iPixelFormat;

    /* 初始化整个视频采集（摄像头）子系统 */
    VideoInit();

    /* 初始化具体的摄像头设备，比如 /dev/video0。 */
    iError = VideoDeviceInit(argv[1], &tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("VideoDeviceInit for %s error!\n", argv[1]);
        return -1;
    }

    /* 获取摄像头设备的格式 */
    iPixelFormatOfVideo = tVideoDevice.ptOPr->GetFormat(&tVideoDevice);

    /* 格式转化框架初始化 */
    VideoConvertInit();

    /* 获取指定输入\输出格式的转化器，比如MJPEG TO RGB565 */
    ptVideoConvert = GetVideoConvertForFormats(iPixelFormatOfVideo, iPixelFormatOfDisp);
    if (NULL == ptVideoConvert)
    {
        DBG_PRINTF("can not support this format convert\n");
        return -1;
    }

    /* 启动摄像头设备 */
    iError = tVideoDevice.ptOPr->StartDevice(&tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("StartDevice for %s error!\n", argv[1]);
        return -1;
    }

    /* 分配视频缓存 */
    memset(&tVideoBuf, 0, sizeof(tVideoBuf));

    /* 格式转化后的缓存 */
    memset(&tConvertBuf, 0, sizeof(tConvertBuf));
    tConvertBuf.iPixelFormat     = iPixelFormatOfDisp;
    tConvertBuf.tPixelDatas.iBpp = iLcdBpp;
    
    /* 缩放缓存 */
    memset(&tZoomBuf, 0, sizeof(tZoomBuf));
    
    while (1)
    {
        /* 读入摄像头数据 */
        iError = tVideoDevice.ptOPr->GetFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
        {
            DBG_PRINTF("GetFrame for %s error!\n", argv[1]);
            return -1;
        }
        ptVideoBufCur = &tVideoBuf;//赋值给当前帧缓存

        //如果摄像头格式和显示设备的格式不相等则开始转化
        if (iPixelFormatOfVideo != iPixelFormatOfDisp)
        {
            /* 转换为RGB */
            iError = ptVideoConvert->Convert(&tVideoBuf, &tConvertBuf);
            DBG_PRINTF("Convert %s, ret = %d\n", ptVideoConvert->name, iError);
            if (iError)
            {
                DBG_PRINTF("Convert for %s error!\n", argv[1]);
                return -1;
            }            
            ptVideoBufCur = &tConvertBuf;
        }
        
        /* 如果图像分辨率大于LCD, 缩放 */
        if (ptVideoBufCur->tPixelDatas.iWidth > iLcdWidth || ptVideoBufCur->tPixelDatas.iHeight > iLcdHeigt)
        {
            /* 确定缩放后的分辨率 */
            /* 把图片按比例缩放到VideoMem上, 居中显示
             * 1. 先算出缩放后的大小
             */
            //算图像的高宽比 k = 高 / 宽，用于后续缩放。
            k = (float)ptVideoBufCur->tPixelDatas.iHeight / ptVideoBufCur->tPixelDatas.iWidth;

            //假设：先试图把图像缩放成和 LCD 一样宽。
            //高度：用 LCD 宽度 × k 得到等比的高度。
            tZoomBuf.tPixelDatas.iWidth  = iLcdWidth;
            tZoomBuf.tPixelDatas.iHeight = iLcdWidth * k;

            //目的： 如果刚才的计算导致高度超出 LCD，那就反过来——按高度限制，重新计算宽度。
            //这样做的结果： 图像一定能在 LCD 上完整居中显示，不会拉伸变形。
            if ( tZoomBuf.tPixelDatas.iHeight > iLcdHeigt)
            {
                tZoomBuf.tPixelDatas.iWidth  = iLcdHeigt / k;
                tZoomBuf.tPixelDatas.iHeight = iLcdHeigt;
            }

            //准备缩放后的图像缓冲区参数：
            tZoomBuf.tPixelDatas.iBpp        = iLcdBpp;
            tZoomBuf.tPixelDatas.iLineBytes  = tZoomBuf.tPixelDatas.iWidth * tZoomBuf.tPixelDatas.iBpp / 8;
            tZoomBuf.tPixelDatas.iTotalBytes = tZoomBuf.tPixelDatas.iLineBytes * tZoomBuf.tPixelDatas.iHeight;

            //分配内存
            if (!tZoomBuf.tPixelDatas.aucPixelDatas)
            {
                tZoomBuf.tPixelDatas.aucPixelDatas = malloc(tZoomBuf.tPixelDatas.iTotalBytes);
            }
            
            PicZoom(&ptVideoBufCur->tPixelDatas, &tZoomBuf.tPixelDatas);
            ptVideoBufCur = &tZoomBuf;
        }

        /* 合并进framebuffer */
        /* 接着算出居中显示时左上角坐标 */
        iTopLeftX = (iLcdWidth - ptVideoBufCur->tPixelDatas.iWidth) / 2;
        iTopLeftY = (iLcdHeigt - ptVideoBufCur->tPixelDatas.iHeight) / 2;

        PicMerge(iTopLeftX, iTopLeftY, &ptVideoBufCur->tPixelDatas, &tFrameBuf.tPixelDatas);

        //framebuffer 缓冲的数据刷新到真正的设备
        FlushPixelDatasToDev(&tFrameBuf.tPixelDatas);

        //告诉摄像头驱动“这帧图像我用完了”
        iError = tVideoDevice.ptOPr->PutFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
        {
            DBG_PRINTF("PutFrame for %s error!\n", argv[1]);
            return -1;
        }                    

        /* 把framebuffer的数据刷到LCD上, 显示 */
    }
		
	return 0;
}

