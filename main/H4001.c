#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#define Delay(t) vTaskDelay(pdMS_TO_TICKS(t))
#define gpio_set_value(_p, _v) gpio_set_level(_p, _v)
#define mdelay(_t) //ets_delay_us((_t)*1000)
#define udelay(_t) //ets_delay_us(_t)


struct st7701_data
{
    int rst;
    int cs;
    int clk;
    int sdo;
};

/**
 * @brief 这是一个单例的驱动
 * 
 */
static const struct st7701_data s_st7701 = 
{
    .rst = -1,
    .cs = 17,
    .clk = 16,
    .sdo = 15
};



static int st7701_init_gpios(const struct st7701_data* st)
{
    gpio_config_t io_conf = {};
    uint64_t mask = 0; 

    if (st->rst >= 0)
    {
        mask |= (1ULL << st->rst);
    }

    mask |= (1ULL << st->cs);
    mask |= (1ULL << st->clk);
    mask |= (1ULL << st->sdo);

    // dlog("st7701 gpio mask:%llx\n", mask);

    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = mask;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);    

    if (st->rst >= 0)
    {
        gpio_set_value(st->rst, 1);
    }    

    gpio_set_value(st->cs, 1);
    gpio_set_value(st->clk, 1);
    gpio_set_value(st->sdo, 1);

    return 0;
}


#define RESET(n)\
     gpio_set_level(s_st7701.rst, n)

#define CS(n)\
     gpio_set_level(s_st7701.cs, n)

#define SCK(n)\
     gpio_set_level(s_st7701.clk, n)

#define SDO(n)\
	 gpio_set_level(s_st7701.sdo, n)


static void SPI_SendData(unsigned short i)
{
   unsigned char n;
   for(n=0; n<16; n++) {
	   if(i&0x8000)
		   SDO(1);
	   else
		   SDO(0);
	   i = i << 1;

	   SCK(1);
	   udelay(10);
	   SCK(0);
	   udelay(10);
   }
}

static void SPI_WriteComm(unsigned short c)
{
	CS(0);
	udelay(10);
	SCK(0);
	udelay(10);

	SPI_SendData(((c>>8)&0x00FF)|0x2000);

	SCK(1);
	udelay(10);
	SCK(0);

	CS(1);
	udelay(10);
	CS(0);
	udelay(10);
	
	SPI_SendData((c&0x00FF));
	CS(1);
	udelay(10);
	
}

static void SPI_WriteData(unsigned short d)
{
	CS(0);
	udelay(10);
	SCK(0);
	udelay(10);

	d &= 0x00FF;
	d |= 0x4000;
	SPI_SendData(d);

	SCK(1);
	udelay(10);
	SCK(0);
	udelay(10);

	CS(1);
	udelay(10);
}


