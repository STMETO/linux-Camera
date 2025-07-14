
/* MJPEG : 实质上每一帧数据都是一个完整的JPEG文件 */

#include <convert_manager.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>

typedef struct MyErrorMgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
}T_MyErrorMgr, *PT_MyErrorMgr;

extern void jpeg_mem_src_tj(j_decompress_ptr, unsigned char *, unsigned long);

static int isSupportMjpeg2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
	//输入格式必须是JPEG
    if (iPixelFormatIn != V4L2_PIX_FMT_MJPEG)
        return 0;
	
	//输出格式必须为RGB565、RGB32
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}


/**********************************************************************
 * 函数名称： MyErrorExit
 * 功能描述： 自定义的libjpeg库出错处理函数
 *            默认的错误处理函数是让程序退出,我们当然不会使用它
 *            参考libjpeg里的bmp.c编写了这个错误处理函数
 * 输入参数： ptCInfo - libjpeg库抽象出来的通用结构体
 * 输出参数： 无
 * 返 回 值： 无
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  韦东山	      创建
 ***********************************************************************/
//这段函数代码是一个 libjpeg 的自定义错误处理函数，主要目的是在 JPEG 解码过程中出现错误时，
//记录错误信息并使用 longjmp 跳转回安全点，防止程序崩溃。
static void MyErrorExit(j_common_ptr ptCInfo)
{
    static char errStr[JMSG_LENGTH_MAX];
    
	PT_MyErrorMgr ptMyErr = (PT_MyErrorMgr)ptCInfo->err;

    /* Create the message */
    (*ptCInfo->err->format_message) (ptCInfo, errStr);
    DBG_PRINTF("%s\n", errStr);

	longjmp(ptMyErr->setjmp_buffer, 1);
}

/**********************************************************************
 * 函数名称： CovertOneLine
 * 功能描述： 把已经从JPG文件取出的一行象素数据,转换为能在显示设备上使用的格式
 * 输入参数：  iWidth      - 宽度,即多少个象素
 *            iSrcBpp     - 已经从JPG文件取出的一行象素数据里面,一个象素用多少位来表示
 *            iDstBpp     - 显示设备上一个象素用多少位来表示
 *            pudSrcDatas - 已经从JPG文件取出的一行象素数所存储的位置
 *            pudDstDatas - 转换所得数据存储的位置
 * 输出参数： 无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 * 修改日期        版本号     修改人	      修改内容
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  韦东山	      创建
 ***********************************************************************/
//将一行宽度为 iWidth 的 24bpp 图像像素数据转换为 16bpp / 24bpp / 32bpp 格式的数据
static int CovertOneLine(int iWidth, int iSrcBpp, int iDstBpp, unsigned char *pudSrcDatas, unsigned char *pudDstDatas)
{
	unsigned int dwRed;
	unsigned int dwGreen;
	unsigned int dwBlue;
	unsigned int dwColor;

	unsigned short *pwDstDatas16bpp = (unsigned short *)pudDstDatas;//转化为RGB565的指针
	unsigned int   *pwDstDatas32bpp = (unsigned int *)pudDstDatas;//转化为RGB32的指针

	int i;
	int pos = 0;
	
	//如果不是从 24 位 RGB888 格式转换，函数直接失败。
	if (iSrcBpp != 24)
	{
		return -1;
	}

	//目标也是 24bpp
	if (iDstBpp == 24)
	{
		//源和目标都是 24bpp，直接内存拷贝。
		memcpy(pudDstDatas, pudSrcDatas, iWidth*3);
	}
	else//目标是 32bpp 或 16bpp
	{
		for (i = 0; i < iWidth; i++)
		{
			//每像素读取 3 字节（RGB），注意顺序是 R-G-B
			dwRed   = pudSrcDatas[pos++];
			dwGreen = pudSrcDatas[pos++];
			dwBlue  = pudSrcDatas[pos++];
			
			if (iDstBpp == 32)// 目标为 32bpp（RGB888 → ARGB32）
			{
				dwColor = (dwRed << 16) | (dwGreen << 8) | dwBlue;
				*pwDstDatas32bpp = dwColor;
				pwDstDatas32bpp++;
			}
			else if (iDstBpp == 16)//目标为 16bpp（RGB888 → RGB565）
			{
				/* 565 */
				dwRed   = dwRed >> 3;
				dwGreen = dwGreen >> 2;
				dwBlue  = dwBlue >> 3;
				dwColor = (dwRed << 11) | (dwGreen << 5) | (dwBlue);
				*pwDstDatas16bpp = dwColor;
				pwDstDatas16bpp++;
			}
		}
	}
	return 0;
}


/**********************************************************************
 * 函数名称： GetPixelDatasFrmJPG
 * 功能描述： 把JPG文件中的图像数据,取出并转换为能在显示设备上使用的格式
 * 输入参数： ptFileMap    - 内含文件信息
 * 输出参数： ptPixelDatas - 内含象素数据
 *            ptPixelDatas->iBpp 是输入的参数, 它确定从JPG文件得到的数据要转换为该BPP
 * 返 回 值： 0 - 成功, 其他值 - 失败
 * 修改日期        版本号     修改人          修改内容
 * -----------------------------------------------
 * 2013/02/08        V1.0     韦东山          创建
 ***********************************************************************/
