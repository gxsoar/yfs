#include "rpc.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "lock_server_cache.h"
#include "paxos.h"
#include "rsm.h"

#include "jsl_log.h"

// Main loop of lock_server

static void
force_exit(int) {
    exit(0);
}

int
main(int argc, char *argv[])
{
  int count = 0;

  // Force the lock_server to exit after 20 minutes
  signal(SIGALRM, force_exit);
  alarm(20 * 60);

  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  srandom(getpid());

  if(argc != 2){
    fprintf(stderr, "Usage: %s port\n", argv[0]);
    exit(1);
  }

  char *count_env = getenv("RPC_COUNT");
  if(count_env != NULL){
    count = atoi(count_env);
  }

  //jsl_set_debug(2);
  // Comment out the next line to switch between the ordinary lock
  // server and the RSM.  In Lab 6, we disable the lock server and
  // implement Paxos.  In Lab 7, we will make the lock server use your
  // RSM layer.
#define RSM
#ifdef RSM
  // lock_server ls;
  rsm rsm(argv[1], argv[2]);
  rpcs server(atoi(argv[1]), count);
  lock_server_cache lsc;
  // server.reg(lock_protocol::stat, &ls, &lock_server::stat);
  // server.reg(lock_protocol::acquire, &ls, &lock_server::acquire);
  // server.reg(lock_protocol::release, &ls, &lock_server::release);
  server.reg(lock_protocol::stat, &lsc, &lock_server_cache::stat);
  server.reg(lock_protocol::acquire, &lsc, &lock_server_cache::acquire);
  server.reg(lock_protocol::release, &lsc, &lock_server_cache::release);
#endif  // RSM


  while(1)
    sleep(1000);
}
