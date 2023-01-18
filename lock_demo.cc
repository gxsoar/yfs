//
// Lock demo
//

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "lock_client.h"
#include "lock_protocol.h"
#include "rpc.h"

std::string dst;
lock_client *lc;

int main(int argc, char *argv[]) {
  int r;

  if (argc != 2) {
    fprintf(stderr, "Usage: %s [host:]port\n", argv[0]);
    exit(1);
  }

  dst = argv[1];
  lc = new lock_client(dst);
  r = lc->stat(1);
  printf("stat returned %d\n", r);
}
