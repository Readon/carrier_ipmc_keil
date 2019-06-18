/***********************************************Copyright (c)*********************************************
**                                BeiJing Shiyuan Telecom Technology Co.,LTD.
**
**                                       http://www.zlgmcu.com
**
**--------------File Info---------------------------------------------------------------------------------
** File name:			    IIC.c
** Last modified Date:      2007-10-15
** Last Version:		    1.0
** Descriptions:		    I2C����ʵ�֣������豸������������
**
**--------------------------------------------------------------------------------------------------------
** Created by:			    lixiaocheng
** Created date:		    2007-10-15
** Version:				    1.0
** Descriptions:		    ��
**
**--------------------------------------------------------------------------------------------------------
** Modified by:			    wangxiumei
** Modified Date:		    2011-12-22
** Version:				    1.1
** Descriptions:		    ������I2C��UCOSII����ϵͳ�ļ���
**
*********************************************************************************************************/
#include "config.h"
#include "i2cPrivate.h"
#include "i2c.h"
#include "app_cfg.h"

//#define I2CMBOX


/*********************************************************************************************************
   �������I2C�����Ľṹ�壬�ж����������Ҫ��������ṹ��
*********************************************************************************************************/
static __I2C_INFO   __I2C0Data;
static __I2C_INFO   __I2C1Data;
static __I2C_INFO   __I2C2Data;

uint8 DataBuffer0[DATA_BUFFER_SIZE];
uint8 DataBuffer1[DATA_BUFFER_SIZE];
uint8 DataBuffer2[DATA_BUFFER_SIZE];

I2c_MemCopy_FiFo g_I2C_MemCpy_fifo; 	//used as mem copy mailbox data

/*********************************************************************************************************
   �ǲ���ϵͳ�������ź���
*********************************************************************************************************/
#if __UCOSII_EN > 0
    OS_EVENT       *GposeI2c0Sem;
    OS_EVENT       *GposeI2c1Sem;
    OS_EVENT       *GposeI2c2Sem;
    OS_EVENT      **GpposeI2cTable[__IIC_MAX_NUM] = {&GposeI2c0Sem, &GposeI2c1Sem, &GposeI2c2Sem};
#endif

#ifdef I2CMBOX
	OS_EVENT		*I2C0Mbox;
	OS_EVENT		*I2C1Mbox;
	OS_EVENT		*I2C2Mbox;
	OS_EVENT		**I2CMboxTable[__IIC_MAX_NUM] = {&I2C0Mbox, &I2C1Mbox, &I2C2Mbox};

#endif
/*********************************************************************************************************
   ����ʹ��ָ������������ṹ��ָ��,����Ը���������չ
*********************************************************************************************************/
const __PI2C_INFO   __GpiinfoDateTab[__IIC_MAX_NUM] = {&__I2C0Data, &__I2C1Data, &__I2C2Data};

/*********************************************************************************************************
   ���涨����I2C0����ֵַ������ж��I2C���������ڸ�λ�������Ӧ�Ļ���ַ����
*********************************************************************************************************/
const uint32        __GuiI2cBaseAddrTab[__IIC_MAX_NUM] = {I2C0_BASE_ADDR, I2C1_BASE_ADDR, I2C2_BASE_ADDR};

#ifdef INFO_COLLECT
struct info_collect i2cInfo[3];
#endif

extern OS_EVENT *I2c_Q;
extern void UART0_SendStr(char *);

#ifdef INFO_COLLECT
void initI2cInfo(uint8 id)
{
	memset(&i2cInfo[id], 0, sizeof(struct info_collect));
}

void getI2cInfo(uint8 *data, uint8 *bytes) {

	*bytes = 3*sizeof(struct info_collect);
	memcpy(data, i2cInfo, *bytes);
}

#endif

I2c_MemCopy_Entry * get_I2c_mailbox_buf(void)
{
	I2c_MemCopy_Entry *ptr = &(g_I2C_MemCpy_fifo.entry[g_I2C_MemCpy_fifo.index++]);
	if(g_I2C_MemCpy_fifo.index >= I2C_MEM_COPY_ENTRY_NUM)
		g_I2C_MemCpy_fifo.index = 0;
	return ptr;	
}