//static int GetPixelDatasFrmJPG(PT_FileMap ptFileMap, PT_PixelDatas ptPixelDatas)
/* 把内存里的JPEG图像转换为RGB图像 */
//将一帧 MJPEG 格式的图像数据（通常来自摄像头）解码为 RGB 图像数据（支持 RGB565 或 RGB32），
//并填充到 ptVideoBufOut 中。
static int Mjpeg2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
	struct jpeg_decompress_struct tDInfo; //tDInfo：libjpeg 用于解码 JPEG 的主结构体
	//struct jpeg_error_mgr tJErr;
    int iRet;
    int iRowStride;
    unsigned char *aucLineBuffer = NULL;
    unsigned char *pucDest;
	T_MyErrorMgr tJerr;
    PT_PixelDatas ptPixelDatas = &ptVideoBufOut->tPixelDatas;

	// 分配和初始化一个decompression结构体
	//tDInfo.err = jpeg_std_error(&tJErr);

	//这里使用 setjmp/longjmp 做异常跳转处理
	//如果 JPEG 解码出错，会执行 MyErrorExit 函数，跳回这个 setjmp 位置
	//出错时会释放资源然后返回 -1
	tDInfo.err               = jpeg_std_error(&tJerr.pub);
	tJerr.pub.error_exit     = MyErrorExit;

	if(setjmp(tJerr.setjmp_buffer))
	{
		/* 如果程序能运行到这里, 表示JPEG解码出错 */
        jpeg_destroy_decompress(&tDInfo);
        if (aucLineBuffer)
        {
            free(aucLineBuffer);
        }
        if (ptPixelDatas->aucPixelDatas)
        {
            free(ptPixelDatas->aucPixelDatas);
        }
		return -1;
	}

	//创建解码器
	jpeg_create_decompress(&tDInfo);

	// 用jpeg_read_header获得jpg信息
	//jpeg_stdio_src(&tDInfo, ptFileMap->tFp);
	/* 把数据设为内存中的数据 */
    jpeg_mem_src_tj (&tDInfo, ptVideoBufIn->tPixelDatas.aucPixelDatas, ptVideoBufIn->tPixelDatas.iTotalBytes);
    

	//读取 JPEG 文件头，提取图像宽高、颜色通道等信息
    iRet = jpeg_read_header(&tDInfo, TRUE);

	// 设置解压参数,比如放大、缩小
	//设置缩放参数（这里不缩放）
    tDInfo.scale_num = tDInfo.scale_denom = 1;
    
	// 启动解压：jpeg_start_decompress	
	//启动解压流程
	jpeg_start_decompress(&tDInfo);
    
	// 一行的数据长度
	iRowStride = tDInfo.output_width * tDInfo.output_components;
	//分配一行数据的缓冲区（临时行缓存）
	aucLineBuffer = malloc(iRowStride);

    if (NULL == aucLineBuffer)
    {
        return -1;
    }

	//填写解码后的图像宽高、每行字节数、总字节数
	ptPixelDatas->iWidth  = tDInfo.output_width;
	ptPixelDatas->iHeight = tDInfo.output_height;
	//ptPixelDatas->iBpp    = iBpp;
	ptPixelDatas->iLineBytes    = ptPixelDatas->iWidth * ptPixelDatas->iBpp / 8;
    ptPixelDatas->iTotalBytes   = ptPixelDatas->iHeight * ptPixelDatas->iLineBytes;
	//分配输出缓冲区
	if (NULL == ptPixelDatas->aucPixelDatas)
	{
	    ptPixelDatas->aucPixelDatas = malloc(ptPixelDatas->iTotalBytes);
	}

    pucDest = ptPixelDatas->aucPixelDatas;

	// 循环调用jpeg_read_scanlines来一行一行地获得解压的数据
	while (tDInfo.output_scanline < tDInfo.output_height) 
	{
        /* 得到一行数据,里面的颜色格式为0xRR, 0xGG, 0xBB */
		(void) jpeg_read_scanlines(&tDInfo, &aucLineBuffer, 1);

		// 转到ptPixelDatas去
		//每行数据转换为目标格式
		CovertOneLine(ptPixelDatas->iWidth, 24, ptPixelDatas->iBpp, aucLineBuffer, pucDest);
		pucDest += ptPixelDatas->iLineBytes;
	}
	
	//解码完成，释放资源
	free(aucLineBuffer);
	jpeg_finish_decompress(&tDInfo);
	jpeg_destroy_decompress(&tDInfo);

    return 0;
}


static int Mjpeg2RgbConvertExit(PT_VideoBuf ptVideoBufOut)
{
    if (ptVideoBufOut->tPixelDatas.aucPixelDatas)
    {
        free(ptVideoBufOut->tPixelDatas.aucPixelDatas);
        ptVideoBufOut->tPixelDatas.aucPixelDatas = NULL;
    }
    return 0;
}

/* 构造 */
static T_VideoConvert g_tMjpeg2RgbConvert = {
    .name        = "mjpeg2rgb",
    .isSupport   = isSupportMjpeg2Rgb,
    .Convert     = Mjpeg2RgbConvert,
    .ConvertExit = Mjpeg2RgbConvertExit,
};


/* 注册 */
int Mjpeg2RgbInit(void)
{
    return RegisterVideoConvert(&g_tMjpeg2RgbConvert);
}


