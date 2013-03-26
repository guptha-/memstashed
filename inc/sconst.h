#ifndef INC_SERV_CONST_H
#define INC_SERV_CONST_H

#define VERSION_STRING "VERSION 1.0\r\n"
#define SERV_DEF_MEM_LIMIT 6000
#define SERV_DEF_LIST_PORT 11211
#define SERV_DEF_ADDRESS "127.0.0.1"
#define SERV_DEF_WORKER_THREADS 5
#define DB_MAX_HASH_TABLES 4
#define EXPIRY_THRESHOLD 60*60*24*30
#define TRUE             1
#define FALSE            0

enum retStatus
{
  SUCCESS,
  MEMORY_FULL,
  EXIST,
  NOT_EXIST,
  FAILURE
};


#endif