/*********************************************************************************************************
** Function name:           endBus
** Descriptions:            ���ߴ���������ߣ��ĺ�����__i2cISR����
** Input parameters:        Parm--I2C�豸�������ṹ��ָ��
** Output parameters:       NONE
** Returned value:          NONE
*********************************************************************************************************/
static void __endBus (__PI2C_INFO Parm)
{
	            uint32				uiOffBase;
	volatile 	uint8 	           *pucAddrBase;

	pucAddrBase	= Parm->pucAddrBase;
	uiOffBase   = Parm->uiOffBase;

    Parm->ucIICflag     = I2C_ERR;
	Parm->ucSlave   = __IIC_MASTER;


	pucAddrBase[__B_IIC_SET << uiOffBase] = 0x14;                     /*  ���߳�����������.STO      */
	pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x28;                       /*  ���߳�������STA,SI     */

}
/* I2C��Ϊ����ʱ����ȷ��������������ʱ����������ǰ�����ʹ˱�־����д���� */
static void __retAck (__PI2C_INFO Parm)
{
	#ifdef I2CMBOX
		Parm->i2cintendflag = 1;
		OSMboxPost(*I2CMboxTable[Parm->uiID], (void *)&Parm->i2cintendflag);
	#endif
}
/* I2C��Ϊ����ʱ��������SLA+W/R���յ�NACK����������ǰ�����ʹ˱�־����д���� */
static void __retNack (__PI2C_INFO Parm)
{
	#ifdef I2CMBOX
		Parm->i2cintendflag = 2;
		OSMboxPost(*I2CMboxTable[Parm->uiID], (void *)&Parm->i2cintendflag);
	#endif
}

/* I2C��Ϊ����ʱ������һ�����ݺ󣬶�ʧ���ߣ���������ǰ�����ʹ˱�־����д���� */
static void __retLossBus (__PI2C_INFO Parm)
{
	#ifdef I2CMBOX
		Parm->i2cintendflag = 3;
		OSMboxPost(*I2CMboxTable[Parm->uiID], (void *)&Parm->i2cintendflag);
	#endif
}

/* I2C�յ�����״̬��־ʱ����������ǰ�����ʹ˱�Ǹ���д���� */
static void __retOtherStatus (__PI2C_INFO Parm)
{
	#ifdef I2CMBOX
		Parm->i2cintendflag = 4;
		OSMboxPost(*I2CMboxTable[Parm->uiID], (void *)&Parm->i2cintendflag);
	#endif
}
/*********************************************************************************************************
** Function name:           AddrWrite
** Descriptions:            д��ӻ���ַ���ĺ�����__i2cISR����
** Input parameters:        Parm--I2C�豸�������ṹ��ָ��
** Output parameters:       NONE
** Returned value:          NONE
*********************************************************************************************************/
static void __AddrWrite (__PI2C_INFO Parm)
{
	            uint32				uiOffBase;
	volatile 	uint8 	           *pucAddrBase;

	pucAddrBase	= Parm->pucAddrBase;
	uiOffBase   = Parm->uiOffBase;

	pucAddrBase[__B_IIC_DAT << uiOffBase] = (uint8)(Parm->ucSLAddr);

	pucAddrBase[__B_IIC_SET << uiOffBase]     = 0x04;
	pucAddrBase[__B_IIC_CLR << uiOffBase]     = 0x28;                   /*  ����жϱ�־                */
}

