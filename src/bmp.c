#include "bmp.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>

#define align(value, align)   ((( (value) + ( (align) - 1 ) ) \
                                     / (align) ) * (align) )

//BMP文件头（14字节）
typedef struct                       /**** BMP file header structure ****/  
{  
    unsigned int   bfSize;           /* Size of file */  
    unsigned short bfReserved1;      /* Reserved */  
    unsigned short bfReserved2;      /* ... */  
    unsigned int   bfOffBits;        /* Offset to bitmap data */  
} BITMAPFILEHEADER; 

//位图信息头（40字节）
typedef struct                       /**** BMP file info structure ****/  
{  
    unsigned int   biSize;           /* Size of info header */  
    int            biWidth;          /* Width of image */  
    int            biHeight;         /* Height of image */  
    unsigned short biPlanes;         /* Number of color planes */  
    unsigned short biBitCount;       /* Number of bits per pixel */  
    unsigned int   biCompression;    /* Type of compression to use */  
    unsigned int   biSizeImage;      /* Size of image data */  
    int            biXPelsPerMeter;  /* X pixels per meter */  
    int            biYPelsPerMeter;  /* Y pixels per meter */  
    unsigned int   biClrUsed;        /* Number of colors used */  
    unsigned int   biClrImportant;   /* Number of important colors */  
} BITMAPINFOHEADER;

/*******************************************************************************
* 函数名  : rgb2bmp
* 描  述	: 将RGB888格式的图片数据流转换成bmp格式的图片
* 输  入	: const char *filename  -- bmp文件名
*		  	  buf	    --RGB888数据
*		  	  width		--RGB图像的宽
*		  	  height	--RGB图像的高
* 返回值    : 成功返回 0
*		   失败返回 -1
*******************************************************************************/
int rgb2bmpfile(const char *filename, unsigned char *rgb_buf, int rgb_width, int rgb_height)
{  
    BITMAPFILEHEADER bfh;  
    BITMAPINFOHEADER bih;  
	int i = 0, row_align;

    if(rgb_width == 0 || rgb_height == 0 || rgb_buf == 0)
    {
        printf("Invalid parameter \n");
        return -1;
    }
    
    FILE *file = fopen(filename,"wb");
    if (!file) {
        printf("Error opening file\n");
        return -1;
    }

	//bmp的每行数据的长度要4字节对齐
	row_align = align(rgb_width * 3, 4);
    unsigned short bfType=0x4d42;    //'BM'             
    bfh.bfReserved1 = 0;  
    bfh.bfReserved2 = 0;  
    bfh.bfSize = 2 + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + row_align * rgb_height;  
    bfh.bfOffBits = 2 + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);  
  
    bih.biSize = sizeof(BITMAPINFOHEADER);  
    bih.biWidth = rgb_width;  
    bih.biHeight = rgb_height;  
    bih.biPlanes = 1;  
    bih.biBitCount = 24;  
    bih.biCompression = 0;  
    bih.biSizeImage = 0;  
    bih.biXPelsPerMeter = 0;  
    bih.biYPelsPerMeter = 0;  
    bih.biClrUsed = 0;  
    bih.biClrImportant = 0;  

    fwrite(&bfType, sizeof(unsigned char), sizeof(bfType), file);
    fwrite(&bfh, sizeof(unsigned char), sizeof(bfh), file);
    fwrite(&bih, sizeof(unsigned char), sizeof(bih), file);

	for(i = rgb_height-1; i >= 0 ; i--)
	{
        fwrite(rgb_buf + rgb_width * 3 * i, sizeof(unsigned char), rgb_width * 3, file);
	}

    fclose(file);
	
	return 0;
}  

