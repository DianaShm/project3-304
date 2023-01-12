#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#define TLB_SIZE 16
#define PAGES 1024
#define PAGE_MASK 0x000FFC00 

#define PAGE_SIZE 1024
#define OFFSET_BITS 10
#define OFFSET_MASK 0x000003FF 

#define MEMORY_SIZE PAGES * PAGE_SIZE

#define BUFFER_SIZE 10

struct tlbentry {
  unsigned char log;
  unsigned char phys;
};

struct tlbentry tlb[TLB_SIZE];
int tlbindex = 0;
int pagetable[PAGES];

signed char main_memory[MEMORY_SIZE];

signed char *back;

int max(int a, int b)
{
  if (a > b)
    return a;
  return b;
}

int search_tlb(unsigned char log_page) {
  int i;
  
    for (i = 0; i < TLB_SIZE; i++) {
       if (tlb[i].log == log_page) {
       return tlb[i].phys;
    }
  }
  
  return -1;
}

void add_to_tlb(unsigned char log, unsigned char phys) {
    struct tlbentry entry;
    entry.log = log;
    entry.phys = phys;

    tlb[tlbindex] = entry;
    
    tlbindex++;
    tlbindex = tlbindex % TLB_SIZE;
}

int main(int argc, const char *argv[])
{
  if (argc != 3) {
    fprintf(stderr, "Usage ./virtmem backingstore input\n");
    exit(1);
  }
  
  const char *back_filename = argv[1]; 
  int back_fd = open(back_filename, O_RDONLY);
  back = mmap(0, MEMORY_SIZE, PROT_READ, MAP_PRIVATE, back_fd, 0); 
  
  const char *input_filename = argv[2];
  FILE *input_fp = fopen(input_filename, "r");
  
  int i;
  for (i = 0; i < PAGES; i++) {
    pagetable[i] = -1;
  }

  char buffer[BUFFER_SIZE];

  int total_addresses = 0;
  int tlb_hits = 0;
  int page_faults = 0;

  unsigned char free_page = 0;

  while (fgets(buffer, BUFFER_SIZE, input_fp) != NULL) {
    total_addresses++;
    int log_address = atoi(buffer);

    int offset = log_address & OFFSET_MASK;
    int log_page = (log_address & PAGE_MASK) >> OFFSET_BITS;
    
    int phys_page = search_tlb(log_page);
    
    if (phys_page != -1) {
      tlb_hits++;
    }
    
    else {
      phys_page = pagetable[log_page];
      if (phys_page == -1) {
        page_faults++;
        memcpy(&main_memory[free_page*PAGE_SIZE], back + log_page * PAGE_SIZE, PAGE_SIZE);

        phys_page = free_page;
        pagetable[log_page] = phys_page;
        free_page++;
      }
      add_to_tlb(log_page, phys_page);
    }
    
    int phys_address = (phys_page << OFFSET_BITS) | offset;
    signed char value = main_memory[phys_page * PAGE_SIZE + offset];
    
    printf("Virtual address: %d physical address: %d Value: %d\n", log_address, phys_address, value);
  }

  printf("Number of Translated Addresses = %d\n", total_addresses);
  printf("Page Faults = %d\n", page_faults);
  printf("Page Fault Rate = %.3f\n", (double) page_faults / total_addresses);
  printf("TLB Hits = %d\n", tlb_hits);
  printf("TLB Hit Rate = %.3f\n", (double) tlb_hits / total_addresses);

  return 0;
}