/*********************************************************************************************************
** Function name:           dateWrite
** Descriptions:            д�����ݣ��ĺ�����__subAddrWrite����
** Input parameters:        Parm--I2C�豸�������ṹ��ָ��
** Output parameters:       NONE
** Returned value:          NONE
*********************************************************************************************************/
static void __dataWrite (__PI2C_INFO Parm)
{
	            uint32				uiOffBase;
	volatile 	uint8 	           *pucAddrBase;

	pucAddrBase	= Parm->pucAddrBase;
	uiOffBase   = Parm->uiOffBase;

	if (Parm->usDataMasterNum > 0) {
		pucAddrBase[__B_IIC_DAT << uiOffBase] = Parm->pucDataMasterBuf[Parm->usMasterCounter++];
		Parm->usDataMasterNum--;
		pucAddrBase[__B_IIC_SET << uiOffBase] = 0x04;
		pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;                   /*  ����SI               */

	} else {
		Parm->ucIICflag = I2C_MASTER_WRITE_END;						        /*  �������߽�����־            */

		__retAck(Parm);

		pucAddrBase[__B_IIC_SET << uiOffBase] = 0x14;                   /*  �����ݷ��ͽ�������          */
		pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;

	}
}

/*********************************************************************************************************
** Function name:           __dataMasterRead
** Descriptions:            I2C��������
** Input parameters:        Parm--I2C�豸�������ṹ��ָ��
** Output parameters:       NONE
** Returned value:          NONE
*********************************************************************************************************/
static void __dataMasterRead(__PI2C_INFO Parm)
{
	uint32				uiOffBase;
	volatile 	uint8 	           *pucAddrBase;

	pucAddrBase	= Parm->pucAddrBase;
	uiOffBase   = Parm->uiOffBase;

	if (Parm->usMasterCounter < (Parm->usDataMasterNum - 1)) {
		Parm->pucDataMasterBuf[Parm->usMasterCounter] = pucAddrBase[__B_IIC_DAT << uiOffBase];

		if(Parm->usMasterCounter == (Parm->usDataMasterNum - 2)) {
			Parm->usMasterCounter++;
			pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x04;
			pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;                   /*  ����SI ,AA      */
		} else {
			Parm->usMasterCounter++;
			pucAddrBase[__B_IIC_SET << uiOffBase] = 0x04;
			pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;                   /*  ����SI          */
		}

	} else {
		Parm->pucDataMasterBuf[Parm->usMasterCounter] = pucAddrBase[__B_IIC_DAT << uiOffBase];

        UART0_Printf("i2c MRead data is %02x\n", pucAddrBase[__B_IIC_DAT << uiOffBase]);

		Parm->ucIICflag = I2C_MASTER_READ_END;						        /*  �������߽�����־            */

  		__retAck(Parm);

		pucAddrBase[__B_IIC_SET << uiOffBase] = 0x14;                   /*  �����ݽ��ս�������          */
		pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;

	}
}

