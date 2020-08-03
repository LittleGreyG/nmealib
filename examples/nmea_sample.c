/*
 * Copyright (c) 2006-2020, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2020-07-08     Shine       the first version 
 */

#include <rtthread.h>
#include <rtdevice.h>

#include <string.h>
#include "stdio.h"
#include "string.h"

#define DBG_TAG "nmea"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include "nmea/nmea.h"

/* ���ڽ�����Ϣ�ṹ*/
struct rx_msg
{
    rt_device_t dev;
    rt_size_t size;
};

static rt_device_t serial;
static struct rt_messagequeue rx_mq;

/* �������ݻص����� */
static rt_err_t uart_input(rt_device_t dev, rt_size_t size)
{
    struct rx_msg msg;
    rt_err_t result;
    msg.dev = dev;
    msg.size = size;

    result = rt_mq_send(&rx_mq, &msg, sizeof(msg));
    if (result == -RT_EFULL)
    {
        /* ��Ϣ������ */
        rt_kprintf("message queue full��\n");
    }
    return result;
}

//#define __GPS_DEBUG
/**
 * @brief  trace �ڽ���ʱ��������GPS���
 * @param  str: Ҫ������ַ�����str_size:���ݳ���
 * @retval ��
 */
static void trace(const char *str, int str_size)
{
    rt_kprintf("\nnmea trace:");
    for (int i = 0; i < str_size; i++)
        rt_kprintf("%c", str[i]);
    rt_kprintf("\n");
}

/**
 * @brief  error �ڽ������ʱ�����ʾ��Ϣ
 * @param  str: Ҫ������ַ�����str_size:���ݳ���
 * @retval ��
 */
static void error(const char *str, int str_size)
{
    rt_kprintf("\nnmea error:");
    for (int i = 0; i < str_size; i++)
        rt_kprintf("%c", str[i]);
    rt_kprintf("\n");
}

static void nmea_thread_entry(void *parameter)
{
    struct rx_msg msg;
    rt_err_t result;
    rt_uint32_t rx_length;
    static char rx_buffer[RT_SERIAL_RB_BUFSZ + 1];

    char ss[128];   //��ӡ�ַ���buffer

    double deg_lat; //ת����[degree].[degree]��ʽ��γ��
    double deg_lon; //ת����[degree].[degree]��ʽ�ľ���

    nmeaINFO info;          //GPS�����õ�����Ϣ
    nmeaPARSER parser;      //����ʱʹ�õ����ݽṹ

    nmea_property()->trace_func = &trace;
    nmea_property()->error_func = &error;

    nmea_zero_INFO(&info);
    nmea_parser_init(&parser);

    while (1)
    {
        rt_memset(&msg, 0, sizeof(msg));
        /* ����Ϣ�����ж�ȡ��Ϣ*/
        result = rt_mq_recv(&rx_mq, &msg, sizeof(msg), RT_WAITING_FOREVER);
        if (result == RT_EOK)
        {
            /* �Ӵ��ڶ�ȡ����*/
            rx_length = rt_device_read(msg.dev, 0, rx_buffer, msg.size);
            rx_buffer[rx_length] = '\0';

            nmea_parse(&parser, (const char *) &rx_buffer[0], rx_length, &info);

            //info.lat lon�еĸ�ʽΪ[degree][min].[sec/60]��ʹ�����º���ת����[degree].[degree]��ʽ
            deg_lat = nmea_ndeg2degree(info.lat);
            deg_lon = nmea_ndeg2degree(info.lon);

            LOG_D("utc_time:%d-%02d-%02d,%d:%d:%d ", info.utc.year + 1900, info.utc.mon + 1, info.utc.day,
                    info.utc.hour, info.utc.min, info.utc.sec);
            //��ΪLOG_D��֧�ָ����������Դ˴�ʹ��snprintf���д�ӡ������LOG_D���
            snprintf(ss, 128, "wd:%f,jd:%f", deg_lat, deg_lon);
            LOG_D(ss);
            snprintf(ss, 128, "high:%f m", info.elv);
            LOG_D(ss);
            snprintf(ss, 128, "v:%f km/h", info.speed);
            LOG_D(ss);
            snprintf(ss, 128, "hangxiang:%f du", info.direction);
            LOG_D(ss);
            snprintf(ss, 128, "used GPS:%d,show GPS:%d", info.satinfo.inuse, info.satinfo.inview);
            LOG_D(ss);
            snprintf(ss, 128, "PDOP:%f,HDOP:%f,VDOP:%f", info.PDOP, info.HDOP, info.VDOP);
            LOG_D(ss);

        }
    }
}

static int nmea_thread_init(int argc, char *argv[])
{
    rt_err_t ret = RT_EOK;
    static char msg_pool[256];
    static char up_msg_pool[256];
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;
    /* ���Ҵ����豸 */
    serial = rt_device_find(NMEALIB_UART_PORT);
    /* ��ʼ����Ϣ���� */
    rt_mq_init(&rx_mq, "rx_mq", msg_pool, sizeof(struct rx_msg), sizeof(msg_pool), RT_IPC_FLAG_FIFO);
    //�޸Ĳ�����Ϊ9600
    config.baud_rate = NMEALIB_UART_BAUDRATE;
    rt_device_control(serial, RT_DEVICE_CTRL_CONFIG, &config);

    rt_device_open(serial, RT_DEVICE_FLAG_DMA_RX); /* �� DMA ���ռ���ѯ���ͷ�ʽ�򿪴����豸 */
    rt_device_set_rx_indicate(serial, uart_input); /* ���ý��ջص����� */

    rt_thread_t thread = rt_thread_create("nmea", nmea_thread_entry, RT_NULL, 4096, 25, 10); /* ���� serial �߳� */

    /* �����ɹ��������߳� */
    if (thread != RT_NULL)
    {
        rt_thread_startup(thread);
    }
    else
    {
        ret = RT_ERROR;
    }

    return ret;
}
/* ������ msh �����б��� */
MSH_CMD_EXPORT(nmea_thread_init, nmea thread init);
INIT_APP_EXPORT(nmea_thread_init);
