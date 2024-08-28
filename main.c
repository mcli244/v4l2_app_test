#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>

#include "bmp.h"

#define WIDTH   640
#define HEIGHT   360

volatile int is_exec = 1;

void handler(int signum) 
{
	is_exec = 0;
}

// 这里输入的图像数据是RGB 8 8 8的格式
// 将其保存为bpm位图数据
int saveImage(unsigned char *dat, int len, int index)
{
    char name[64] = {0};
    sprintf(name, "test_%02d.bmp", index);
    rgb2bmpfile(name, dat, WIDTH, HEIGHT);
    return 0;
}


static int xioctl(int fd, int request, void *arg) {
  int r;

  do {
    r = ioctl(fd, request, arg);
  } while (-1 == r && EINTR == errno);

  return r;
}


void yuyv_to_rgb(unsigned char *yuyvdata, unsigned char *rgbdata, int w, int h)
{
    int r1, g1, b1;
    int r2, g2, b2;
    for (int i = 0; i < w * h / 2; i++)
    {
        char data[4];
        memcpy(data, yuyvdata + i * 4, 4);
        // Y0 U0 Y1 V1-->[Y0 U0 V1] [Y1 U0 v1]
        unsigned char Y0 = data[0];
        unsigned char U0 = data[1];
        unsigned char Y1 = data[2];
        unsigned char V1 = data[3];

        r1 = Y0 + 1.4075 * (V1-128); if(r1>255)r1=255; if(r1<0)r1=0;
	    g1 =Y0 - 0.3455 * (U0-128) - 0.7169 * (V1-128); if(g1>255)g1=255; if(g1<0)g1=0;
        b1 = Y0 + 1.779 *(U0-128); if(b1>255)b1=255; if(b1<0)b1=0;

	    r2 = Y1+1.4075* (V1-128) ;if(r2>255)r2=255; if(r2<0)r2=0;
	    g2 = Y1- 0.3455 *(U0-128) - 0.7169*(V1-128); if(g2>255)g2=255;if(g2<0)g2=0;
        b2 = Y1+ 1.779 * (U0-128);if(b2>255)b2=255;if(b2<0)b2=0;

        rgbdata[i * 6 + 0] = r1;
        rgbdata[i * 6 + 1] = g1;
        rgbdata[i * 6 + 2] = b1;
        rgbdata[i * 6 + 3] = r2;
        rgbdata[i * 6 + 4] = g2;
        rgbdata[i * 6 + 5] = b2;
    }
}

