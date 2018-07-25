#ifndef _QUEUE_H_
#define _QUEUE_H_

typedef	struct 
{
	unsigned char* pStart;	
	int BuffSize;
	int Head;
	int End;
}QUEUE;


/*------------------------------------------------------------------------
void QueueMemCopy(unsigned char *pDes, unsigned char *pSrc, int Size)

  Queue memory copy. 
					   
Input Arguments:-
	pDes :The pointer of destination.
	pSrc :The pointer of source.
	Size :The size of bytes
 Return:-
	None. 
			
------------------------------------------------------------------------*/

void QueueMemCopy(unsigned char *pDes, unsigned char *pSrc, int Size);
/*------------------------------------------------------------------------
int QueueInit(QUEUE *pQueue, unsigned char* pBuff, int BuffSize)

  Initialize the queue. 
					   
Input Arguments:-
	pQueue : The pointer of queue.	
	pBuff : The pointer of queue buff.
	Buffsize : The size of queue buff.
 Return:-
	0 means initializing successfully.-1 means initializing faultly. 
			
------------------------------------------------------------------------*/

int QueueInit(QUEUE *pQueue, unsigned char* pBuff, int BuffSize);
/*------------------------------------------------------------------------
int QueueDataSize(QUEUE *pQueue)

  The data size of queue. 
					   
Input Arguments:-
	pQueue : The pointer of queue.			
 Return:-
	The size of data. 
			
------------------------------------------------------------------------*/

int QueueDataSize(QUEUE *pQueue);
/*------------------------------------------------------------------------
int QueueBuffSize(QUEUE *pQueue)

  Get the buff size of the queue. 
					   
Input Arguments:-
	pQueue : The pointer of queue.			
 Return:-
	Buff size. 
			
------------------------------------------------------------------------*/

int QueueBuffSize(QUEUE *pQueue);
/*------------------------------------------------------------------------
int QueueEmpty(QUEUE *pQueue)

  Check if the queue is empty. 
					   
Input Arguments:-
	pQueue : The pointer of queue.			
 Return:-
	1 means queue is empty.-1 means queue having not initialized. 
			
------------------------------------------------------------------------*/

int QueueEmpty(QUEUE *pQueue);
/*------------------------------------------------------------------------
int QueueFull(QUEUE *pQueue)

  Check if the queue is full. 
					   
Input Arguments:-
	pQueue : The pointer of queue.			
 Return:-
	1 means queue is full.-1 means queue having not initialized.0 means not full. 
			
------------------------------------------------------------------------*/

int QueueFull(QUEUE *pQueue);
/*------------------------------------------------------------------------
int QueueRead(QUEUE *pQueue, unsigned char* pData, int Size)

  Read data from the queue. 
					   
Input Arguments:-
	pQueue : The pointer of queue.
	pData :The pointer of buff for storing the reading data.
	Size : The bytes of reading data.
 Return:-
	The size reading data . 
			
------------------------------------------------------------------------*/

int QueueRead(QUEUE *pQueue, unsigned char* pData, int Size);
/*------------------------------------------------------------------------
int QueueWrite(QUEUE *pQueue, unsigned char* pData, const int Size)

  Write data to the queue. 
					   
Input Arguments:-
	pQueue : The pointer of queue.
	pData :The pointer of buff for writing to queue.
	Size : The bytes of writing data.
 Return:-
	The size writing data . 
			
------------------------------------------------------------------------*/

int QueueWrite(QUEUE *pQueue, unsigned char* pData, const int Size);
/*------------------------------------------------------------------------
int QueueByteAccess(QUEUE *pQueue, int Index)

  Access data in the queue. 
					   
Input Arguments:-
	pQueue : The pointer of queue.
	Index : The index of data in the queue.
 Return:-
	The access data . 
			
------------------------------------------------------------------------*/

int QueueByteAccess(QUEUE *pQueue, int Index); 

/*------------------------------------------------------------------------
int QueueDelete(QUEUE *pQueue, int Size)

  Delete data in the queue. 
					   
Input Arguments:-
	pQueue : The pointer of queue.
	Size : The size of data for deleting.
 Return:-
	0 means deleting data successfully. -1 means fault. 
			
------------------------------------------------------------------------*/

int QueueDelete(QUEUE *pQueue, int Size); 



#endif
