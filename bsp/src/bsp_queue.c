#include "bsp_queue.h"
#include "ucos_ii.h"

//����в���һ�ֽ�
void BSP_Queue_Input( pQUEUE q,INT8U dat )
{
#if OS_CRITICAL_METHOD == 3u
    OS_CPU_SR cpu_sr = 0u;
#endif
	OS_ENTER_CRITICAL();
	if (q->cnt < q->size) {
		q->pbuf[q->in] = dat;    //д������
		q->in = (q->in+1) % (q->size);//������ڵ�ַ
		q->cnt++;  //�������ݸ���(�˲������ɱ��ж�)
	} else {
		q->err++;
	}
    OS_EXIT_CRITICAL();
}

//�Ӷ��ж���һ�ֽ�
INT8U BSP_Queue_Output( pQUEUE q )
{
#if OS_CRITICAL_METHOD == 3u
    OS_CPU_SR cpu_sr = 0u;
#endif
	INT8U ret = 0; 
	OS_ENTER_CRITICAL();
	if (q->cnt>0) {
		ret = q->pbuf[q->out];  //������
		q->out = (q->out+1) % (q->size);  //�������ڵ�ַ
		q->cnt--;
	}
    OS_EXIT_CRITICAL();
	return ret;
}

//��ö�������
INT32U BSP_Queue_Get_Size( pQUEUE q )
{
#if OS_CRITICAL_METHOD == 3u
    OS_CPU_SR cpu_sr = 0u;
#endif
	INT32U size;
	OS_ENTER_CRITICAL();
	size = q->size;
    OS_EXIT_CRITICAL();
	return size;
}

//��ö��������ݸ���
INT32U BSP_Queue_Get_Data_Count( pQUEUE q )
{
#if OS_CRITICAL_METHOD == 3u
    OS_CPU_SR cpu_sr = 0u;
#endif
	INT32U cnt;
	OS_ENTER_CRITICAL();
	cnt = q->cnt;
    OS_EXIT_CRITICAL();
	return cnt;
}

INT32U	BSP_Queue_Get_Empty_Count( pQUEUE q )
{
#if OS_CRITICAL_METHOD == 3u
    OS_CPU_SR cpu_sr = 0u;
#endif
	INT32U ept;
	OS_ENTER_CRITICAL();
	ept = (q->size) - (q->cnt);
    OS_EXIT_CRITICAL();
	return ept;
}

//��ն���,ִ��ʱ���ɱ��ж�
void BSP_Queue_Clear( pQUEUE q )
{
#if OS_CRITICAL_METHOD == 3u
    OS_CPU_SR cpu_sr = 0u;
#endif
	OS_ENTER_CRITICAL();
	q->in = 0;
	q->out = 0;
	q->cnt = 0;
	q->err = 0;
    OS_EXIT_CRITICAL();
}

//��ʼ��һ����
void BSP_Queue_Create( pQUEUE q,INT8U *buf,INT32U size )
{
#if OS_CRITICAL_METHOD == 3u
    OS_CPU_SR cpu_sr = 0u;
#endif
	OS_ENTER_CRITICAL();
	q->pbuf = buf;
	q->size = size;
	BSP_Queue_Clear(q);
    OS_EXIT_CRITICAL();
}

