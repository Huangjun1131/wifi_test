#include "queue.h"
#include "stdio.h"
 /*-----------------------------------------------------------------  
void QueueMemCopy(unsigned char *pDes, unsigned char *pSrc, int Size)

  See header file for description.

  -----------------------------------------------------------------*/

void QueueMemCopy(unsigned char *pDes, unsigned char *pSrc, int Size)
{
    int i;
    
    for (i = 0; i < Size; i++)
    {
      pDes[i] = pSrc[i];
    }
}


 /*-----------------------------------------------------------------  
int QueueInit(QUEUE *pQueue, unsigned char* pBuff, int BuffSize)

  See header file for description.

  -----------------------------------------------------------------*/

int QueueInit(QUEUE *pQueue, unsigned char* pBuff, int BuffSize)
{
      if((pQueue == NULL) || (pBuff == NULL) || (BuffSize <= 0))
      {
      	return -1;
      }	
      
      //数据初始化
      pQueue->pStart = pBuff;
      pQueue->BuffSize = BuffSize;
      pQueue->Head = 0;
      pQueue->End = 0;
   
      return 0;
}


 /*-----------------------------------------------------------------  
int QueueDataSize(QUEUE *pQueue)

  See header file for description.

  -----------------------------------------------------------------*/

int QueueDataSize(QUEUE *pQueue)
{
    if(pQueue == NULL)
    {
      return 0;
    }
            
    if(QueueEmpty(pQueue))
    {
      return 0;
    }
	
            
    if(pQueue->End < pQueue->Head)
    {
      return pQueue->Head - pQueue->End;
    }		
    else
    {
      return pQueue->BuffSize - (pQueue->End - pQueue->Head);
    }		
}

 /*-----------------------------------------------------------------  
int QueueBuffSize(QUEUE *pQueue)

  See header file for description.

  -----------------------------------------------------------------*/

int QueueBuffSize(QUEUE *pQueue)
{
    if(pQueue == NULL)
    {
      return -1;
    }
            
    return pQueue->BuffSize;
}


 /*-----------------------------------------------------------------  
int QueueEmpty(QUEUE *pQueue)

  See header file for description.

  -----------------------------------------------------------------*/

int QueueEmpty(QUEUE *pQueue)
{
    if (pQueue == NULL)
    {
		return -1;
    }
            
    if (pQueue->End == pQueue->Head)
    {
		return 1;
    }
            
    return 0;
}

 /*-----------------------------------------------------------------  
int QueueFull(QUEUE *pQueue)

  See header file for description.

  -----------------------------------------------------------------*/

int QueueFull(QUEUE *pQueue)
{
    if (pQueue == NULL)
    {
		return -1;
    }
            
    if ((pQueue->End == (pQueue->Head + 1))
		||((pQueue->End == 0) && (pQueue->Head == pQueue->BuffSize-1)))
    {
		return 1;
    }
   
    return 0;
}


 /*-----------------------------------------------------------------  
int QueueRead(QUEUE *pQueue, unsigned char* pData,  int Size)

  See header file for description.

  -----------------------------------------------------------------*/

int QueueRead(QUEUE *pQueue, unsigned char* pData,  int Size)
{	
	int Len;
	
	if ((pQueue == NULL) || (pData == NULL) || (Size <= 0) || (Size > QueueBuffSize(pQueue)))
	{
		return -1;
	}

	Len = QueueDataSize(pQueue);

	if (Len < Size)
	{
		Size = Len;
	}
	
	if (pQueue->End <= pQueue->Head)
	{
		QueueMemCopy(pData, pQueue->pStart + pQueue->End, Size);
		pQueue->End += Size;
		if (pQueue->End >= pQueue->BuffSize)
		{
			pQueue->End = 0;
		}
		
		return Size;
	}
	else
	{
		Len = pQueue->BuffSize - (pQueue->End);
		if (Size <= Len)
		{
			  QueueMemCopy(pData, pQueue->pStart + pQueue->End, Size);
			  pQueue->End += Size;	
			  
			  if (pQueue->End >= pQueue->BuffSize)
			  {
				  pQueue->End = 0;
			  }
			  return Size;
		}
		else
		{
			QueueMemCopy(pData, pQueue->pStart + pQueue->End, Len);
			pQueue->End = 0;
			QueueMemCopy(pData +  Len, pQueue->pStart, Size - Len);
			pQueue->End +=(Size - Len);
			return Size;
		}
	}
}

 /*-----------------------------------------------------------------  
int QueueWrite(QUEUE *pQueue, unsigned char* pData, const int Size)

  See header file for description.

  -----------------------------------------------------------------*/

int QueueWrite(QUEUE *pQueue, unsigned char* pData, const int Size)
{
      int Len = 0;
      
      if ((pQueue == NULL) || (pData == NULL) || (Size <= 0) || (QueueFull(pQueue)) 
	  	||((pQueue->BuffSize - QueueDataSize(pQueue)) < Size))
      {
          return -1;
      }
             
      Len = pQueue->BuffSize - pQueue->Head;
     
      if (Size <= Len)
      {
          QueueMemCopy(pQueue->pStart + pQueue->Head, pData, Size);
          pQueue->Head += Size;
		  if (pQueue->Head >= pQueue->BuffSize)
		  {
			  pQueue->Head = 0;
		  }
          return Size;
      }
      else
      {
          QueueMemCopy(pQueue->pStart + pQueue->Head, pData, Len);
          pQueue->Head = 0;
          QueueMemCopy(pQueue->pStart, pData + Len, Size - Len);
          pQueue->Head += (Size - Len);		
          return Size;
      }
}


 /*-----------------------------------------------------------------  
int QueueByteAccess(QUEUE *pQueue, int Index)

  See header file for description.

  -----------------------------------------------------------------*/

/*
 * 访问环形缓冲区中的数据
 * circ: 环形缓冲对象
 * index: 数据索引, 从0开始
 */
int QueueByteAccess(QUEUE *pQueue, int Index) 
{
	int Offset;
	
	if(pQueue == NULL) {
		return 0xFF;
	}

	if ((pQueue->End + Index) >= pQueue->BuffSize)
	{
		Offset = (pQueue->End + Index) - pQueue->BuffSize;
	}

	else
	{
		Offset = pQueue->End + Index;
	}

	return *(pQueue->pStart + Offset);
}



 /*-----------------------------------------------------------------  
int QueueDelete(QUEUE *pQueue, int Size)

  See header file for description.

  -----------------------------------------------------------------*/

/*
 * 将数据移出缓冲区
 * circ: 环形缓冲对象
 * count: 移出数据字节数
 */
int QueueDelete(QUEUE *pQueue, int Size) 
{
	int Len;

	if ((pQueue == NULL) || (Size <= 0) || (Size > QueueDataSize(pQueue))) 
	{
		return -1;
	}
	
	Len = pQueue->BuffSize - (pQueue->End);

	if (Size < Len)
	{
		pQueue->End += Size;
		
		if (pQueue->End >= pQueue->BuffSize)
		{
			pQueue->End = 0;
		}
	}
	else
	{
		pQueue->End = 0;
		pQueue->End += (Size - Len);
	}
	return 0;
	
}
