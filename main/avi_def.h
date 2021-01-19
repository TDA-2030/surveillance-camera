#ifndef _AVI_DEFINE_H_
#define _AVI_DEFINE_H_

#include "stdint.h"


/**
 * reference links: https://www.cnblogs.com/songhe364826110/p/7619949.html 
 */

typedef struct {
    uint8_t id[4];        
    uint32_t size;  //块大小，等于去掉id和size的大小
    uint8_t type[4];   
}AVI_RIFF_HEAD, AVI_LIST_HEAD;

typedef struct {
    uint8_t id[4];            //块ID，固定为avih
    uint32_t size;              //块大小，等于struct avi_avih_chunk去掉id和size的大小
    uint32_t us_per_frame;      //视频帧间隔时间(以微秒为单位)
    uint32_t max_bytes_per_sec; //AVI文件的最大数据率
    uint32_t padding;           //设为0即可
    uint32_t flags;             //AVI文件全局属性，如是否含有索引块、音视频数据是否交叉存储等
    uint32_t total_frames;      //总帧数
    uint32_t init_frames;       //为交互格式指定初始帧数(非交互格式应该指定为0)
    uint32_t streams;           //文件包含的流的个数，仅有视频流时为1
    uint32_t suggest_buff_size; //指定读取本文件建议使用的缓冲区大小，通常为存储一桢图像                                            //以及同步声音所需的数据之和，不指定时设为0
    uint32_t width;             //视频主窗口宽度（单位：像素）
    uint32_t height;            //视频主窗口高度（单位：像素）
    uint32_t reserved[4];       //保留段，设为0即可
}AVI_AVIH_CHUNK;

typedef struct avi_rect_frame
{
    short left;
    short top;
    short right;
    short bottom;    
}AVI_RECT_FRAME;

typedef struct avi_strh_chunk
{    
    uint8_t id[4];            //块ID，固定为strh
    uint32_t size;              //块大小，等于struct avi_strh_chunk去掉id和size的大小
    uint8_t stream_type[4];   //流的类型，vids表示视频流，auds表示音频流
    uint8_t codec[4];         //指定处理这个流需要的解码器，如JPEG
    uint32_t flags;             //标记，如是否允许这个流输出、调色板是否变化等，一般设为0即可
    unsigned short priority;        //流的优先级，视频流设为0即可
    unsigned short language;        //音频语言代号，视频流设为0即可
    uint32_t init_frames;       //为交互格式指定初始帧数(非交互格式应该指定为0)
    uint32_t scale;             //
    uint32_t rate;              //对于视频流，rate / scale = 帧率fps
    uint32_t start;             //对于视频流，设为0即可
    uint32_t length;            //对于视频流，length即总帧数
    uint32_t suggest_buff_size; //读取这个流数据建议使用的缓冲区大小
    uint32_t quality;           //流数据的质量指标
    uint32_t sample_size;       //音频采样大小，视频流设为0即可
    AVI_RECT_FRAME rcFrame;         //这个流在视频主窗口中的显示位置，设为{0,0，width,height}即可
}AVI_STRH_CHUNK;

/*对于视频流，strf块结构如下*/
typedef struct avi_strf_chunk
{
    uint8_t id[4];             //块ID，固定为strf
    uint32_t size;               //块大小，等于struct avi_strf_chunk去掉id和size的大小
    uint32_t size1;              //size1含义和值同size一样
    uint32_t width;              //视频主窗口宽度（单位：像素）
    uint32_t height;             //视频主窗口高度（单位：像素）
    unsigned short planes;           //始终为1  
    unsigned short bitcount;         //每个像素占的位数，只能是1、4、8、16、24和32中的一个
    uint8_t compression[4];    //视频流编码格式，如JPEG、MJPG等
    uint32_t image_size;         //视频图像大小，等于width * height * bitcount / 8
    uint32_t x_pixels_per_meter; //显示设备的水平分辨率，设为0即可
    uint32_t y_pixels_per_meter; //显示设备的垂直分辨率，设为0即可
    uint32_t num_colors;         //含义不清楚，设为0即可   
    uint32_t imp_colors;         //含义不清楚，设为0即可
}AVI_STRF_CHUNK;

typedef struct avi_strl_list
{
    uint8_t id[4];    //块ID，固定为LIST    
    uint32_t size;      //块大小，等于struct avi_strl_list去掉id和size的大小        
    uint8_t type[4];  //块类型，固定为strl
    AVI_STRH_CHUNK strh;      
    AVI_STRF_CHUNK strf;      
}AVI_STRL_LIST;

typedef struct avi_hdrl_list
{
    uint8_t id[4];    //块ID，固定为LIST    
    uint32_t size;      //块大小，等于struct avi_hdrl_list去掉id和size的大小        
    uint8_t type[4];  //块类型，固定为hdrl
    AVI_AVIH_CHUNK avih;
    AVI_STRL_LIST  strl;
}AVI_HDRL_LIST;

typedef struct
{
	uint32_t dwMicroSecPerFrame;     //显示每帧所需的时间ns，定义avi的显示速率
	uint32_t dwMaxBytesPerSec;       // 最大数据传输率
	uint32_t dwPaddingGranularity;   //记录块的长度须为此值的倍数，通常是2048
	uint32_t dwFlags;       // AVI文件的特殊属性，包含文件中的任何标志字。如：有无索引块，是否是interlaced，是否含版权信息等
    uint32_t dwTotalFrames;  	    // 数据帧的总数
    uint32_t dwInitialFrames;     // 在开始播放前需要的帧数
    uint32_t dwStreams;           //文件中包含的数据流种类
	uint32_t dwSuggestedBufferSize;//建议使用的缓冲区的大小，通常为存储一帧图像以及同步声音所需要的数据之和，大于最大的CHUNK的大小
    uint32_t dwWidth;             //图像宽，像素
    uint32_t dwHeight;            //图像高，像素
    uint32_t dwReserved[4];       //保留值dwScale,dwRate,dwStart,dwLength
} MainAVIHeader;

/**
"db"：未压缩的视频帧（RGB数据流）；
"dc"：压缩的视频帧；
"wb"：音频未压缩数据（Wave数据流）；
"wc"：音频压缩数据（压缩的Wave数据流）；
"pc"：改用新的调色板。（新的调色板使用一个数据结构AVIPALCHANGE来定义。如果一个流的调色板中途可能改变，则应在这个流格式的描述中，也就是AVISTREAMHEADER结构的dwFlags中包含一个AVISF_VIDEO_PALCHANGES标记。）
*/

#endif