int main(int argc, char **argv)
{
    int fd;
    int ret = 0;
    struct v4l2_capability cap;
    enum v4l2_buf_type capture_buf_type;
    struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

    if (argc != 2)
    {
        printf("Usage: v4l2_test <device>\n");
        printf("example: v4l2_test /dev/video0\n");
        return -1;
    }

    signal(SIGINT, handler);

    fd = open(argv[1], O_RDWR);
    if (fd < 0){
        printf("open video device fail\n");
        return -1;
    }

    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0){
        printf("Get video capability error!\n");
        return -1;
    }

    printf("device_caps:0x%x.\n", cap.device_caps);
    if (!(cap.device_caps & V4L2_CAP_VIDEO_CAPTURE)){
        printf("Video device not support capture!\n");
        close(fd);
        return -1;
    }
    printf("Support capture!\n");

    struct v4l2_input inp;
    printf("============ VIDIOC_ENUMINPUT ==================\n");
    for (int i = 0; i < 4; i++)
    {
        inp.index = i;
        if (0 == xioctl(fd, VIDIOC_ENUMINPUT, &inp))
        {
            printf("0x%08llx \t %s\n", inp.std, inp.name);
        }
    }

    // 获取驱动支持的格式
    struct v4l2_fmtdesc fmtdesc;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmtdesc.index = 0;
    printf("============ VIDIOC_ENUM_FMT ==================\n");
    for (int i = 0; ; i++)
    {
        fmtdesc.index = i;
        if (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc))
        {
            printf("%02d 0x%x \t %s\n", i, fmtdesc.pixelformat, fmtdesc.description);
        }
        else
            break;
    }	

    // 获取当前使用的格式
    capture_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    struct v4l2_format g_v4l2_fmt;
    memset(&g_v4l2_fmt, 0, sizeof(g_v4l2_fmt));
    g_v4l2_fmt.type = capture_buf_type;
    ret = xioctl(fd, VIDIOC_G_FMT, &g_v4l2_fmt);
    if (ret != 0)
    {
        printf("VIDIOC_G_FMT failed ret:%d\n", ret);
        goto _exit;
    }
    printf("============ VIDIOC_G_FMT ==================\n");
    printf("width:%d height:%d pixelformat:0x%x type:0x%x\n", 
        g_v4l2_fmt.fmt.pix.width, g_v4l2_fmt.fmt.pix.height, 
        g_v4l2_fmt.fmt.pix.pixelformat, g_v4l2_fmt.type);

    // 从支持的格式中选择一个格式设置到驱动
    struct v4l2_format v4l2_fmt;
    v4l2_fmt.type = capture_buf_type;
    // 重要！！ 分辨率不能随意设置，需要从驱动支持的分辨率下取一个
    v4l2_fmt.fmt.pix.width = WIDTH;                     // 宽度
    v4l2_fmt.fmt.pix.height = HEIGHT;                    // 高度
    v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; // 像素格式
    v4l2_fmt.fmt.pix.field = V4L2_FIELD_TOP;
    ret = xioctl(fd, VIDIOC_S_FMT, &v4l2_fmt);
    if (0 != ret)
    {
        printf("set VIDIOC_S_FMT failed ret:%d\n", ret);
        goto _exit;
    }

    printf("VIDIOC_S_FMT succ\n");

    memset(&g_v4l2_fmt, 0, sizeof(g_v4l2_fmt));
    g_v4l2_fmt.type = capture_buf_type; // 这个是v4l2内部做处理的
    ret = xioctl(fd, VIDIOC_G_FMT, &g_v4l2_fmt);
    if (ret != 0)
    {
        printf("ret:%d\n", ret);
    }
    printf("============ VIDIOC_G_FMT ==================\n");
    printf("width:%d height:%d pixelformat:0x%x type:0x%x\n", 
        g_v4l2_fmt.fmt.pix.width, g_v4l2_fmt.fmt.pix.height, 
        g_v4l2_fmt.fmt.pix.pixelformat, g_v4l2_fmt.type);

    // 申请缓存，用于存储相机输出的图像
    #define BUFFER_COUNT    4
    struct v4l2_requestbuffers req;
    req.count = BUFFER_COUNT; // 缓存数量
    req.type = capture_buf_type;
    req.memory = V4L2_MEMORY_MMAP;          // 内存映射方式，由驱动层申请内存，应用层用指针映射过去即可
    ret = xioctl(fd, VIDIOC_REQBUFS, &req);
    if (ret < 0)
    {
        printf("VIDIOC_REQBUFS failed ret:%d\n", ret);
        goto _exit;
    }

    struct MyVideoBuffer
    {
        void *data;
        size_t frame_size;
        uint8_t *frame_buffer;
        int len;
    };
    struct MyVideoBuffer mVideoBuffer[BUFFER_COUNT];
    
    for(int i=0; i<BUFFER_COUNT; i++)
    {
        // 定义v4l2_buffer结构，每个buffer需要初始化为什么格式？在buffer结构的成员指定好
        struct v4l2_buffer buffer;
        memset(&buffer, 0, sizeof(buffer));
        buffer.index = i; // 初始化第0~3个buffer中的第i个
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.type = capture_buf_type; //支持的设备视频输入类型

        if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buffer))  // 从驱动中取出VIDIOC_REQBUFS时申请的内存缓存信息 "buffer.m.offset"
		{
            printf("get %d VIDIOC_QUERYBUF error\n", i);
            // TODO: 错误处理
			goto _exit;
        }

        // 映射buffer的驱动空间地址到应用层的指针并且保存指针。
        mVideoBuffer[i].data= mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buffer.m.offset);
        mVideoBuffer[i].len = buffer.length;
        mVideoBuffer[i].frame_size = v4l2_fmt.fmt.pix.width * v4l2_fmt.fmt.pix.height * 3;
        mVideoBuffer[i].frame_buffer = (uint8_t *) calloc(1, mVideoBuffer[i].frame_size);

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buffer)) // 将这个信息放回驱动
		{
            printf("set %d VIDIOC_QUERYBUF to line error\n", i);
            // TODO: 错误处理
			goto _exit;
        }
    }

    printf("============ VIDIOC_QUERYBUF/VIDIOC_QUERYBUF ==================\n");
    for(int i=0; i<BUFFER_COUNT; i++){
        printf("buffer:%p len:%d\n", mVideoBuffer[i].data, mVideoBuffer[i].len);
    }

    // 通知驱动，启动摄像头
	if (-1 == xioctl(fd, VIDIOC_STREAMON, &capture_buf_type)) 
	{
		printf("set VIDIOC_STREAMON error\n");
		goto _free;
	}

    fd_set fds;
	struct timeval tv;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
    int frameId = 0;
    uint8_t rgb_buf[WIDTH*HEIGHT*3 + 10];
    while(is_exec)
    {
        FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 50000;
		
		if(0 >= select(fd + 1, &fds, NULL, NULL, &tv)) 
			continue;
        
        // 获取图像，主要是获取索引
        struct v4l2_buffer t_buffer;
		memset(&t_buffer, 0, sizeof(t_buffer));
		t_buffer.type = capture_buf_type; //支持的设备视频输入类型
		t_buffer.memory = V4L2_MEMORY_MMAP;
		if(-1 == xioctl(fd, VIDIOC_DQBUF, &t_buffer)) 
			continue;
        
        // 判断获取的状态有没有出错
		if (t_buffer.flags & V4L2_BUF_FLAG_ERROR) 
		{
			printf("v4l2 buf error! buf flag 0x%x, index=%d", t_buffer.flags, t_buffer.index);
			continue;
		}

        // 数据格式为RGB 888
        printf("index:%d bytesused:%d flags:0x%x field:0x%x t_buffer.m.offset:0x%x\n", 
            t_buffer.index, t_buffer.bytesused, t_buffer.flags, t_buffer.field, t_buffer.m.offset);
        memcpy(mVideoBuffer[t_buffer.index].frame_buffer, mVideoBuffer[t_buffer.index].data, t_buffer.bytesused);
        saveImage(mVideoBuffer[t_buffer.index].frame_buffer, t_buffer.bytesused, frameId ++);

        if(frameId >= 10)
            break;
        

        // 处理完后，把这个缓冲器再还回驱动层。实际上是再次加入到驱动层的队列中去。
        if (-1 == xioctl(fd, VIDIOC_QBUF, &t_buffer)) // 将这个信息放回驱动
		{
            printf("set %d VIDIOC_QUERYBUF to line error\n", t_buffer.index);
            continue;
        }
    }

    // 关闭摄像头
    if(-1 == xioctl(fd, VIDIOC_STREAMOFF, &capture_buf_type)) 
	{
        printf("set VIDIOC_STREAMOFF error\n");
    }

_free:
    for(int i=0; i<BUFFER_COUNT; i++) 
        munmap(mVideoBuffer[i].data, mVideoBuffer[i].len);

    // // 16通过xioctl向已经打开的设备发送命令VIDIOC_REQBUFS: 清除buffer
    // struct v4l2_requestbuffers req_buffers_clear;
	// memset(&req_buffers_clear, 0, sizeof(req_buffers_clear));
    // req_buffers_clear.type = capture_type; //支持的设备视频输入类型
    // req_buffers_clear.memory = V4L2_MEMORY_MMAP;
    // req_buffers_clear.count = 0;
    // if(-1 == xioctl(fd, VIDIOC_REQBUFS, &req_buffers_clear)) 
	// {
    //     printf("set VIDIOC_REQBUFS error\n");
	// 	goto exit;
    // }

_exit:
    close(fd);

    return 0;
}