/*********************************************************************************************************
** Function name:           __dateReadEnd
** Descriptions:            д�����ݣ��ĺ�����__subAddrWrite����
** Input parameters:        Parm--I2C�豸�������ṹ��ָ��
** Output parameters:       NONE
** Returned value:          NONE
*********************************************************************************************************/
static uint8 __dataReadEnd (__PI2C_INFO Parm)
{
	uint8 err;
	I2c_MemCopy_Entry *I2cMem_PtrGet;

	if (Parm->i2cSlaveErrFlag == 1) {
		return 1;
	}
	I2cMem_PtrGet = get_I2c_mailbox_buf();
	if(Parm->usDataSlaveNum > I2C_MEMCPY_BUF_SIZE)
		I2cMem_PtrGet->len = I2C_MEMCPY_BUF_SIZE;
	else 
		I2cMem_PtrGet->len = Parm->usDataSlaveNum;
	I2cMem_PtrGet->i2cID = Parm->uiID;

	memcpy(I2cMem_PtrGet->databuf, Parm->pucDataSlaveBuf, I2cMem_PtrGet->len);

	err = OSQPost(I2c_Q, I2cMem_PtrGet);

	#ifdef INFO_COLLECT
	i2cInfo[Parm->uiID].rxTLLCnt++;
	#endif
	return err;
}
/*********************************************************************************************************
** Function name:           ISR_I2C
** Descriptions:
** Input parameters:
** Output parameters:       NONE
** Returned value:          NONE
*********************************************************************************************************/
static void __i2cISR (__PI2C_INFO Parm)
{
	            uint8				ucSta;
	            uint32				uiOffBase;
	volatile 	uint8 	           *pucAddrBase;

	pucAddrBase	= Parm->pucAddrBase;
	uiOffBase   = Parm->uiOffBase;

	ucSta = pucAddrBase[__B_IIC_STAT << uiOffBase];                     /*  ��ȡI2C״̬��               */
	//UART0_Printf("i2c status reg:%x\n",ucSta);
	switch(ucSta) {

    case __SEND_START:                                                  /*  �ѷ�����ʼ����,����д����   */
	case __SEND_RESTART:                                                /*  �����������ߺ󣬷��ʹӻ���ַ*/

		__AddrWrite ( Parm );
		break;

	case __SEND_SLA_W_ACK:												/*  �ѷ���SLA+W���Ѿ�����Ӧ��    */
	case __SEND_DATA_ACK:                                               /*  �������ݣ��ѽ���Ӧ��         */
	                                                                    /*  �����������ʹ�����溯��     */
        __dataWrite( Parm );                                            /*  I2Cд���ݺ͵�ַ��һ��        */

		break;

	case __SEND_SLA_R_ACK:
		pucAddrBase[__B_IIC_SET << uiOffBase] = 0x04;
		pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;	/* clear SI */
		break;

	case __RECV_DATA_ACK:
	case __RECV_DATA_NOACK:

		__dataMasterRead(Parm);
		break;

	case __SEND_SLA_W_NOACK:                                            /*  ����SLA_W,�յ���Ӧ��         */
	case __SEND_SLA_R_NOACK:											/*  ����SLA_R,�յ���Ӧ��		 */
		__retNack (Parm);
		__endBus(Parm);
		break;
	case __LOSS_BUS:                                                    /*  ��ʧ�ٲ�                     */
	case __SEND_DATA_NOACK:                                             /*  �������ݣ��յ���Ӧ��         */
	    __retLossBus (Parm);                                            /*  �������������Ҫ��������     */
	    __endBus( Parm );                                               /*  ��������                     */
		break;

	case __IICSTAT_SR_SLA_START:
	case __IICSTAT_SR_SLA_START2:
	case __IICSTAT_SR_ALL_START:
	case __IICSTAT_SR_ALL_START2:
			Parm->ucIICflag = I2C_BUSY;
			Parm->ucSlave   = __IIC_SLAVER;
			Parm->usDataSlaveNum = 0;
			Parm->i2cSlaveErrFlag = 0;
			Parm->pucDataSlaveBuf[Parm->usDataSlaveNum] = pucAddrBase[__B_IIC_ADR << uiOffBase];
			Parm->usDataSlaveNum++;
			pucAddrBase[__B_IIC_SET << uiOffBase] = 0x04;
			pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;	/* clear SI */

		break;

	case __IICSTAT_SR_DATASLA_ACK:
	case __IICSTAT_SR_DATAALL_ACK:
		if(Parm->usDataSlaveNum < DATA_BUFFER_SIZE) {
			Parm->pucDataSlaveBuf[Parm->usDataSlaveNum] = pucAddrBase[__B_IIC_DAT << uiOffBase];
			Parm->usDataSlaveNum++;
		}
		else {
			Parm->i2cSlaveErrFlag = 1;
		}
		pucAddrBase[__B_IIC_SET << uiOffBase] = 0x04;
		pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;	/* clear SI */

		break;

	case __IICSTAT_SR_DATASLA_NOACK:
	case __IICSTAT_SR_DATAALL_NOACK:
		pucAddrBase[__B_IIC_SET << uiOffBase] = 0x04;
		pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;	/* clear SI */

		break;
	case __IICSTAT_SR_STOP:
		__dataReadEnd( Parm );
		Parm->ucIICflag = I2C_SLAVE_READ_END;
		Parm->ucSlave   = __IIC_MASTER; //�л�������ģʽ
		pucAddrBase[__B_IIC_SET << uiOffBase] = 0x04;
		pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x08;	/* clear SI */

		break;
	default:
		__retOtherStatus(Parm);                                         /*  ����״̬      */
        __endBus( Parm );                                               /*  ��������      */
		break;
	}
}

