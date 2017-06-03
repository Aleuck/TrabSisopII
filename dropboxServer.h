#include <stdio.h>
#include <stdlib.h>
#include "dropboxUtil.h"

struct Client{
  int devices[2];
  char userid[MAXNAME];
  int logged_in;
};
