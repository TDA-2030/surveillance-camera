#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "file_manage.h"
#include "pwm_audio.h"
#include "avifile.h"
#include "vidoplayer.h"

static const char *TAG = "videoplayer";
FILE      *fileR;
uint32_t  BytesRD;
uint8_t   Frame_buf[1024*30]={0};

uint8_t   *pbuffer;

uint32_t  mid;
uint32_t  Strsize;
uint16_t  Strtype;

static uint8_t timeout;
extern WAVEFORMAT*   wavinfo;
extern avih_TypeDef* avihChunk;

void audio_init(void)
{
    pwm_audio_config_t pac;
    pac.duty_resolution    = LEDC_TIMER_10_BIT;
    pac.gpio_num_left      = 12;
    pac.ledc_channel_left  = LEDC_CHANNEL_0;
    pac.gpio_num_right     = -1;
    pac.ledc_channel_right = LEDC_CHANNEL_1;
    pac.ledc_timer_sel     = LEDC_TIMER_0;
    pac.tg_num             = TIMER_GROUP_0;
    pac.timer_num          = TIMER_0;
    pac.ringbuf_len        = 1024 * 8;
    pwm_audio_init(&pac);

    // pwm_audio_set_param(wave_framerate, wave_bits, wave_ch);
    // pwm_audio_start();
    // pwm_audio_set_volume(0);

    // pwm_audio_write((uint8_t *)wave_array + index, block_w, &cnt, 5000 / portTICK_PERIOD_MS);

}


void AVI_play(char *filename)
{
  uint32_t offset;
  uint16_t audiosize;
  uint8_t avires=0;
  uint8_t audiosavebuf;
  pbuffer=Frame_buf;
  fileR=fopen(filename,"rb");
  if(fileR==NULL)
  {
    ESP_LOGE(TAG,"Cannot open file!(1)");	
    return;
  }
  
  BytesRD = fread(pbuffer,20480,1,fileR);
  avires=AVI_Parser(pbuffer);//解析AVI文件格式
  if(avires)
  {
    ESP_LOGE(TAG, "%d:File format error!(%d)", __LINE__, avires);
    return;    
  }
  
  avires=Avih_Parser(pbuffer+32);//解析avih数据块
  if(avires)
  {
    ESP_LOGE(TAG, "%d:File format error!(%d)", __LINE__, avires);
    return;    
  }
  
  avires=Strl_Parser(pbuffer+88);//解析strh数据块
  if(avires)
  {
    ESP_LOGE(TAG, "%d:File format error!(%d)", __LINE__, avires);
    return;    
  }
  
  avires=Strf_Parser(pbuffer+164);//解析strf数据块
  if(avires)
  {
    ESP_LOGE(TAG, "%d:File format error!(%d)", __LINE__, avires);
    return;    
  }
  
  mid=Search_Movi(pbuffer);//寻找movi ID		
  if(mid==0)
  {
    ESP_LOGE(TAG, "%d:File format error!(%d)", __LINE__, avires);
    return;    
  }
  
  Strtype=MAKEWORD(pbuffer+mid+6);//流类型
  Strsize=MAKEuint32_t(pbuffer+mid+8);//流大小
  if(Strsize%2)Strsize++;//奇数加1
  fseek(fileR, mid+12, SEEK_SET);//跳过标志ID
  
  offset=Search_Auds(pbuffer);
  if(offset==0)
  {
    ESP_LOGE(TAG, "File format error!(7)");
    return;    
  }  
  audiosize=*(uint8_t *)(pbuffer+offset+4)+256*(*(uint8_t *)(pbuffer+offset+5));
  if(audiosize==0)
  {
    offset=(uint32_t)pbuffer+offset+4;
    mid=Search_Auds((uint8_t *)offset);
    if(mid==0)
    {
      ESP_LOGE(TAG, "File format error!(8)");
      return;    
    }
    audiosize=*(uint8_t *)(mid+offset+4)+256*(*(uint8_t *)(mid+offset+5));
  }
  
  timeout=0;

  // while(1)//播放循环
  // {					
  //   if(Strtype==T_vids)//显示帧
  //   {      
  //     pbuffer=Frame_buf;
  //     f_read(&fileR,pbuffer,Strsize+8,&BytesRD);//读入整帧+下一数据流ID信息
  //     mjpegdraw(pbuffer,BytesRD);
  //     while(timeout==0)
  //     {        
  //       __WFI();
  //     }
  //     timeout=0;
  //   }//显示帧
  //   else if(Strtype==T_auds)//音频输出
  //   { 
  //     uint8_t i;
  //     audiosavebuf++;
  //     if(audiosavebuf>3)audiosavebuf=0;
  //     do
  //     {
  //       i=audiobufflag;
  //       if(i)i--;
  //       else i=3; 
  //     }while(audiobufflag==i);
  //     f_read(&fileR,Sound_buf[audiosavebuf],Strsize+8,&BytesRD);//读入整帧+下一数据流ID信息
	// 	  pbuffer=Sound_buf[audiosavebuf];      
  //   }
  //   else break;
  //   Strtype=MAKEWORD(pbuffer+Strsize+2);//流类型
  //   Strsize=MAKEDWORD(pbuffer+Strsize+4);//流大小									
  //   if(Strsize%2)Strsize++;//奇数加1							   	
  // }
  
  fclose(fileR);
}
