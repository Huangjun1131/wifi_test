/**************************************************************************

Copyright:KangSong

Author: Jun Huang

Date:2017-05-16

Description:

**************************************************************************/

#ifndef _DDEBUG_
#define _DDEBUG_

#ifdef	__cplusplus
extern "C"{
#endif


#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>


/*
 *Usage of DDEBUG:
 *1.gcc with -DALLPRINT or -DURGENCY or -DWARNING or -DNORMAL -DUSRLDEBUG -DUSRHDEBUG -DALLUSER
 *2.code with DDEBUG(P_URGENCY, ...); 
 *			  DDEBUG(P_WARNING, ...);
 *			  DDEBUG(P_NORMAL, ...);
 *			  DDEBUG(P_USRLDEBUG, ...);
 *			  DDEBUG(P_USRHDEBUG, ...);
 *
 *			  DBG_DRR(...);
 *
 *Usage of syserr:
 *1.if you DON'T want to exit after meet a error then gcc with -DSYSERR_CONT
 *2.code with syserr(...);
 *
 * */


#ifdef ALLPRINT
#define D_ALLPRINT 0x000f
#else 
#define D_ALLPRINT 0
#endif

#ifdef ALLUSER
#define D_ALLUSER 0x00f0
#else 
#define D_ALLUSER 0
#endif

#if defined(URGENCY)
#define D_URGENCY 0x0001
#else 
#define D_URGENCY 0
#endif

#ifdef WARNING
#define D_WARNING 0x0002
#else 
#define D_WARNING 0
#endif

#ifdef NORMAL
#define D_NORMAL 0x0004
#else 
#define D_NORMAL 0
#endif

#ifdef USRLDEBUG
#define D_USRLDEBUG	0x0010
#else
#define D_USRLDEBUG	0
#endif

#ifdef USRHDEBUG
#define D_USRHDEBUG	0x0020
#else
#define D_USRHDEBUG	0
#endif

#define P_URGENCY		0x0001
#define P_WARNING 		0x0002
#define P_NORMAL		0x0004
#define P_USRLDEBUG		0x0010
#define P_USRHDEBUG		0x0020

#if defined(ALLPRINT) || defined(URGENCY) || defined(WARNING) || defined(NORMAL) || defined(USRLDEBUG) || defined(USRHDEBUG)
#ifndef DEBUG_PRINT_FLAGS
#define DEBUG_PRINT_FLAGS

#define  debug_print_flags (D_URGENCY | D_WARNING | D_NORMAL | D_ALLPRINT | D_USRLDEBUG | D_USRHDEBUG | D_ALLUSER)
#endif

#define DDEBUG(flag, fmt, ...) \
	 do{\
		 if(debug_print_flags & flag){\
			 printf(fmt,## __VA_ARGS__);}\
	 }while(0)
#else
#define DDEBUG(flag, fmt, ...) 

#endif

#define DBG_ERR(args)	printf(args)

#ifdef SYSERR_CONT
#define syserr(arg...) do{ 	\
	fprintf(stderr,arg);	\
	fprintf(stderr,": %s\n",strerror(errno));\
}while(0)
#else
#define syserr(arg...) do{ 	\
	fprintf(stderr,arg);	\
	fprintf(stderr,": %s\n",strerror(errno));\
	exit(1);				\
}while(0)
#endif

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({       		   		\
		const typeof( ((type *)0)->member ) *__mptr = (ptr); 	\
		(type *)( (char *)__mptr - offsetof(type,member) );}) 


#ifdef	__cplusplus
}
#endif


#endif