/*********************************************************************************************************
** Function name:           I2CxIRQ
** Descriptions:            i2cx interrupt service
** Input parameters:        NONE
** Output parameters:       NONE
** Returned value:          NONE
*********************************************************************************************************/
void i2c0IRQ ( void )
{
	__i2cISR(__GpiinfoDateTab[0]);
}

void i2c1IRQ ( void )
{
	__i2cISR(__GpiinfoDateTab[1]);
}

void i2c2IRQ ( void )
{
	__i2cISR(__GpiinfoDateTab[2]);
}
/*********************************************************************************************************
** Function name:           I2CInit
** Descriptions:            initial of i2c
** Input parameters:        ID              device id
**							ucSlave			master or slave
**							ucAddr			I2C address
** Output parameters:       NONE
** Returned value:          PIN_ERR
**                          OPERATE_FAIL
**                          OPERATE_SUCCESS
*********************************************************************************************************/

int	i2cInit ( uint32 ID,
                     uint8 ucAddr,
                      uint8 *Buf )
{
	volatile uint8 *pucAddrBase;
	volatile uint32 uiOffBase;

	/*
	 *  ��������,�ж�
	 */
	if ( ID >= __IIC_MAX_NUM ) {
	    return OPERATE_FAIL;
	}

	/*
	 *  �����ź���������ϵͳ����
	 */
#if __UCOSII_EN > 0
    *GpposeI2cTable[ID] = OSSemCreate(1);
    if(*GpposeI2cTable[ID] == (void *)NULL) {                       /*  ��������ź���ʧ�ܷ��ش���  */
        UART0_SendStr("Sem create failed\n");
        return OPERATE_FAIL;
    }
#endif

#ifdef I2CMBOX
 	*I2CMboxTable[ID] = OSMboxCreate((void *)0);
 	if(*I2CMboxTable[ID] == (void *)NULL) {
 		UART0_SendStr("i2c Mbox create failed\n");
 		return OPERATE_FAIL;
 	}
#endif

	/*
	 *  ��ʼ��һЩ����
	 */
	__GpiinfoDateTab[ID]->uiID          = ID;
	__GpiinfoDateTab[ID]->pucAddrBase   = (uint8*)__GuiI2cBaseAddrTab[ID];
	__GpiinfoDateTab[ID]->uiOffBase     = 2;
	__GpiinfoDateTab[ID]->ucIICflag     = I2C_IDLE;                     /*  ��ʶ���������ڿ���          */
	__GpiinfoDateTab[ID]->ucSlave       = __IIC_MASTER;                 /*  ��ʶ������Ϊ�ӻ�            */
	__GpiinfoDateTab[ID]->usDataMasterNum    = 0;
	__GpiinfoDateTab[ID]->usDataSlaveNum     = 0;
	__GpiinfoDateTab[ID]->usMasterCounter    = 0;

#ifdef I2CMBOX
	__GpiinfoDateTab[ID]->i2cintendflag     = 0;
#endif


	pucAddrBase = __GpiinfoDateTab[ID]->pucAddrBase;	                /*  ��ȡָ�����                */
	uiOffBase   = __GpiinfoDateTab[ID]->uiOffBase;

	*(uint16*)(pucAddrBase+(__B_IIC_SCLH << uiOffBase) ) = 100;
	                                                                    /*  ����ʱ�Ӹߵ�ƽʱ��          */

	*(uint16*)(pucAddrBase+(__B_IIC_SCLL << uiOffBase) ) = 100;
	                                                                    /*  ����ʱ�ӵ͵�ƽʱ��          */
    *(uint8*)(pucAddrBase+(__B_IIC_ADR << uiOffBase) ) = (ucAddr);        /*  ����I2C��ַ,δʹ��ͨ�õ��õ�ַ */

    __GpiinfoDateTab[ID]->pucDataSlaveBuf  = Buf;					/*  �������ݵĻ�����            */
    __GpiinfoDateTab[ID]->pucDataMasterBuf = NULL;
    pucAddrBase[__B_IIC_CLR << uiOffBase]  = 0x28;
    pucAddrBase[__B_IIC_SET << uiOffBase]  = 0x44;                      /*  ���ô�ģʽ 	*/

#ifdef INFO_COLLECT
    initI2cInfo(ID);
#endif

    return OPERATE_SUCCESS;
}


