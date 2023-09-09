
#ifndef __LN_QUEUE_H__
#define __LN_QUEUE_H__

typedef char* QDateType;//���д洢��������

typedef struct QueueNode //����Ԫ�ؽڵ�
{
	QDateType val;
	int len;
	struct QueueNode* next;
}QueueNode;

typedef	struct Queue //����
{
	QueueNode* head;
	QueueNode* tail;
}Queue;


void QueueInti(Queue* pq);
// ���г�ʼ��
void QueueDestory(Queue* pq);
// ���е�����
void QueuePush(Queue* pq, QDateType x,int len);
// ���
void QueuePop(Queue* pq);
// ����
QDateType QueueFront(Queue* pq);
// ȡ������Ԫ��
int QueueSize(Queue* pq);
// ����еĳ���
bool QueueEmpty(Queue* pq);
// �ж϶��Ƿ�Ϊ��


#endif