void h4001_init()
{
	st7701_init_gpios(&s_st7701);

	SPI_WriteComm(0x1100); //Software reset
	Delay(300);

	//---------------------------------------Bank0 Setting-------------------------------------------------//
	//------------------------------------Display Control setting----------------------------------------------//
	SPI_WriteComm(0xFF00);SPI_WriteData(0x77);
	SPI_WriteComm(0xFF01);SPI_WriteData(0x01);
	SPI_WriteComm(0xFF02);SPI_WriteData(0x00);
	SPI_WriteComm(0xFF03);SPI_WriteData(0x00);
	SPI_WriteComm(0xFF04);SPI_WriteData(0x10);

	SPI_WriteComm(0xC000);SPI_WriteData(0x3B);
	SPI_WriteComm(0xC001);SPI_WriteData(0x00);

    SPI_WriteComm(0xC100);SPI_WriteData(0x0D);
    SPI_WriteComm(0xC101);SPI_WriteData(0x02);

	SPI_WriteComm(0xC200);SPI_WriteData(0x21);
	SPI_WriteComm(0xC201);SPI_WriteData(0x08);

	
	SPI_WriteComm(0xB000);SPI_WriteData(0x00);
	SPI_WriteComm(0xB001);SPI_WriteData(0x11);
	SPI_WriteComm(0xB002);SPI_WriteData(0x18);
	SPI_WriteComm(0xB003);SPI_WriteData(0x0D);
	SPI_WriteComm(0xB004);SPI_WriteData(0x11);
	SPI_WriteComm(0xB005);SPI_WriteData(0x06);
	SPI_WriteComm(0xB006);SPI_WriteData(0x07);
	SPI_WriteComm(0xB007);SPI_WriteData(0x08);
	SPI_WriteComm(0xB008);SPI_WriteData(0x07);
	SPI_WriteComm(0xB009);SPI_WriteData(0x22);
	SPI_WriteComm(0xB00A);SPI_WriteData(0x04);
	SPI_WriteComm(0xB00B);SPI_WriteData(0x12);
	SPI_WriteComm(0xB00C);SPI_WriteData(0x0F);
	SPI_WriteComm(0xB00D);SPI_WriteData(0xAA);
	SPI_WriteComm(0xB00E);SPI_WriteData(0x31);
	SPI_WriteComm(0xB00F);SPI_WriteData(0x18);

	SPI_WriteComm(0xB100);SPI_WriteData(0x00);
	SPI_WriteComm(0xB101);SPI_WriteData(0x11);
	SPI_WriteComm(0xB102);SPI_WriteData(0x19);
	SPI_WriteComm(0xB103);SPI_WriteData(0x0E);
	SPI_WriteComm(0xB104);SPI_WriteData(0x12);
	SPI_WriteComm(0xB105);SPI_WriteData(0x07);
	SPI_WriteComm(0xB106);SPI_WriteData(0x08);
	SPI_WriteComm(0xB107);SPI_WriteData(0x08);
	SPI_WriteComm(0xB108);SPI_WriteData(0x08);
	SPI_WriteComm(0xB109);SPI_WriteData(0x22);
	SPI_WriteComm(0xB10A);SPI_WriteData(0x04);
	SPI_WriteComm(0xB10B);SPI_WriteData(0x11);
	SPI_WriteComm(0xB10C);SPI_WriteData(0x11);
	SPI_WriteComm(0xB10D);SPI_WriteData(0xA9);
	SPI_WriteComm(0xB10E);SPI_WriteData(0x32);
	SPI_WriteComm(0xB10F);SPI_WriteData(0x18);

	SPI_WriteComm(0xFF00);SPI_WriteData(0x77);
	SPI_WriteComm(0xFF01);SPI_WriteData(0x01);
	SPI_WriteComm(0xFF02);SPI_WriteData(0x00);
	SPI_WriteComm(0xFF03);SPI_WriteData(0x00);
	SPI_WriteComm(0xFF04);SPI_WriteData(0x11);

	SPI_WriteComm(0xB000);SPI_WriteData(0x60);

	SPI_WriteComm(0xB100);SPI_WriteData(0x30);

	SPI_WriteComm(0xB200);SPI_WriteData(0x87);

	SPI_WriteComm(0xB300);SPI_WriteData(0x80);

	SPI_WriteComm(0xB500);SPI_WriteData(0x49);

	SPI_WriteComm(0xB700);SPI_WriteData(0x85);

	SPI_WriteComm(0xB800);SPI_WriteData(0x21);

	//SPI_WriteComm(0xB900);SPI_WriteData(0x10);

	SPI_WriteComm(0xC100);SPI_WriteData(0x78);

	SPI_WriteComm(0xC200);SPI_WriteData(0x78);

	Delay(100);
	//---------------------------------------------GIP Setting----------------------------------------------------//
	SPI_WriteComm(0xE000);SPI_WriteData(0x00);
	SPI_WriteComm(0xE001);SPI_WriteData(0x1B);
	SPI_WriteComm(0xE002);SPI_WriteData(0x02);
	
	SPI_WriteComm(0xE100);SPI_WriteData(0x08);
	SPI_WriteComm(0xE101);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE102);SPI_WriteData(0x00);
	SPI_WriteComm(0xE103);SPI_WriteData(0x00);
	SPI_WriteComm(0xE104);SPI_WriteData(0x07);
	SPI_WriteComm(0xE105);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE106);SPI_WriteData(0x00);
	SPI_WriteComm(0xE107);SPI_WriteData(0x00);
	SPI_WriteComm(0xE108);SPI_WriteData(0x00);
	SPI_WriteComm(0xE109);SPI_WriteData(0x44);
	SPI_WriteComm(0xE10A);SPI_WriteData(0x44);

	
	SPI_WriteComm(0xE200);SPI_WriteData(0x11);
	SPI_WriteComm(0xE201);SPI_WriteData(0x11);
	SPI_WriteComm(0xE202);SPI_WriteData(0x44);
	SPI_WriteComm(0xE203);SPI_WriteData(0x44);
	SPI_WriteComm(0xE204);SPI_WriteData(0xED);
	SPI_WriteComm(0xE205);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE206);SPI_WriteData(0x00);
	SPI_WriteComm(0xE207);SPI_WriteData(0x00);
	SPI_WriteComm(0xE208);SPI_WriteData(0xEC);
	SPI_WriteComm(0xE209);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE20A);SPI_WriteData(0x00);
	SPI_WriteComm(0xE20B);SPI_WriteData(0x00);
	//SPI_WriteComm(0xE20C);SPI_WriteData(0x00);
	 //**********************************//
	SPI_WriteComm(0xE300);SPI_WriteData(0x00);
	SPI_WriteComm(0xE301);SPI_WriteData(0x00);
	SPI_WriteComm(0xE302);SPI_WriteData(0x11);
	SPI_WriteComm(0xE303);SPI_WriteData(0x11);
	
	SPI_WriteComm(0xE400);SPI_WriteData(0x44);
	SPI_WriteComm(0xE401);SPI_WriteData(0x44);
	
	SPI_WriteComm(0xE500);SPI_WriteData(0x0A);
	SPI_WriteComm(0xE501);SPI_WriteData(0xE9);
	SPI_WriteComm(0xE502);SPI_WriteData(0xD8);
	SPI_WriteComm(0xE503);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE504);SPI_WriteData(0x0C);
	SPI_WriteComm(0xE505);SPI_WriteData(0xEB);
	SPI_WriteComm(0xE506);SPI_WriteData(0xD8);
	SPI_WriteComm(0xE507);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE508);SPI_WriteData(0x0E);
	SPI_WriteComm(0xE509);SPI_WriteData(0xED);
	SPI_WriteComm(0xE50A);SPI_WriteData(0xD8);
	SPI_WriteComm(0xE50B);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE50C);SPI_WriteData(0x10);
	SPI_WriteComm(0xE50D);SPI_WriteData(0xEF);
	SPI_WriteComm(0xE50E);SPI_WriteData(0xD8);
	SPI_WriteComm(0xE50F);SPI_WriteData(0xA0);
	
	SPI_WriteComm(0xE600);SPI_WriteData(0x00);
	SPI_WriteComm(0xE601);SPI_WriteData(0x00);
	SPI_WriteComm(0xE602);SPI_WriteData(0x11);
	SPI_WriteComm(0xE603);SPI_WriteData(0x11);
	
	SPI_WriteComm(0xE700);SPI_WriteData(0x44);
	SPI_WriteComm(0xE701);SPI_WriteData(0x44);
	
	SPI_WriteComm(0xE800);SPI_WriteData(0x09);
	SPI_WriteComm(0xE801);SPI_WriteData(0xE8);
	SPI_WriteComm(0xE802);SPI_WriteData(0xD8);
	SPI_WriteComm(0xE803);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE804);SPI_WriteData(0x0B);
	SPI_WriteComm(0xE805);SPI_WriteData(0xEA);
	SPI_WriteComm(0xE806);SPI_WriteData(0xD8);
	SPI_WriteComm(0xE807);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE808);SPI_WriteData(0x0D);
	SPI_WriteComm(0xE809);SPI_WriteData(0xEC);
	SPI_WriteComm(0xE80A);SPI_WriteData(0xD8);
	SPI_WriteComm(0xE80B);SPI_WriteData(0xA0);
	SPI_WriteComm(0xE80C);SPI_WriteData(0x0F);
	SPI_WriteComm(0xE80D);SPI_WriteData(0xEE);
	SPI_WriteComm(0xE80E);SPI_WriteData(0xD8);
	SPI_WriteComm(0xE80F);SPI_WriteData(0xA0);
	
	SPI_WriteComm(0xEB00);SPI_WriteData(0x02);
	SPI_WriteComm(0xEB01);SPI_WriteData(0x00);
	SPI_WriteComm(0xEB02);SPI_WriteData(0xE4);
	SPI_WriteComm(0xEB03);SPI_WriteData(0xE4);
	SPI_WriteComm(0xEB04);SPI_WriteData(0x88);
	SPI_WriteComm(0xEB05);SPI_WriteData(0x00);
	SPI_WriteComm(0xEB06);SPI_WriteData(0x40);
	
	SPI_WriteComm(0xEC00);SPI_WriteData(0x3C);
	SPI_WriteComm(0xEC01);SPI_WriteData(0x00);
	
	SPI_WriteComm(0xED00);SPI_WriteData(0xAB);
	SPI_WriteComm(0xED01);SPI_WriteData(0x89);
	SPI_WriteComm(0xED02);SPI_WriteData(0x76);
	SPI_WriteComm(0xED03);SPI_WriteData(0x54);
	SPI_WriteComm(0xED04);SPI_WriteData(0x02);
	SPI_WriteComm(0xED05);SPI_WriteData(0xFF);
	SPI_WriteComm(0xED06);SPI_WriteData(0xFF);
	SPI_WriteComm(0xED07);SPI_WriteData(0xFF);
	SPI_WriteComm(0xED08);SPI_WriteData(0xFF);
	SPI_WriteComm(0xED09);SPI_WriteData(0xFF);
	SPI_WriteComm(0xED0A);SPI_WriteData(0xFF);
	SPI_WriteComm(0xED0B);SPI_WriteData(0x20);
	SPI_WriteComm(0xED0C);SPI_WriteData(0x45);
	SPI_WriteComm(0xED0D);SPI_WriteData(0x67);
	SPI_WriteComm(0xED0E);SPI_WriteData(0x98);
	SPI_WriteComm(0xED0F);SPI_WriteData(0xBA);
	//--------------------------------------------End GIP Setting-----------------------------------------------//
	//------------------------------ Power Control Registers Initial End-----------------------------------//
	//------------------------------------------Bank1 Setting----------------------------------------------------//
	SPI_WriteComm(0xFF00);SPI_WriteData(0x77);
	SPI_WriteComm(0xFF01);SPI_WriteData(0x01);
	SPI_WriteComm(0xFF02);SPI_WriteData(0x00);
	SPI_WriteComm(0xFF03);SPI_WriteData(0x00);
	SPI_WriteComm(0xFF04);SPI_WriteData(0x00);
	

	SPI_WriteComm(0x3A00);SPI_WriteData(0x66);
	SPI_WriteComm(0x3600);SPI_WriteData(0x00);	//08
	
	SPI_WriteComm(0x1100);
	Delay(500);
	SPI_WriteComm(0x2900);
	Delay(50);

}