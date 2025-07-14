
/* MJPEG : ʵ����ÿһ֡���ݶ���һ��������JPEG�ļ� */

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
	//�����ʽ������JPEG
    if (iPixelFormatIn != V4L2_PIX_FMT_MJPEG)
        return 0;
	
	//�����ʽ����ΪRGB565��RGB32
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}


/**********************************************************************
 * �������ƣ� MyErrorExit
 * ���������� �Զ����libjpeg���������
 *            Ĭ�ϵĴ����������ó����˳�,���ǵ�Ȼ����ʹ����
 *            �ο�libjpeg���bmp.c��д�������������
 * ��������� ptCInfo - libjpeg����������ͨ�ýṹ��
 * ��������� ��
 * �� �� ֵ�� ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
//��κ���������һ�� libjpeg ���Զ��������������ҪĿ������ JPEG ��������г��ִ���ʱ��
//��¼������Ϣ��ʹ�� longjmp ��ת�ذ�ȫ�㣬��ֹ���������
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
 * �������ƣ� CovertOneLine
 * ���������� ���Ѿ���JPG�ļ�ȡ����һ����������,ת��Ϊ������ʾ�豸��ʹ�õĸ�ʽ
 * ���������  iWidth      - ���,�����ٸ�����
 *            iSrcBpp     - �Ѿ���JPG�ļ�ȡ����һ��������������,һ�������ö���λ����ʾ
 *            iDstBpp     - ��ʾ�豸��һ�������ö���λ����ʾ
 *            pudSrcDatas - �Ѿ���JPG�ļ�ȡ����һ�����������洢��λ��
 *            pudDstDatas - ת���������ݴ洢��λ��
 * ��������� ��
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
//��һ�п��Ϊ iWidth �� 24bpp ͼ����������ת��Ϊ 16bpp / 24bpp / 32bpp ��ʽ������
static int CovertOneLine(int iWidth, int iSrcBpp, int iDstBpp, unsigned char *pudSrcDatas, unsigned char *pudDstDatas)
{
	unsigned int dwRed;
	unsigned int dwGreen;
	unsigned int dwBlue;
	unsigned int dwColor;

	unsigned short *pwDstDatas16bpp = (unsigned short *)pudDstDatas;//ת��ΪRGB565��ָ��
	unsigned int   *pwDstDatas32bpp = (unsigned int *)pudDstDatas;//ת��ΪRGB32��ָ��

	int i;
	int pos = 0;
	
	//������Ǵ� 24 λ RGB888 ��ʽת��������ֱ��ʧ�ܡ�
	if (iSrcBpp != 24)
	{
		return -1;
	}

	//Ŀ��Ҳ�� 24bpp
	if (iDstBpp == 24)
	{
		//Դ��Ŀ�궼�� 24bpp��ֱ���ڴ濽����
		memcpy(pudDstDatas, pudSrcDatas, iWidth*3);
	}
	else//Ŀ���� 32bpp �� 16bpp
	{
		for (i = 0; i < iWidth; i++)
		{
			//ÿ���ض�ȡ 3 �ֽڣ�RGB����ע��˳���� R-G-B
			dwRed   = pudSrcDatas[pos++];
			dwGreen = pudSrcDatas[pos++];
			dwBlue  = pudSrcDatas[pos++];
			
			if (iDstBpp == 32)// Ŀ��Ϊ 32bpp��RGB888 �� ARGB32��
			{
				dwColor = (dwRed << 16) | (dwGreen << 8) | dwBlue;
				*pwDstDatas32bpp = dwColor;
				pwDstDatas32bpp++;
			}
			else if (iDstBpp == 16)//Ŀ��Ϊ 16bpp��RGB888 �� RGB565��
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
 * �������ƣ� GetPixelDatasFrmJPG
 * ���������� ��JPG�ļ��е�ͼ������,ȡ����ת��Ϊ������ʾ�豸��ʹ�õĸ�ʽ
 * ��������� ptFileMap    - �ں��ļ���Ϣ
 * ��������� ptPixelDatas - �ں���������
 *            ptPixelDatas->iBpp ������Ĳ���, ��ȷ����JPG�ļ��õ�������Ҫת��Ϊ��BPP
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���          �޸�����
 * -----------------------------------------------
 * 2013/02/08        V1.0     Τ��ɽ          ����
 ***********************************************************************/