/*********************************************************************************************************
** Function name:           i2cSetMode
** Descriptions:            ����I2C�ٶȣ���Ҫ���ڳ�ʼ�����û���ı�I2C��������ʱ�ú���ֻ������������ģʽ���ٶȡ�
**                          ���д���ٶȴ���400K�������ó�ϵͳĬ�ϵ�300K
** Input parameters:        ID   �������豸��,����ID=0,��ʾ�������豸��I2C0
**				            speed	����I2C�ٶ�
**				 	        ucSlave  ����I2C����ģʽ
**				            Rsv	 ��������
** Output parameters:       NONE
** Returned value:          OPERATE_FAIL    ����ʧ��
**                          OPERATE_SUCCESS ���óɹ�
*********************************************************************************************************/
int32 i2cSetMode (uint32   ID,
				   uint32	speed,
                   uint8   ucSlave)
{
	volatile uint8     *pucAddrBase;
	volatile uint32     uiOffBase;

	pucAddrBase = __GpiinfoDateTab[ID]->pucAddrBase;
	uiOffBase   = __GpiinfoDateTab[ID]->uiOffBase;

	if (ID < __IIC_MAX_NUM) {

		/*
		 *  ��������
		 */
		if (speed > 400000) {
		    speed = 300000;
		}

    	__I2C_LOCK(*GpposeI2cTable[ID]);                                /*  ����I2C��Դ                 */

		                                                                /*  �����ٶ�                    */
		*(uint16*)(pucAddrBase + (__B_IIC_SCLH << uiOffBase)) = (uint16)((Fpclk / speed) / 2);
		*(uint16*)(pucAddrBase + (__B_IIC_SCLL << uiOffBase)) = (uint16)((Fpclk / speed) / 2);
		__GpiinfoDateTab[ID]->ucSlave       = ucSlave;

        __I2C_UNLOCK(*GpposeI2cTable[ID]);                              /*  �ͷ�I2C��Դ                 */

		return OPERATE_SUCCESS;
	}
	return OPERATE_FAIL;
}

/*********************************************************************************************************
** Function name:           __i2cStart
** Descriptions:            ����I2C����
** Input parameters:        ID   �������豸��,����ID=0,��ʾ�������豸��I2C0
** Output parameters:       NONE
** Returned value:          NONE
*********************************************************************************************************/
static uint8 __i2cStart (uint32 ID)
{
	volatile uint8     *pucAddrBase;
	volatile uint32     uiOffBase;

	pucAddrBase = __GpiinfoDateTab[ID]->pucAddrBase;
	uiOffBase   = __GpiinfoDateTab[ID]->uiOffBase;

	if (__GpiinfoDateTab[ID]->ucIICflag == I2C_BUSY) {
		    return OPERATE_FAIL;
	}
	__GpiinfoDateTab[ID]->ucIICflag = I2C_BUSY;

	pucAddrBase[__B_IIC_CLR << uiOffBase] = 0x28;                       /*  ��������,����Ϊ����         */
	pucAddrBase[__B_IIC_SET << uiOffBase] = 0x64;

	return OPERATE_SUCCESS;
}


/*********************************************************************************************************
** Function name:           i2cWrite
** Descriptions:
** Input parameters:        ID			   : which i2c device
**                          Buf 		   : pointer to memory which to write
**                          Nbyte          : write lengh
** Output parameters:       NONE
** Returned value:          success        : OPERATE_SUCCESS
**                          fail           : OPERATE_FAIL
*********************************************************************************************************/

