#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
#define BIT_SIZE(type) (sizeof(type) * 8)
int
sys_pgaccess(void)
{
  uint64 pa;
  int num_pages;
  uint64 mask;

  argaddr(0, &pa);
  argint(1, &num_pages);
  argaddr(2, &mask);

  int mask_size= (num_pages + BIT_SIZE(char) - 1) / BIT_SIZE(char);
  char local_mask[mask_size];
  memset(local_mask, 0, mask_size);
  pagetable_t pt = myproc()->pagetable;

  for (int i = 0; i < num_pages; i++) {
    pte_t *pte = walk(pt, pa + i * PGSIZE, 0);
    if (pte == 0) return -1;

    if (*pte & PTE_A) {
      local_mask[i / BIT_SIZE(char)] |= 1 << (i % BIT_SIZE(char));
      *pte &= ~PTE_A;
    }
  }

  return copyout(pt, mask, local_mask, sizeof(local_mask));
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
