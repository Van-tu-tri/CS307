#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm_dbg.h"

#define NOPS (16)

#define OPC(i) ((i) >> 12)
#define DR(i) (((i) >> 9) & 0x7)
#define SR1(i) (((i) >> 6) & 0x7)
#define SR2(i) ((i) & 0x7)
#define FIMM(i) ((i >> 5) & 01)
#define IMM(i) ((i) & 0x1F)
#define SEXTIMM(i) sext(IMM(i), 5)
#define FCND(i) (((i) >> 9) & 0x7)
#define POFF(i) sext((i) & 0x3F, 6)
#define POFF9(i) sext((i) & 0x1FF, 9)
#define POFF11(i) sext((i) & 0x7FF, 11)
#define FL(i) (((i) >> 11) & 1)
#define BR(i) (((i) >> 6) & 0x7)
#define TRP(i) ((i) & 0xFF)

/* New OS declarations */

// OS bookkeeping constants
#define PAGE_SIZE       (4096)  // Page size in bytes
#define OS_MEM_SIZE     (2)     // OS Region size. Also the start of the page tables' page
#define Cur_Proc_ID     (0)     // id of the current process
#define Proc_Count      (1)     // total number of processes, including ones that finished executing.
#define OS_STATUS       (2)     // Bit 0 shows whether the PCB list is full or not
#define OS_FREE_BITMAP  (3)     // Bitmap for free pages

// Process list and PCB related constants
#define PCB_SIZE  (3)  // Number of fields in a PCB
#define PID_PCB   (0)  // Holds the pid for a process
#define PC_PCB    (1)  // Value of the program counter for the process
#define PTBR_PCB  (2)  // Page table base register for the process

#define CODE_SIZE       (2)  // Number of pages for the code segment
#define HEAP_INIT_SIZE  (2)  // Number of pages for the heap segment initially

bool running = true;

typedef void (*op_ex_f)(uint16_t i);
typedef void (*trp_ex_f)();

enum { trp_offset = 0x20 };
enum regist { R0 = 0, R1, R2, R3, R4, R5, R6, R7, RPC, RCND, PTBR, RCNT };
enum flags { FP = 1 << 0, FZ = 1 << 1, FN = 1 << 2 };

uint16_t mem[UINT16_MAX] = {0};
uint16_t reg[RCNT] = {0};
uint16_t PC_START = 0x3000;

void initOS();
int createProc(char *fname, char *hname);
void loadProc(uint16_t pid);
uint16_t allocMem(uint16_t ptbr, uint16_t vpn, uint16_t read, uint16_t write);  // Can use 'bool' instead
int freeMem(uint16_t ptr, uint16_t ptbr);
static inline uint16_t mr(uint16_t address);
static inline void mw(uint16_t address, uint16_t val);
static inline void tbrk();
static inline void thalt();
static inline void tyld();
static inline void trap(uint16_t i);

