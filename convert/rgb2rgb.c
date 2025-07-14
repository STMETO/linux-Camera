
#include <convert_manager.h>
#include <stdlib.h>
#include <string.h>

// �ж��Ƿ�֧�ִ� RGB565 ת��ΪĿ���ʽ��RGB565 �� RGB32��
static int isSupportRgb2Rgb(int iPixelFormatIn, int iPixelFormatOut)
{
    // ֻ֧�������ʽΪ RGB565
    if (iPixelFormatIn != V4L2_PIX_FMT_RGB565)
        return 0;

    // ֻ֧�����Ϊ RGB565 �� RGB32
    if ((iPixelFormatOut != V4L2_PIX_FMT_RGB565) && (iPixelFormatOut != V4L2_PIX_FMT_RGB32))
    {
        return 0;
    }

    // �������ΪRGB565�����ΪRGB565��RGB32�ͷ���1
    return 1;
}

//��ʽת��
static int Rgb2RgbConvert(PT_VideoBuf ptVideoBufIn, PT_VideoBuf ptVideoBufOut)
{   
    // ����/���ͼ���������ݽṹ
    PT_PixelDatas ptPixelDatasIn  = &ptVideoBufIn->tPixelDatas;
    PT_PixelDatas ptPixelDatasOut = &ptVideoBufOut->tPixelDatas;

    int x, y;
    int r, g, b;
    int color;

     // ��������Ϊ RGB565��ÿ����16λ��
    unsigned short *pwSrc = (unsigned short *)ptPixelDatasIn->aucPixelDatas;
    unsigned int *pdwDest;

    if (ptVideoBufIn->iPixelFormat != V4L2_PIX_FMT_RGB565)
    {
        return -1;
    }

    // ��������ʽҲ�� RGB565��ֱ�Ӹ�������
    if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB565)
    {
        ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp    = 16;
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;
        // �����ڴ棨����δ���䣩
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }
        
        // ����ֱ�ӿ���
        memcpy(ptPixelDatasOut->aucPixelDatas, ptPixelDatasIn->aucPixelDatas, ptPixelDatasOut->iTotalBytes);
        return 0;
    }
    // ��������ʽ�� RGB32��ÿ����32λ��0x00RRGGBB ��ʽ��
    else if (ptVideoBufOut->iPixelFormat == V4L2_PIX_FMT_RGB32)
    {
        ptPixelDatasOut->iWidth  = ptPixelDatasIn->iWidth;
        ptPixelDatasOut->iHeight = ptPixelDatasIn->iHeight;
        ptPixelDatasOut->iBpp    = 32;// ÿ����32λ
        ptPixelDatasOut->iLineBytes  = ptPixelDatasOut->iWidth * ptPixelDatasOut->iBpp / 8;
        ptPixelDatasOut->iTotalBytes = ptPixelDatasOut->iLineBytes * ptPixelDatasOut->iHeight;
        if (!ptPixelDatasOut->aucPixelDatas)
        {
            ptPixelDatasOut->aucPixelDatas = malloc(ptPixelDatasOut->iTotalBytes);
        }

        // ��������ػ���������ʼ��ַ aucPixelDatas ǿ��ת��Ϊ unsigned int * ���ͣ�����ֵ�� pdwDest��
        //����RGB32���ݣ����ֽڣ���ֵ
        pdwDest = (unsigned int *)ptPixelDatasOut->aucPixelDatas;
        
         // ����ת����RGB565 -> RGB32
        for (y = 0; y < ptPixelDatasOut->iHeight; y++)
        {
            for (x = 0; x < ptPixelDatasOut->iWidth; x++)
            {
                color = *pwSrc++;   //pwSrcΪunsigned short���ͣ�ָ��������Խ2�ֽ�
                /* ��RGB565��ʽ����������ȡ��R,G,B */
                //RRRRR GGGGGG BBBBB
                r = color >> 11;
                g = (color >> 5) & (0x3f); //ֻȡ��6λ
                b = color & 0x1f;   //ֻȡ��5λ

                /* ��r,g,bתΪ0x00RRGGBB��32λ���� */
                //ÿһ����ɫλ����Ϊһ���ֽ�
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

/* ���� */
static T_VideoConvert g_tRgb2RgbConvert = {
    .name        = "rgb2rgb",
    .isSupport   = isSupportRgb2Rgb,
    .Convert     = Rgb2RgbConvert,
    .ConvertExit = Rgb2RgbConvertExit,
};


/* ע�� */
int Rgb2RgbInit(void)
{
    return RegisterVideoConvert(&g_tRgb2RgbConvert);
}



