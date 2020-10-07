#ifndef _PRO_KIT_H_
#define _PRO_KIT_H_





typedef enum
{

	TYPE_BOOL = 0,
	
	TYPE_CHAR,
	TYPE_UCHAR,
	
	TYPE_SHORT,
	TYPE_USHORT,
	
	TYPE_INT,
	TYPE_UINT,
	
	TYPE_LONG,
	TYPE_ULONG,
	
	TYPE_FLOAT,
	TYPE_DOUBLE,
	
	TYPE_GPS,
	TYPE_LBS,
	
	TYPE_STRING,

} DATA_TYPE;


typedef struct
{

	char *name;
	void *dataPoint;
	DATA_TYPE dataType;
	_Bool flag;

} DATA_STREAM;


typedef enum
{

	FORMAT_TYPE1 = 1,
	FORMAT_TYPE2,
	FORMAT_TYPE3,
	FORMAT_TYPE4,
	FORMAT_TYPE5

} FORMAT_TYPE;


typedef struct
{
	
	//�ƶ����š��ƶ����Һ���Ĭ��Ϊ0��460
	char cell_id[8];			//��վ��
	char lac[8];				//����������
	
	/*
		0 GSM 
		1 GSM Compact 
		2 UTRAN 
		3 GSM w/EGPRS 
		4 UTRAN w/HSDPA 
		5 UTRAN w/HSUPA 
		6 UTRAN w/HSDPA and HSUPA (
		7 E-UTRAN
		8 EC-GSM-IoT 
		9 E-UTRAN 
		10 E-UTRA connected to a 5G CN 
		11 NR connected to a 5G CN 
		12 NR connected to an EPS core 
		13 NG-RAN 
		14 E-UTRA-NR dual connectivity
	*/
	unsigned char network_type;	//������ʽ
	
	unsigned char flag;			//10-ʮ����		16-ʮ������

} DATA_LBS;


short DSTREAM_GetDataStream_Body(unsigned char type, DATA_STREAM *streamArray, unsigned short streamArrayCnt, unsigned char *buffer, short maxLen, short offset);

short DSTREAM_GetDataStream_Body_Measure(unsigned char type, DATA_STREAM *streamArray, unsigned short streamArrayCnt, _Bool flag);

#endif
