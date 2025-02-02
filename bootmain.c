// Boot loader.
//
// Part of the boot block, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads an ELF kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"

#define SECTSIZE 512

void readseg(uchar *, uint, uint);

// bootmainはkernelを0x10000にロードする
void bootmain(void)
{
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar *pa;

  // 0x10000 is the first address after the kernel in memory
  elf = (struct elfhdr *)0x10000; // scratch space

  // Read 1st page off disk
  // readsegはxv6.imgの1セクタ目(kernel)を読み込む
  readseg((uchar *)elf, 4096, 0);

  // Is this an ELF executable?
  if (elf->magic != ELF_MAGIC)
    return; // let bootasm.S handle error

  // program segment をロードする(ignores ph flags).
  ph = (struct proghdr *)((uchar *)elf + elf->phoff);
  eph = ph + elf->phnum;
  for (; ph < eph; ph++)
  {
    pa = (uchar *)ph->paddr;
    readseg(pa, ph->filesz, ph->off);
    if (ph->memsz > ph->filesz)
      // memszが大きければ足りない分を0で埋める
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  // Call the entry point from the ELF header.
  // Does not return!
  entry = (void (*)(void))(elf->entry);
  // kernelのエントリポイントを呼び出す
  // kernel.ldでENTRYを指定できる
  entry();
}

void waitdisk(void)
{
  // Wait for disk ready.
  while ((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// Read a single sector at offset into dst.
void readsect(void *dst, uint offset)
{
  // Issue command.
  waitdisk();
  outb(0x1F2, 1); // count = 1
  outb(0x1F3, offset);
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0);
  outb(0x1F7, 0x20); // cmd 0x20 - read sectors

  // Read data.
  waitdisk();
  insl(0x1F0, dst, SECTSIZE / 4);
}

// Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
// Might copy more than asked.
// pa: physical address
// count: number of bytes to read
// offset: kernelを0byte目にあるdiskとしてのoffset by bytes
void readseg(uchar *pa, uint count, uint offset)
{
  uchar *epa;

  epa = pa + count;

  // Round down to sector boundary.
  pa -= offset % SECTSIZE;

  // Translate from bytes to sectors; kernel starts at sector 1.
  // 1セクタ目から
  offset = (offset / SECTSIZE) + 1;

  // If this is too slow, we could read lots of sectors at a time.
  // We'd write more to memory than asked, but it doesn't matter --
  // we load in increasing order.
  for (; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset);
}

// readseg(100, 512, 10);
// pa == 90
// offset: block単位に変わる
// 0sector目はbootloader
// 1sector目からkernel
// offsetはdiskの何セクター目を読むか