static inline uint16_t sext(uint16_t n, int b) { return ((n >> (b - 1)) & 1) ? (n | (0xFFFF << b)) : n; }
static inline void uf(enum regist r) {
    if (reg[r] == 0)
        reg[RCND] = FZ;
    else if (reg[r] >> 15)
        reg[RCND] = FN;
    else
        reg[RCND] = FP;
}
static inline void add(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] + (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void and(uint16_t i)  { reg[DR(i)] = reg[SR1(i)] & (FIMM(i) ? SEXTIMM(i) : reg[SR2(i)]); uf(DR(i)); }
static inline void ldi(uint16_t i)  { reg[DR(i)] = mr(mr(reg[RPC]+POFF9(i))); uf(DR(i)); }
static inline void not(uint16_t i)  { reg[DR(i)]=~reg[SR1(i)]; uf(DR(i)); }
static inline void br(uint16_t i)   { if (reg[RCND] & FCND(i)) { reg[RPC] += POFF9(i); } }
static inline void jsr(uint16_t i)  { reg[R7] = reg[RPC]; reg[RPC] = (FL(i)) ? reg[RPC] + POFF11(i) : reg[BR(i)]; }
static inline void jmp(uint16_t i)  { reg[RPC] = reg[BR(i)]; }
static inline void ld(uint16_t i)   { reg[DR(i)] = mr(reg[RPC] + POFF9(i)); uf(DR(i)); }
static inline void ldr(uint16_t i)  { reg[DR(i)] = mr(reg[SR1(i)] + POFF(i)); uf(DR(i)); }
static inline void lea(uint16_t i)  { reg[DR(i)] =reg[RPC] + POFF9(i); uf(DR(i)); }
static inline void st(uint16_t i)   { mw(reg[RPC] + POFF9(i), reg[DR(i)]); }
static inline void sti(uint16_t i)  { mw(mr(reg[RPC] + POFF9(i)), reg[DR(i)]); }
static inline void str(uint16_t i)  { mw(reg[SR1(i)] + POFF(i), reg[DR(i)]); }
static inline void rti(uint16_t i)  {} // unused
static inline void res(uint16_t i)  {} // unused
static inline void tgetc()        { reg[R0] = getchar(); }
static inline void tout()         { fprintf(stdout, "%c", (char)reg[R0]); }
static inline void tputs() {
  uint16_t *p = mem + reg[R0];
  while(*p) {
    fprintf(stdout, "%c", (char) *p);
    p++;
  }
}
static inline void tin()      { reg[R0] = getchar(); fprintf(stdout, "%c", reg[R0]); }
static inline void tputsp()   { /* Not Implemented */ }
static inline void tinu16()   { fscanf(stdin, "%hu", &reg[R0]); }
static inline void toutu16()  { fprintf(stdout, "%hu\n", reg[R0]); }

trp_ex_f trp_ex[10] = {tgetc, tout, tputs, tin, tputsp, thalt, tinu16, toutu16, tyld, tbrk};
static inline void trap(uint16_t i) { trp_ex[TRP(i) - trp_offset](); }
op_ex_f op_ex[NOPS] = {/*0*/ br, add, ld, st, jsr, and, ldr, str, rti, not, ldi, sti, jmp, res, lea, trap};

/**
  * Load an image file into memory.
  * @param fname the name of the file to load
  * @param offsets the offsets into memory to load the file
  * @param size the size of the file to load
*/
void ld_img(char *fname, uint16_t *offsets, uint16_t size) {
    FILE *in = fopen(fname, "rb");
    if (NULL == in) {
        fprintf(stderr, "Cannot open file %s.\n", fname);
        exit(1);
    }

    for (uint16_t s = 0; s < size; s += PAGE_SIZE) {
        uint16_t *p = mem + offsets[s / PAGE_SIZE];
        uint16_t writeSize = (size - s) > PAGE_SIZE ? PAGE_SIZE : (size - s);
        fread(p, sizeof(uint16_t), (writeSize), in);
    }
    
    fclose(in);
}

void run(char *code, char *heap) {
  while (running) {
    uint16_t i = mr(reg[RPC]++);
    op_ex[OPC(i)](i);
  }
}

// YOUR CODE STARTS HERE

// This function is used to get file size.
uint16_t get_file_size(const char *fname) {
    FILE *file = fopen(fname, "rb");
    if (file == NULL) {
        fprintf(stderr, "Cannot open file %s.\n", fname);
        exit(1);
    }

    fseek(file, 0, SEEK_END); // Move the file pointer to the end
    uint16_t size = (uint16_t)ftell(file); // Get the current position (file size)
    fclose(file);
    return size;
}


void initOS() {
  mem[0] = 0xffff;
  mem[1] = 0x0000;
  mem[2] = 0x0000;
  mem[3] = 0b0001111111111111;
  mem[4] = 0b1111111111111111;
  return;
}

// Process functions to implement
int createProc(char *fname, char *hname) {
  if ((mem[2] & 0b0000000000000001) == 1)
  {
    printf("The OS memory region is full. Cannot create a new PCB.\n");
    running = 0;
    return 0;
  }

  // mem[1] is total process count.
  uint16_t index = mem[1];
  if (index == 0) // This is the first program.
  {
    
    mem[12 + index * 3] = 0x0000;       // Initialize process id
    mem[(12 + index * 3) + 1] = 0x3000; // Initialize PC
    mem[(12 + index * 3) + 2] = 4096;   // Initialize page_table_entry
    
  }
  else
  {
    mem[12 + index * 3] = mem[12 + (index - 1) * 3] + 1;  // Initialize process id (+1 bigger)
    mem[(12 + index * 3) + 1] = 0x3000;                   // Initialize PC
    mem[(12 + index * 3) + 2] = 4096 + index * 32;        // Initialize page_table_entry
  }

  int16_t found_code1 = -1; 
  int16_t found_code2 = -1;
  int16_t found_heap1 = -1;
  int16_t found_heap2 = -1;

  // Memory allocation is vpn.  
  found_code1 = allocMem(mem[(12 + index * 3) + 2], 6, 0xffff, 0x0000);  // Try to allocate first available page for code.
  
  if (found_code1 != -1)
  {
    found_code2 = allocMem(mem[(12 + index * 3) + 2], 7, 0xffff, 0x0000);  // Try to allocate second available page for code.
  
    if (found_code2 != -1)
    {
      found_heap1 = allocMem(mem[(12 + index * 3) + 2], 8, 0xffff, 0xffff);
      if (found_heap1 != -1)
      {
        found_heap2 = allocMem(mem[(12 + index * 3) + 2], 9, 0xffff, 0xffff);
        if (found_heap2 != -1)
        {
          uint16_t code_offsets[2] = {found_code1 * 2048, found_code2 * 2048};  // Physical addresses.
          uint16_t heap_offsets[2] = {found_heap1 * 2048, found_heap2 * 2048};  // Physical addresses.
          uint16_t code_size = get_file_size(fname);  // File size
          uint16_t heap_size = get_file_size(hname);  // File size
          
          ld_img(fname, code_offsets, code_size);
          ld_img(hname, heap_offsets, heap_size);
          
          mem[1] = mem[1] + 1;  // Increase process number.
          return 1;
        }
      }
    }
  }
  
  if ( (found_code1 == -1) || (found_code2 == -1) )
  {
    if (found_code1 != -1)
    {
      freeMem(6, mem[(12 + index * 3) + 2] = 0x0000);
    }
    printf("Cannot create code segment.\n");
  }
  if ( (found_heap1 == -1) || (found_heap2 == -1) )
  {
    freeMem(6, mem[(12 + index * 3) + 2] = 0x0000);
    freeMem(7, mem[(12 + index * 3) + 2] = 0x0000);
    if (found_heap1 != -1)
    {
      freeMem(8, mem[(12 + index * 3) + 2] = 0x0000);
    }
    printf("Cannot create heap segment.\n");
  }

  // Free the allocated memory.
  mem[12 + index * 3] = 0x0000;
  mem[(12 + index * 3) + 1] = 0x0000;
  mem[(12 + index * 3) + 2] = 0x0000;
  return 0;
}

void loadProc(uint16_t pid) {
  mem[0] = pid;                         // Set current process.
  reg[PTBR] = mem[(12 + pid * 3) + 2];  // Set page_table_base_register.
  reg[RPC] = mem[(12 + pid * 3) + 1];   // Set PC for this process.
}

uint16_t allocMem(uint16_t ptbr, uint16_t vpn, uint16_t read, uint16_t write) {

  // Check wheter is it allocated or not.
  if ( (mem[ptbr + vpn] & 0x0001) == 1)
  {
    printf("Cannot allocate memory for page %d of pid %d since it is already allocated.\n", vpn, mem[0]);
    return 0;
  }
  

  int16_t found = -1;
  uint16_t mask = 0b1000000000000000;


  for (int i = 0; i < 16; i++)
  {
    // mem[3] is free page map.
    if ( ((mem[3] & mask) >> (15 - i)) == 1)
    {
      found = i;
      mem[3] = mem[3] & ~(0b0000000000000001 << (15 - i)); // Update the free page.
      break;
    }
    else
    {
      mask = mask >> 1; // Look next bit.
    }
  }

  if (found == -1) // There is no free space in the first 16 pages.
  {
    
    mask = 0b1000000000000000;  // Set the mask.
    for (int i = 0; i < 16; i++)
    {
      if ( ((mem[4] & mask) >> (15 - i)) == 1)
      {
        found = i + 16;
        mem[4] = mem[4] & ~(0b0000000000000001 << (15 - i));
        break;
      }
      else
      {
        mask = mask >> 1;
      }
    }
  }

  if (found != -1) 
  {
    
    if (read == UINT16_MAX)
    {
      if (write == UINT16_MAX)
      {
         mem[ptbr + vpn] = (found << 11) + 0b0000000000000111;
         //printf("Page table entry is written with this value (WR): %d\n", mem[ptbr + vpn]);
      }
      else
      {
        mem[ptbr + vpn] = (found << 11) + 0b0000000000000011;
        //printf("Page table entry is written with this value (R): %d\n", mem[ptbr + vpn]);
      }
    }
    else
    {
      if (write == UINT16_MAX)
      {
         mem[ptbr + vpn] = (found << 11) + 0b0000000000000101;
         //printf("Page table entry is written with this value (W): %d\n", mem[ptbr + vpn]);
      }
      else
      {
        mem[ptbr + vpn] = (found << 11) + 0b0000000000000000;
        //printf("Page table entry is written with this value (INV): %d\n", mem[ptbr + vpn]);
      }
    }
   
    // If all pages are allocated: Set the OS status to 1.
    if (found == 31)
    {
      mem[2] = 0x0001;
    }


    // Return the PF
    return (found);
  }
  
  // There is no free space.
  printf("Cannot allocate more space for pid %d since there is no free page frames.\n", mem[0]);
  return 0;
}

int freeMem(uint16_t vpn, uint16_t ptbr) {
  // The page is already freed.
  if ((mem[ptbr + vpn] & 0x0001) == 0)
  {
    return 0;
  }
  else
  {
    uint16_t temp = mem[ptbr + vpn] >> 11; // physical page number;
    if (temp >= 16) 
    {
      temp = temp - 16;
      mem[4] = mem[4] ^ (0b0000000000000001 << (15 - temp));
    }
    else
    {
      mem[3] = mem[3] ^ (0b0000000000000001 << (15 - temp));
    }
    // Update the page entry INVALID.
    mem[ptbr + vpn] = mem[ptbr + vpn] & 0b1111111111111110;

    // Set the OSstatus to 0 again. (A page is freed)
    mem[2] = 0x0000;
    return 1;
  }
}

// Instructions to implement
static inline void tbrk() {
  uint16_t address = reg[R0];

  uint16_t vpn = address >> 11;
  uint16_t request = address & 0x0001;                  // If 1 allocate, if 0 free the vpn.
  uint16_t read = (address & 0b0000000000000010) >> 1;  // If 1, read, else no read.
  uint16_t write = (address & 0b0000000000000100) >> 2; 
  if (read == 1) {read = 0xffff;}
  else read = 0x0000;

  if (write ==1 ) write = 0xffff;
  else write = 0x0000;

  // Write and read it set.
  if (request == 1) // Allocate memory
  {
    printf("Heap increase requested by process %d.\n", mem[0]);
    allocMem(reg[PTBR], vpn, read, write);
  }
  else // Free memory
  {
    printf("Heap decrease requested by process %d.\n", mem[0]);
    uint16_t success = freeMem(vpn, reg[PTBR]);
    if (success == 0)
    {
      printf("Cannot free memory of page %d of pid %d since it is not allocated.\n", vpn, mem[0]);
    }
  }
}

static inline void tyld() {
  uint16_t current = mem[0];
  uint16_t old = mem[0];

  // Save the current PC.
  mem[(12 + current * 3) + 1] = reg[RPC];
  
  // Go to next process.
  current = (current + 1) % mem[1];

  while ((mem[(12 + current * 3)] != mem[0]) && (mem[(12 + current * 3)] == 0xffff))
  {
    current = (current + 1) % mem[1];
  }
  
  // Set the current process.
  mem[0] = current;
  
  // Retrive the PC and Page_table_base_register
  reg[PTBR] = mem[(12 + current * 3) + 2];
  reg[RPC] = mem[(12 + current * 3) + 1];

  // Inform the switch.
  printf("We are switching from process %d to %d.\n", old, mem[0]);
}

// Instructions to modify
static inline void thalt() {

  uint16_t current = mem[0];
  uint16_t old = mem[0];

  // Go to the next process
  current = (current + 1) % mem[1];
  
  while ((mem[(12 + current * 3)] != mem[0]) && (mem[(12 + current * 3)] == 0xffff))
  {
    current = (current + 1) % mem[1];
  }

  if (current == old) // If the next process is the same.
  {
    // Set it to 0xffff
    mem[12 + current * 3] = 0xffff;

    // For everypage that is allocated, free the page.
    uint16_t current_page_entry = mem[(12 + current * 3) + 2];
    for (int i = 0; i < 32; i++)
    {
      if (mem[current_page_entry] != 0)
      {
        freeMem(i, mem[(12 + current * 3) + 2]);
      }
      current_page_entry++;
    }
    
    // Stop the machine.
    running = 0;
  }
  else
  {
    // There is still process to run.
    // Set the PCB to 0xffff;
    mem[12 + old * 3] = 0xffff;

    uint16_t current_page_entry = mem[(12 + old * 3) + 2];
    for (int i = 0; i < 32; i++)
    {
      if (mem[current_page_entry] != 0)
      {
        
        freeMem(i, mem[(12 + old * 3) + 2]);
      }
      current_page_entry++;
    }

   
    mem[0] = current;       // Set the new process
    

    // Restore PC and page_table_base_register.
    reg[PTBR] = mem[(12 + current * 3) + 2];
    reg[RPC] = mem[(12 + current * 3) + 1];
  }
}

static inline uint16_t mr(uint16_t address) {
  uint16_t vpn = address >> 11;
  
  if (vpn <= 5)
  {
    printf("Segmentation fault.\n");
    running = 0;
    return -1;
  }
  if ((mem[reg[PTBR] + vpn] & 0b0000000000000001) == 0)
  {
    printf("Segmentation fault inside free space.\n");
    running = 0;
    return -1;
  }
  if ((mem[reg[PTBR] + vpn] & 0b0000000000000010) == 0)
  {
    printf("Cannot read from a write-only page.\n");
    running = 0;
    return - 1;
  }
  uint16_t offset = address & 0b0000011111111111;
  uint16_t PF = mem[reg[PTBR] + vpn] >> 11;

  return mem[(PF << 11) + offset];
}

static inline void mw(uint16_t address, uint16_t val) {
  // Address is virtual.
  // Take the page table.
  uint16_t vpn = address >> 11;
  //printf("This is the page table register value and vpn: %d, %d\n", reg[PTBR], vpn);
  //printf("This is the page table entry: %d.\n", mem[reg[PTBR] + vpn]);
  if (vpn <= 5)
  {
    printf("Segmentation fault.\n");
    running = 0;
    return;
  }
  if ((mem[reg[PTBR] + vpn] & 0b0000000000000001) == 0)
  {
    printf("Segmentation fault inside free space.\n");
    running = 0;
    return;
  }
  
  if ((mem[reg[PTBR] + vpn] & 0b0000000000000100) == 0)
  {
    printf("Cannot write to a read-only page.\n");
    running = 0;
    return;
  }
  uint16_t offset = address & 0b0000011111111111;
  uint16_t PF = mem[reg[PTBR] + vpn] >> 11;

  mem[(PF << 11) + offset] = val;
}



// YOUR CODE ENDS HERE
