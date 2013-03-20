#ifndef INC_SERV_CMD_H
#define INC_SERV_CMD_H

#include <string>

typedef struct
{
  string key;
  string flags;
  unsigned long int expTime;
  unsigned int bytes;
  string casStr;
  bool noreply;
  string data;
} StorageStruct;

#endif