int32 i2cWrite (uint32  ID,
				uint8	Addr,
                uint8  *Buf,
                uint16  Nbyte)
{
#ifdef I2CMBOX
	uint8 err;
	uint8 i2cflag;
#endif

#ifdef I2CPOLLEND
	uint8 j;
#endif
	uint8 i2cbuf[64];

	if (Nbyte == 0) {
	    return I2C_RET_ERR;                 /* Can't no write (0 byte is wrong)*/
	}

    if(Nbyte > DATA_BUFFER_SIZE) {
		return I2C_RET_ERR;
	}

	if (ID < __IIC_MAX_NUM) {
         __I2C_LOCK(*GpposeI2cTable[ID]);   /* Apply semaphore */

		memset(i2cbuf, 0, sizeof(i2cbuf));
		memcpy(i2cbuf, Buf, Nbyte);
    	__GpiinfoDateTab[ID]->ucSLAddr        	= (uint8)(Addr & 0xfe); /* Slave address */
		__GpiinfoDateTab[ID]->pucDataMasterBuf  = i2cbuf;               /* Write buffer */
		__GpiinfoDateTab[ID]->usDataMasterNum   = Nbyte;                /* Num of write */
		__GpiinfoDateTab[ID]->usMasterCounter 	= 0;
#ifdef I2CMBOX
		__GpiinfoDateTab[ID]->i2cintendflag  	= 0;
		i2cflag = 0;
#endif

		if(__i2cStart(ID) == OPERATE_FAIL) {
			__I2C_UNLOCK(*GpposeI2cTable[ID]);
			return I2C_RET_BUS_BUSY;
		}

#ifdef I2CMBOX
		i2cflag = *(uint8 *)OSMboxPend(*I2CMboxTable[ID], I2C_SEM_GET_TIMEOUT, &err);
#ifdef INFO_COLLECT
		i2cInfo[ID].txTLLCnt++;
#endif
#ifdef TESTI2C
		tcp.txTotal++;
#endif
		if (err == OS_ERR_NONE) {
			if (i2cflag == 1) {
				#ifdef INFO_COLLECT
				i2cInfo[ID].txRCnt++;
				#endif
				#ifdef TESTI2C
				tcp.txRcnt++;
				#endif
				__I2C_UNLOCK(*GpposeI2cTable[ID]);
				return I2C_RET_SUCCESS;
			}
			else if (i2cflag == 2) {
				__I2C_UNLOCK(*GpposeI2cTable[ID]);
				return I2C_RET_NO_PARTNER;
			}
			else if (i2cflag == 3) {
				__I2C_UNLOCK(*GpposeI2cTable[ID]);
				return I2C_RET_LOSS_BUS;
			}
			else {
				__I2C_UNLOCK(*GpposeI2cTable[ID]);
				return I2C_RET_ERR;
			}
		}
		else if (err == OS_ERR_TIMEOUT) {
			__endBus(__GpiinfoDateTab[ID]);
			__I2C_UNLOCK(*GpposeI2cTable[ID]);
			return I2C_RET_TIMEOUT;
		}
		else {
			__I2C_UNLOCK(*GpposeI2cTable[ID]);
			return I2C_RET_ERR;
		}
#endif

	}
	return I2C_RET_ERR;
}