//static int GetPixelDatasFrmJPG(PT_FileMap ptFileMap, PT_PixelDatas ptPixelDatas)
/* ���ڴ����JPEGͼ��ת��ΪRGBͼ�� */
//��һ֡ MJPEG ��ʽ��ͼ�����ݣ�ͨ����������ͷ������Ϊ RGB ͼ�����ݣ�֧�� RGB565 �� RGB32����
//����䵽 ptVideoBufOut �С�
static int Mjpeg2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
	struct jpeg_decompress_struct tDInfo; //tDInfo��libjpeg ���ڽ��� JPEG �����ṹ��
	//struct jpeg_error_mgr tJErr;
    int iRet;
    int iRowStride;
    unsigned char *aucLineBuffer = NULL;
    unsigned char *pucDest;
	T_MyErrorMgr tJerr;
    PT_PixelDatas ptPixelDatas = &ptVideoBufOut->tPixelDatas;

	// ����ͳ�ʼ��һ��decompression�ṹ��
	//tDInfo.err = jpeg_std_error(&tJErr);

	//����ʹ�� setjmp/longjmp ���쳣��ת����
	//��� JPEG ���������ִ�� MyErrorExit ������������� setjmp λ��
	//����ʱ���ͷ���ԴȻ�󷵻� -1
	tDInfo.err               = jpeg_std_error(&tJerr.pub);
	tJerr.pub.error_exit     = MyErrorExit;

	if(setjmp(tJerr.setjmp_buffer))
	{
		/* ������������е�����, ��ʾJPEG������� */
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

	//����������
	jpeg_create_decompress(&tDInfo);

	// ��jpeg_read_header���jpg��Ϣ
	//jpeg_stdio_src(&tDInfo, ptFileMap->tFp);
	/* ��������Ϊ�ڴ��е����� */
    jpeg_mem_src_tj (&tDInfo, ptVideoBufIn->tPixelDatas.aucPixelDatas, ptVideoBufIn->tPixelDatas.iTotalBytes);
    

	//��ȡ JPEG �ļ�ͷ����ȡͼ���ߡ���ɫͨ������Ϣ
    iRet = jpeg_read_header(&tDInfo, TRUE);

	// ���ý�ѹ����,����Ŵ���С
	//�������Ų��������ﲻ���ţ�
    tDInfo.scale_num = tDInfo.scale_denom = 1;
    
	// ������ѹ��jpeg_start_decompress	
	//������ѹ����
	jpeg_start_decompress(&tDInfo);
    
	// һ�е����ݳ���
	iRowStride = tDInfo.output_width * tDInfo.output_components;
	//����һ�����ݵĻ���������ʱ�л��棩
	aucLineBuffer = malloc(iRowStride);

    if (NULL == aucLineBuffer)
    {
        return -1;
    }

	//��д������ͼ���ߡ�ÿ���ֽ��������ֽ���
	ptPixelDatas->iWidth  = tDInfo.output_width;
	ptPixelDatas->iHeight = tDInfo.output_height;
	//ptPixelDatas->iBpp    = iBpp;
	ptPixelDatas->iLineBytes    = ptPixelDatas->iWidth * ptPixelDatas->iBpp / 8;
    ptPixelDatas->iTotalBytes   = ptPixelDatas->iHeight * ptPixelDatas->iLineBytes;
	//�������������
	if (NULL == ptPixelDatas->aucPixelDatas)
	{
	    ptPixelDatas->aucPixelDatas = malloc(ptPixelDatas->iTotalBytes);
	}

    pucDest = ptPixelDatas->aucPixelDatas;

	// ѭ������jpeg_read_scanlines��һ��һ�еػ�ý�ѹ������
	while (tDInfo.output_scanline < tDInfo.output_height) 
	{
        /* �õ�һ������,�������ɫ��ʽΪ0xRR, 0xGG, 0xBB */
		(void) jpeg_read_scanlines(&tDInfo, &aucLineBuffer, 1);

		// ת��ptPixelDatasȥ
		//ÿ������ת��ΪĿ���ʽ
		CovertOneLine(ptPixelDatas->iWidth, 24, ptPixelDatas->iBpp, aucLineBuffer, pucDest);
		pucDest += ptPixelDatas->iLineBytes;
	}
	
	//������ɣ��ͷ���Դ
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

/* ���� */
static T_VideoConvert g_tMjpeg2RgbConvert = {
    .name        = "mjpeg2rgb",
    .isSupport   = isSupportMjpeg2Rgb,
    .Convert     = Mjpeg2RgbConvert,
    .ConvertExit = Mjpeg2RgbConvertExit,
};


/* ע�� */
int Mjpeg2RgbInit(void)
{
    return RegisterVideoConvert(&g_tMjpeg2RgbConvert);
}


