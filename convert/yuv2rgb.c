
#include <convert_manager.h>
#include <stdlib.h>
#include "color.h"

static int isSupportYuv2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    //�����ʽһ����Ҫ�� YUYV
    if (iPixelFormatIn != V4L2_PIX_FMT_YUYV)
        return 0;
    
    // �����ʽһ��Ҫ��RGB565��RGB32
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }
    return 1;
}


//�� YUV422 ��ʽ ��ͼ�����ݣ�YUYV ��ʽ��ת���� RGB565 ��ʽ
//input_ptr��ָ�� YUYV ��ʽͼ�����ݵ����뻺������
//output_ptr��ָ�� RGB565 ��ʽͼ�����ݵ������������
//image_width �� image_height��ͼ��Ŀ�Ⱥ͸߶ȡ�
static unsigned int Pyuv422torgb565(unsigned char * input_ptr, unsigned char * output_ptr, 
                                    unsigned int image_width, unsigned int image_height)
{
	unsigned int i, size;
	unsigned char Y, Y1, U, V;//Y, Y1����ʾ�������ص����ȷ�����U, V����ʾɫ�ȷ���������һ�� UV ������ Y��
	unsigned char *buff = input_ptr;// ��ʱָ�룬�������� YUYV ���ݡ�
	unsigned char *output_pt = output_ptr;// ���������дָ�롣

    unsigned int r, g, b;//�ֱ�洢ÿ������ת����� RGB ֵ��
    unsigned int color;//���ڹ��� RGB565 ��ʽ�� 16 λ��ɫֵ��
    
    //YUYV ��ʽ��ÿ 4 �ֽ� ���������أ�Y0 U Y1 V��
	size = image_width * image_height /2;
	for (i = size; i > 0; i--) {
		/* bgr instead rgb ?? */
        //Y �ǵ�һ���������ȣ�Y1 �ǵڶ����������ȡ�
        //U �� V ���������ع����ɫ��ֵ��
        //buff += 4���������������ء�
		Y = buff[0] ;
		U = buff[1] ;
		Y1 = buff[2];
		V = buff[3];
		buff += 4;

        //��һ�����أ�Y��U��V �� RGB �� RGB565
		r = R_FROMYV(Y,V);
		g = G_FROMYUV(Y,U,V); //b
		b = B_FROMYU(Y,U); //v

        /* ��r,g,b��ɫ����Ϊrgb565��16λֵ */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;

         //д�������������С�ˣ�
        *output_pt++ = color & 0xff;// ���ֽ�
        *output_pt++ = (color >> 8) & 0xff;// ���ֽ�
			
        //�ڶ�������
		r = R_FROMYV(Y1,V);
		g = G_FROMYUV(Y1,U,V); //b
		b = B_FROMYU(Y1,U); //v
		
        /* ��r,g,b��ɫ����Ϊrgb565��16λֵ */
        r = r >> 3;
        g = g >> 2;
        b = b >> 3;
        color = (r << 11) | (g << 5) | b;
        //д�������������С�ˣ�
        *output_pt++ = color & 0xff;    // ���ֽ�
        *output_pt++ = (color >> 8) & 0xff; // ���ֽ�
	}
	
	return 0;
} 

//�� YUV422 ��ʽ ��ͼ�����ݣ�YUYV ��ʽ��ת���� RGB32 ��ʽ
//input_ptr��ָ�� YUYV ��ʽͼ�����ݵ����뻺������
//output_ptr��ָ�� RGB565 ��ʽͼ�����ݵ������������
//image_width �� image_height��ͼ��Ŀ�Ⱥ͸߶ȡ�
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

//�� YUYV (YUV422) ͼ���ʽת���� RGB565 �� RGB32 ��ʽ������������� ptVideoBufOut��
static int Yuv2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{
    //ȡ���������ͼ���е��������ݽṹָ�룬�����������
    PT_PixelDatas ptPixelDatasIn  = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    //���ͼ��Ŀ�͸�������ͼ��һ�¡�
    ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
    ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
    
    //���Ϊ RGB565
    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
    {
        //���ò���
        ptPixelDatasOut->iBpp = 16;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        //�����ڴ�
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        //��ʽת��
        Pyuv422torgb565(ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->aucPixelDatas, ptPixelDatasOut->iWidth, ptPixelDatasOut->iHeight);
        return 0;
    }
    //���Ϊ RGB32
    else if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
    {
        //���ò���
        ptPixelDatasOut->iBpp = 32;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;

        //�����ڴ�
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        
        //��ʽת��
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

/* ���� */
static T_VideoConvert g_tYuv2RgbConvert = {
    .name        = "yuv2rgb",
    .isSupport   = isSupportYuv2Rgb,
    .Convert     = Yuv2RgbConvert,
    .ConvertExit = Yuv2RgbConvertExit,
};

extern void initLut(void);

/* ע�� */
int Yuv2RgbInit(void)
{
    initLut();
    return RegisterVideoConvert(&g_tYuv2RgbConvert);
}

