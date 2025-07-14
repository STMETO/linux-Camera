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
    int iError;                             // ���ڴ洢�������õķ��ش�����

    T_VideoDevice tVideoDevice;             // ������Ƶ�豸�Ľṹ��ʵ������ /dev/video0��
    PT_VideoConvert ptVideoConvert;         // ָ���ʽת�������ṹ���ָ�루������Ƶ��ʽת����
    int iPixelFormatOfVideo;                // ����ͷԭʼͼ������ظ�ʽ
    int iPixelFormatOfDisp;                 // ��ʾ�豸��LCD�������ظ�ʽ

    PT_VideoBuf ptVideoBufCur;              // ��ǰҪ��ʾ����Ƶ������ָ��
    T_VideoBuf tVideoBuf;                   // ԭʼͼ��֡��������������ͷ��ȡ�ģ�
    T_VideoBuf tConvertBuf;                 // ��ʽת�����ͼ�񻺳�����ת��Ϊ LCD ֧�ֵĸ�ʽ��
    T_VideoBuf tZoomBuf;                    // ���ź��ͼ�񻺳������������� LCD �ֱ��ʣ�
    T_VideoBuf tFrameBuf;                   // LCD ��ʾ���õ�����ͼ��֡������

    int iLcdWidth;                          // LCD �Ŀ�ȣ����أ�
    int iLcdHeigt;                          // LCD �ĸ߶ȣ����أ�
    int iLcdBpp;                            // LCD ÿ�����ص�λ����Bits Per Pixel��

    int iTopLeftX;                          // ͼ�������ʾʱ���Ͻ� X ����
    int iTopLeftY;                          // ͼ�������ʾʱ���Ͻ� Y ����

    float k;                                // ͼ��Ŀ�߱ȣ��������ż���

    //�жϲ���
    if (argc != 2)
    {
        printf("Usage:\n");
        printf("%s </dev/video0,1,...>\n", argv[0]);
        return -1;
    }
    
    /* һϵ�еĳ�ʼ�� */
	/* ע����ʾ�豸 */
	DisplayInit();

	/* ���ܿ�֧�ֶ����ʾ�豸: ѡ��ͳ�ʼ��ָ������ʾ�豸 */
	SelectAndInitDefaultDispDev("fb");
    
    /* ��õ�ǰ��ʾ�豸�ķֱ��� */
    GetDispResolution(&iLcdWidth, &iLcdHeigt, &iLcdBpp);

    /* ����Դ� */
    GetVideoBufForDisplay(&tFrameBuf);

    /* ��õ�ǰ��ʾ�豸����ʾ��ʽ */
    iPixelFormatOfDisp = tFrameBuf.iPixelFormat;

    /* ��ʼ��������Ƶ�ɼ�������ͷ����ϵͳ */
    VideoInit();

    /* ��ʼ�����������ͷ�豸������ /dev/video0�� */
    iError = VideoDeviceInit(argv[1], &tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("VideoDeviceInit for %s error!\n", argv[1]);
        return -1;
    }

    /* ��ȡ����ͷ�豸�ĸ�ʽ */
    iPixelFormatOfVideo = tVideoDevice.ptOPr->GetFormat(&tVideoDevice);

    /* ��ʽת����ܳ�ʼ�� */
    VideoConvertInit();

    /* ��ȡָ������\�����ʽ��ת����������MJPEG TO RGB565 */
    ptVideoConvert = GetVideoConvertForFormats(iPixelFormatOfVideo, iPixelFormatOfDisp);
    if (NULL == ptVideoConvert)
    {
        DBG_PRINTF("can not support this format convert\n");
        return -1;
    }

    /* ��������ͷ�豸 */
    iError = tVideoDevice.ptOPr->StartDevice(&tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("StartDevice for %s error!\n", argv[1]);
        return -1;
    }

    /* ������Ƶ���� */
    memset(&tVideoBuf, 0, sizeof(tVideoBuf));

    /* ��ʽת����Ļ��� */
    memset(&tConvertBuf, 0, sizeof(tConvertBuf));
    tConvertBuf.iPixelFormat     = iPixelFormatOfDisp;
    tConvertBuf.tPixelDatas.iBpp = iLcdBpp;
    
    /* ���Ż��� */
    memset(&tZoomBuf, 0, sizeof(tZoomBuf));
    
    while (1)
    {
        /* ��������ͷ���� */
        iError = tVideoDevice.ptOPr->GetFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
        {
            DBG_PRINTF("GetFrame for %s error!\n", argv[1]);
            return -1;
        }
        ptVideoBufCur = &tVideoBuf;//��ֵ����ǰ֡����

        //�������ͷ��ʽ����ʾ�豸�ĸ�ʽ�������ʼת��
        if (iPixelFormatOfVideo != iPixelFormatOfDisp)
        {
            /* ת��ΪRGB */
            iError = ptVideoConvert->Convert(&tVideoBuf, &tConvertBuf);
            DBG_PRINTF("Convert %s, ret = %d\n", ptVideoConvert->name, iError);
            if (iError)
            {
                DBG_PRINTF("Convert for %s error!\n", argv[1]);
                return -1;
            }            
            ptVideoBufCur = &tConvertBuf;
        }
        
        /* ���ͼ��ֱ��ʴ���LCD, ���� */
        if (ptVideoBufCur->tPixelDatas.iWidth > iLcdWidth || ptVideoBufCur->tPixelDatas.iHeight > iLcdHeigt)
        {
            /* ȷ�����ź�ķֱ��� */
            /* ��ͼƬ���������ŵ�VideoMem��, ������ʾ
             * 1. ��������ź�Ĵ�С
             */
            //��ͼ��ĸ߿�� k = �� / �����ں������š�
            k = (float)ptVideoBufCur->tPixelDatas.iHeight / ptVideoBufCur->tPixelDatas.iWidth;

            //���裺����ͼ��ͼ�����ųɺ� LCD һ����
            //�߶ȣ��� LCD ��� �� k �õ��ȱȵĸ߶ȡ�
            tZoomBuf.tPixelDatas.iWidth  = iLcdWidth;
            tZoomBuf.tPixelDatas.iHeight = iLcdWidth * k;

            //Ŀ�ģ� ����ղŵļ��㵼�¸߶ȳ��� LCD���Ǿͷ������������߶����ƣ����¼����ȡ�
            //�������Ľ���� ͼ��һ������ LCD ������������ʾ������������Ρ�
            if ( tZoomBuf.tPixelDatas.iHeight > iLcdHeigt)
            {
                tZoomBuf.tPixelDatas.iWidth  = iLcdHeigt / k;
                tZoomBuf.tPixelDatas.iHeight = iLcdHeigt;
            }

            //׼�����ź��ͼ�񻺳���������
            tZoomBuf.tPixelDatas.iBpp        = iLcdBpp;
            tZoomBuf.tPixelDatas.iLineBytes  = tZoomBuf.tPixelDatas.iWidth * tZoomBuf.tPixelDatas.iBpp / 8;
            tZoomBuf.tPixelDatas.iTotalBytes = tZoomBuf.tPixelDatas.iLineBytes * tZoomBuf.tPixelDatas.iHeight;

            //�����ڴ�
            if (!tZoomBuf.tPixelDatas.aucPixelDatas)
            {
                tZoomBuf.tPixelDatas.aucPixelDatas = malloc(tZoomBuf.tPixelDatas.iTotalBytes);
            }
            
            PicZoom(&ptVideoBufCur->tPixelDatas, &tZoomBuf.tPixelDatas);
            ptVideoBufCur = &tZoomBuf;
        }

        /* �ϲ���framebuffer */
        /* �������������ʾʱ���Ͻ����� */
        iTopLeftX = (iLcdWidth - ptVideoBufCur->tPixelDatas.iWidth) / 2;
        iTopLeftY = (iLcdHeigt - ptVideoBufCur->tPixelDatas.iHeight) / 2;

        PicMerge(iTopLeftX, iTopLeftY, &ptVideoBufCur->tPixelDatas, &tFrameBuf.tPixelDatas);

        //framebuffer ���������ˢ�µ��������豸
        FlushPixelDatasToDev(&tFrameBuf.tPixelDatas);

        //��������ͷ��������֡ͼ���������ˡ�
        iError = tVideoDevice.ptOPr->PutFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
        {
            DBG_PRINTF("PutFrame for %s error!\n", argv[1]);
            return -1;
        }                    

        /* ��framebuffer������ˢ��LCD��, ��ʾ */
    }
		
	return 0;
}