int32 i2cMasterRead(uint32 ID, uint8 Addr, uint8 *Buf, uint16 Nbyte)
{
#ifdef I2CMBOX
	uint8 err;
	uint8 i2cflag;
#endif

	if(Nbyte == 0) {
		return I2C_RET_ERR;
	}
	if(Nbyte > DATA_BUFFER_SIZE) {
		return I2C_RET_ERR;
	}
	if (ID < __IIC_MAX_NUM) {
		__I2C_LOCK(*GpposeI2cTable[ID]);                                /* Apply semaphore */

		__GpiinfoDateTab[ID]->ucSLAddr = (uint8)((Addr & 0xfe) | 0x01); /* Slave address */
		__GpiinfoDateTab[ID]->pucDataMasterBuf = Buf;                   /* Read buffer */
		__GpiinfoDateTab[ID]->usDataMasterNum = Nbyte;                  /* Num of read */
		__GpiinfoDateTab[ID]->usMasterCounter = 0;
#ifdef I2CMBOX
		__GpiinfoDateTab[ID]->i2cintendflag = 0;
		i2cflag = 0;
#endif
		if(__i2cStart(ID) == OPERATE_FAIL) {
			__I2C_UNLOCK(*GpposeI2cTable[ID]);
			return I2C_RET_BUS_BUSY;
		}

#ifdef I2CMBOX
		i2cflag = *(uint8 *)OSMboxPend(*I2CMboxTable[ID], I2C_SEM_GET_TIMEOUT, &err);
		if (err == OS_ERR_NONE) {
			if (i2cflag == 1) {
				__I2C_UNLOCK(*GpposeI2cTable[ID]);
				return I2C_RET_SUCCESS;
			}
			else if (i2cflag == 2) {
				__I2C_UNLOCK(*GpposeI2cTable[ID]);
				return I2C_RET_NO_PARTNER;
			}
			else if (i2cflag == 3) {
				__I2C_UNLOCK(*GpposeI2cTable[ID]);
				return I2C_RET_LOSS_BUS;
			}
			else {
				__I2C_UNLOCK(*GpposeI2cTable[ID]);
				return I2C_RET_ERR;
			}

		}

		__I2C_UNLOCK(*GpposeI2cTable[ID]);
		return I2C_RET_ERR;
#endif

	}
	return I2C_RET_ERR;
}
/*********************************************************************************************************
** Function name:           i2cGetFlag
** Descriptions:
** Input parameters:        ID             : which i2c device
** Output parameters:       NONE
** Returned value:          success        :
**                                           I2C_IDLE�� idle
**                                           I2C_WRITE_END��write success
**                                           I2C_READ_END��read success
**                                           I2C_ERR��i2c bus error
**                                           I2C_BUSY��i2c bus busy
**                          fail           : OPERATE_FAIL
*********************************************************************************************************/

int32 i2cGetFlag (uint8 ID)
{

	if (ID < __IIC_MAX_NUM) {
		return __GpiinfoDateTab[ID]->ucIICflag;
	}
	return OPERATE_FAIL;
}
uint8	i2cGetHostFlag(uint8 ID)
{
	if (ID < __IIC_MAX_NUM) {
		return __GpiinfoDateTab[ID]->ucSlave;
	}
	return OPERATE_FAIL;
}
uint8 i2cSetFlagIdle (uint8 ID)
{
	if (ID < __IIC_MAX_NUM) {
	    __I2C_LOCK(*GpposeI2cTable[ID]);                /* Apply resource to affirm bus busy or idle*/

		__GpiinfoDateTab[ID]->ucIICflag = I2C_IDLE;
		__GpiinfoDateTab[ID]->ucSlave   = __IIC_MASTER;
		__I2C_UNLOCK(*GpposeI2cTable[ID]);              /* Release I2C resource*/
		return OPERATE_SUCCESS;
	}
	return OPERATE_FAIL;
}

uint8 i2cGetIsrState (uint8 ID)
{
	            uint32				uiOffBase;
	volatile 	uint8 	           *pucAddrBase;

	pucAddrBase	= __GpiinfoDateTab[ID]->pucAddrBase;
	uiOffBase   = __GpiinfoDateTab[ID]->uiOffBase;

	return pucAddrBase[__B_IIC_STAT << uiOffBase];                     /* Get i2c state */
}
/*********************************************************************************************************
** Function name:           i2cGetRemainBytes
** Descriptions:            return i2c have how many bytes not sends
** Input parameters:        ID:which i2c device
** Output parameters:       NONE
** Returned value:          success: return bytes
**                          fail: OPERATE_FAIL
*********************************************************************************************************/
uint32 i2cGetRemainBytes(uint8 ID)
{
    if (ID < __IIC_MAX_NUM) {
        return __GpiinfoDateTab[ID]->usDataMasterNum;
    }
    return OPERATE_FAIL;
}

/*********************************************************************************************************
   END FILE
*********************************************************************************************************/




