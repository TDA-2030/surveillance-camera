#include "esp_log.h"
#include "avifile.h"
#include <stdio.h>
#include <string.h>

AVI_TypeDef   AVI_file;
avih_TypeDef* avihChunk;
strh_TypeDef* strhChunk;
BITMAPINFO*   bmpinfo;
WAVEFORMAT*   wavinfo;
uint32_t      temp=0x00;
uint8_t       vids_ID;
uint8_t       auds_ID;

uint8_t AVI_Parser(uint8_t *buffer)
{
	temp=ReadUnit(buffer,0,4,1);//读"RIFF"
	if(temp!=RIFF_ID)return 1;
	AVI_file.RIFFchunksize=ReadUnit(buffer,4,4,1);//RIFF数据块长度
	temp=ReadUnit(buffer,8,4,1);//读"AVI "
	if(temp!=AVI_ID)return 2;
	temp=ReadUnit(buffer,12,4,1);//读"LIST"
	if(temp!=LIST_ID)return 3;
	AVI_file.LISTchunksize=ReadUnit(buffer,16,4,1);//LIST数据块长度
	temp=ReadUnit(buffer,20,4,1);//读"hdrl"
	if(temp!=hdrl_ID)return 4;
	temp=ReadUnit(buffer,24,4,1);//读"avih"
	if(temp!=avih_ID)return 5;
	AVI_file.avihsize=ReadUnit(buffer,28,4,1);//avih数据块长度	
	return 0;				
}

uint8_t Avih_Parser(uint8_t *buffer)
{
	avihChunk=(avih_TypeDef*)buffer;
#ifdef DEBUGINFO
	printf("\r\navih数据块信息:");
	printf("\r\nSecPerFrame:%d",avihChunk->SecPerFrame);
	printf("\r\nMaxByteSec:%d",avihChunk->MaxByteSec);
	printf("\r\nChunkBase:%d",avihChunk->ChunkBase);
	printf("\r\nSpecProp:%d",avihChunk->SpecProp);
	printf("\r\nTotalFrame:%d",avihChunk->TotalFrame);
	printf("\r\nInitFrames:%d",avihChunk->InitFrames);
	printf("\r\nStreams:%d",avihChunk->Streams);
	printf("\r\nRefBufSize:%d",avihChunk->RefBufSize);
	printf("\r\nWidth:%d",avihChunk->Width);
	printf("\r\nHeight:%d\n",avihChunk->Height);
#endif
	if((avihChunk->Width>800)||(avihChunk->Height>480))return 1;//视频尺寸不支持
	// if(avihChunk->Streams!=2)return 2;//视频流数不支持
	return 0;
}

uint8_t Strl_Parser(uint8_t *buffer)
{
	temp=ReadUnit(buffer,0,4,1);//读"LIST"
	if(temp!=LIST_ID)return 1;
	AVI_file.strlsize=ReadUnit(buffer,4,4,1);//strl数据块长度
	temp=ReadUnit(buffer,8,4,1);//读"strl"
	if(temp!=strl_ID)return 2;
	temp=ReadUnit(buffer,12,4,1);//读"strh"
	if(temp!=strh_ID)return 3;
	AVI_file.strhsize=ReadUnit(buffer,16,4,1);//strh数据块长度
	strhChunk=(strh_TypeDef*)(buffer+20);		 //108
#ifdef DEBUGINFO
	printf("\r\nstrh数据块信息:");	
	printf("\r\nStreamType:%s",strhChunk->StreamType);
	printf("\r\nHandler:%s",strhChunk->Handler);//编码类型MJPEG
	printf("\r\nStreamFlag:%d",strhChunk->StreamFlag);
	printf("\r\nPriority:%d",strhChunk->Priority);
	printf("\r\nLanguage:%d",strhChunk->Language);
	printf("\r\nInitFrames:%d",strhChunk->InitFrames);
	printf("\r\nScale:%d",strhChunk->Scale);
	printf("\r\nRate:%d",strhChunk->Rate);
	printf("\r\nStart:%d",strhChunk->Start);
	printf("\r\nLength:%d",strhChunk->Length);
	printf("\r\nRefBufSize:%d",strhChunk->RefBufSize);
	printf("\r\nQuality:%d",strhChunk->Quality);
	printf("\r\nSampleSize:%d",strhChunk->SampleSize);
	printf("\r\nFrameLeft:%d",strhChunk->Frame.Left);
	printf("\r\nFrameTop:%d",strhChunk->Frame.Top);
	printf("\r\nFrameRight:%d",strhChunk->Frame.Right);
	printf("\r\nFrameBottom:%d\n",strhChunk->Frame.Bottom);
#endif
	if(strhChunk->Handler[0]!='M')return 4;
	return 0;
}

