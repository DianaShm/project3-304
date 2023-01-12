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

#define MEMORY_FRAME 256
#define MEMORY_SIZE MEMORY_FRAME * PAGE_SIZE

#define BUFFER_SIZE 10

struct tlbentry {
  unsigned char log;
  unsigned char phys;
};

struct tlbentry tlb[TLB_SIZE];
int tlbindex = 0;

int pagetable[PAGES];
int table[MEMORY_FRAME];

signed char main_memory[MEMORY_SIZE];

signed char *back;

int max(int a, int b)
{
  if (a > b)
    return a;
  return b;
}

void initialTable(int *table) {
    for (int i = 0; i < MEMORY_FRAME; i++)
        table[i] = MEMORY_FRAME - (i + 1);
}

void invalid(unsigned char phys_adress) {
    for (int i = 0; i < PAGES; i++) {
        if (pagetable[i] == phys_adress) {
            pagetable[i] = -1;
            break;
        }
    }
}

int lru(int *table) {
    int index = 0;
    for (int i = 0; i < MEMORY_FRAME; i++) {
        if (table[i] == MEMORY_FRAME - 1) {
            index = 1;
            break;
        }
    }
    return index;
}

void tableUpdate(int *table, unsigned char last) {
    int usage = table[last];
    for (int i = 0; i < MEMORY_FRAME; i++) {
        if (i == last) {
            table[i] = 0;
            continue;
        }
        if (table[i] < usage) {
            table[i]++;
            continue;
        }
    }
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

  int replacementPolicy;
  for(int i=1; i<argc; i++){
    if(!strcmp(argv[i], "-p")) {replacementPolicy = atoi(argv[++i]);}
  }

  const char *back_filename = argv[1]; 
  int back_fd = open(back_filename, O_RDONLY);
  back = mmap(0, 1024*1024, PROT_READ, MAP_PRIVATE, back_fd, 0);
  
  const char *input_filename = argv[2];
  FILE *input_fp = fopen(input_filename, "r");
  
  int i;
  for (i = 0; i < PAGES; i++) {
    pagetable[i] = -1;
  }
  initialTable(table); 

  char buffer[BUFFER_SIZE];
  
  int total_addresses = 0;
  int tlb_hits = 0;
  int page_faults = 0;
  
  unsigned int free_page = 0;
  
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
          
          
          if (replacementPolicy == 0) {
            memcpy(&main_memory[free_page*PAGE_SIZE], back + log_page*PAGE_SIZE, PAGE_SIZE);
            
            invalid(free_page); 

            phys_page = free_page;
            pagetable[log_page] = phys_page;
            
            free_page++;
            free_page = free_page % MEMORY_FRAME;
          }
          else {
            int replacePage = lru(table);
            
            memcpy(&main_memory[replacePage*PAGE_SIZE], back + log_page*PAGE_SIZE, PAGE_SIZE);
            
            invalid(replacePage); 

            phys_page = replacePage;
            pagetable[log_page] = phys_page;
          }
          
      }
        
      add_to_tlb(log_page, phys_page);
    }
    tableUpdate(table, phys_page);
    
    int phys_address = (phys_page << OFFSET_BITS) | offset;
    signed char value = main_memory[phys_page * PAGE_SIZE + offset];
   
    printf("Virtual address: %d physical address: %d Value: %d\n", log_address, phys_address, value);
  }
  
  printf("Number of Translated Addresses = %d\n", total_addresses);
  printf("Page Faults = %d\n", page_faults);
  printf("Page Fault Rate = %.3f\n", page_faults / (1. * total_addresses));
  printf("TLB Hits = %d\n", tlb_hits);
  printf("TLB Hit Rate = %.3f\n", tlb_hits / (1. * total_addresses));

  return 0;
}