uint8_t Strf_Parser(uint8_t *buffer)
{
	temp=ReadUnit(buffer,0,4,1);//读"strf"
	if(temp!=strf_ID)return 1;
	if(strhChunk->StreamType[0]=='v')//第一个流为视频流
	{
		vids_ID='0';
		auds_ID='1';
		bmpinfo=(BITMAPINFO*)(buffer+8);
		wavinfo=(WAVEFORMAT*)(buffer+4332);
	}
	else if(strhChunk->StreamType[0]=='a')//第一个流为音频流
	{
		vids_ID='1';
		auds_ID='0';
		wavinfo=(WAVEFORMAT*)(buffer+8);
		bmpinfo=(BITMAPINFO*)(buffer+4332);
	}
#ifdef DEBUGINFO		
	printf("\r\nstrf数据块信息(视频流):");		
	printf("\r\n本结构体大小:%d",bmpinfo->bmiHeader.Size);
	printf("\r\n图像宽:%ld",bmpinfo->bmiHeader.Width);
	printf("\r\n图像高:%ld",bmpinfo->bmiHeader.Height);
	printf("\r\n平面数:%d",bmpinfo->bmiHeader.Planes);
	printf("\r\n像素位数:%d",bmpinfo->bmiHeader.BitCount);
	printf("\r\n压缩类型:%s",bmpinfo->bmiHeader.Compression);
	printf("\r\n图像大小:%d",bmpinfo->bmiHeader.SizeImage);
	printf("\r\n水平分辨率:%ld",bmpinfo->bmiHeader.XpixPerMeter);
	printf("\r\n垂直分辨率:%ld",bmpinfo->bmiHeader.YpixPerMeter);
	printf("\r\n使用调色板颜色数:%d",bmpinfo->bmiHeader.ClrUsed);
	printf("\r\n重要颜色:%d",bmpinfo->bmiHeader.ClrImportant);

	printf("\r\nstrf数据块信息(音频流):\n");
	printf("\r\n格式标志:%d",wavinfo->FormatTag);
	printf("\r\n声道数:%d",wavinfo->Channels);
	printf("\r\n采样率:%d",wavinfo->SampleRate);
	printf("\r\n波特率:%d",wavinfo->BaudRate);
	printf("\r\n块对齐:%d",wavinfo->BlockAlign);
	printf("\r\n本结构体大小:%d\n",wavinfo->Size);
#endif
	return 0;
}

uint16_t Search_Movi(uint8_t* buffer)
{
	uint16_t i;
	for(i=0;i<20480;i++)
	{
	   	if(buffer[i]==0x6d)
			if(buffer[i+1]==0x6f)
				if(buffer[i+2]==0x76)	
					if(buffer[i+3]==0x69)return i;//找到"movi"	
	}
	return 0;		
}

uint16_t Search_Fram(uint8_t* buffer)
{
	uint16_t i;
	for(i=0;i<20480;i++)
	{
	   	if(buffer[i]=='0')
			if(buffer[i+1]==vids_ID)
				if(buffer[i+2]=='d')	
					if(buffer[i+3]=='c')return i;//找到"xxdc"	
	}
	return 0;		
}
uint16_t Search_Auds(uint8_t* buffer)
{
	uint16_t i;
	for(i=0;i<20480;i++)
	{
	   	if(buffer[i]=='0')
			if(buffer[i+1]==auds_ID)
				if(buffer[i+2]=='w')	
					if(buffer[i+3]=='b')return i;//找到"xxdc"	
	}
	return 0;		
}

uint32_t __REV(uint32_t le)
{
	uint32_t ret = (le & 0xff) << 24 
            | (le & 0xff00) << 8 
            | (le & 0xff0000) >> 8 
            | ((le >> 24) & 0xff);
	return ret;
}

uint32_t ReadUnit(uint8_t *buffer,uint8_t index,uint8_t Bytes,uint8_t Format)//1:大端模式;0:小端模式
{
  	uint8_t off=0;
  	uint32_t unit=0;  
  	for(off=0;off<Bytes;off++)unit|=buffer[off+index]<<(off*8);
  	if(Format)unit=__REV(unit);//大端模式
  	return unit;
}


