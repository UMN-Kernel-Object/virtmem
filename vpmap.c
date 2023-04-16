// vpmap.c: initial checkin

// Program that attempts to translate virtual 
// addresses to physical addresses in a Linux system 

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

// The following two structs use bitfields to enable easier parsing of
// the data provided in /proc/<pid>/maps that will be a 64-bit number
// with bits representing various aspects of the page. The bit layouts are
// extracted from the kernel docs on the /proc interface to page
// tables which is here:
// https://docs.kernel.org/admin-guide/mm/pagemap.html

// NOTE: may move the structs to a header file at some point

// map entry which is present in memory (not swapped out)
typedef struct {
  uint64_t pfn     : 55;    // Bits 0-54 page frame number (PFN) if present                             
  uint32_t dirty   :  1;    // Bit 55 pte is soft-dirty (see Soft-Dirty PTEs)                           
  uint32_t exclu   :  1;    // Bit 56 page exclusively mapped (since 4.2)                               
  uint32_t wprot   :  1;    // Bit 57 pte is uffd-wp write-protected (since 5.13) (see Userfaultfd)     
  uint32_t zeros   :  3;    // Bits 58-60 zero                                                          
  uint32_t isfile  :  1;    // Bit 61 page is file-page or shared-anon (since 3.5)                      
  uint32_t swapped :  1;    // Bit 62 page swapped                                                      
  uint32_t present :  1;    // Bit 63 page present                                                      
} entry_present_t;

// map entry which is swapped out
typedef struct {
  uint32_t swaptyp :  5;    // Bits 0-4 swap type if swapped
  uint64_t swapoff : 50;    // Bits 5-54 swap offset if swapped
  uint32_t dirty   :  1;    // Bit 55 pte is soft-dirty (see Soft-Dirty PTEs)                           
  uint32_t exclu   :  1;    // Bit 56 page exclusively mapped (since 4.2)                               
  uint32_t wprot   :  1;    // Bit 57 pte is uffd-wp write-protected (since 5.13) (see Userfaultfd)     
  uint32_t zeros   :  3;    // Bits 58-60 zero                                                          
  uint32_t isfile  :  1;    // Bit 61 page is file-page or shared-anon (since 3.5)                      
  uint32_t swapped :  1;    // Bit 62 page swapped                                                      
  uint32_t present :  1;    // Bit 63 page present                                                      
} entry_swapped_t;

// union of the two struct types so that one can access via
// map_entry_t entry;
// entry.existing_entry.pfn == <known pfn>;
typedef union {
  entry_present_t existing_entry;
  entry_swapped_t swapped_entry;
  uint64_t bytes;
} map_entry_t;

#define BUFSIZE 2048

void print_map_entry(map_entry_t m, char *prefix){
  printf("%s",prefix);
  // printf("bytes: %016lx  ",m.bytes);
  // printf("present: %d  ", m.present.present);
  // printf("swapped: %d  ", m.present.swapped);

  if(m.existing_entry.present){ // if entry is present, print its page frame number in memory
    printf("present  ");
    printf("pfn: %lx  ",m.existing_entry.pfn);
  }
  else if(m.existing_entry.swapped){ // otherwise, if entry was swapped, print information about the swap
    printf("swapped  ");
    printf("swaptyp: %lx  ",m.swapped_entry.swaptyp);
    printf("swapoff: %lx  ",m.swapped_entry.swapoff);
  }
  if(m.existing_entry.present || m.existing_entry.swapped){
    printf("dirty: %d  ",m.existing_entry.dirty);
    printf("exclu: %d  ",m.existing_entry.exclu);
    printf("wprot: %d  ",m.existing_entry.wprot);
    printf("isfile: %d  ",m.existing_entry.isfile);
  }
  printf("\n");
}

int main(int argc, char *argv[]){

  if(argc < 2){
    printf("usage: sudo %s <PID>\n",argv[0]);
    return 0;
  }

  if(geteuid() != 0){
    printf("This program only makes sense to run as root / sudo\n");
    printf("Normal user runs will produce little meaningful data\n");
    return 0;
  }

  char errmsg[BUFSIZE];

  // pid of process to fetch information about
  pid_t pid = atol(argv[1]);
  printf("Process %d\n",pid);

  uint64_t pagesize = sysconf(_SC_PAGE_SIZE);

  char maps_fname[BUFSIZE];
  snprintf(maps_fname,BUFSIZE,"/proc/%d/maps",pid);

  FILE *maps_file = fopen(maps_fname,"r");
  if(maps_file == NULL){
    snprintf(errmsg,BUFSIZE,"Error opening '%s'",maps_fname);
    perror(errmsg);
    exit(EXIT_FAILURE);
  }

  char pagemap_fname[BUFSIZE];
  snprintf(pagemap_fname,BUFSIZE,"/proc/%d/pagemap",pid);

  int pagemap_fd = open(pagemap_fname, O_RDONLY);
  if(pagemap_fd == -1){
    snprintf(errmsg,BUFSIZE,"Error opening '%s'",pagemap_fname);
    perror(errmsg);
    fclose(maps_file);
    exit(EXIT_FAILURE);
  }
    
  while(!feof(maps_file)){ // reading each line of the maps_file
    char line[BUFSIZE];
    char *result = fgets(line, BUFSIZE, maps_file);
    if(result == NULL){
      perror("Error reading line\n");
      fclose(maps_file);
      close(pagemap_fd);
      exit(EXIT_FAILURE);
    }
    line[strlen(line)-1] = '\0'; // chop the \n

    off_t start_addr, stop_addr;
    int count = sscanf(line, "%lx-%lx", &start_addr, &stop_addr);
    if(count < 2){
      printf("Error  parsing start/stop addresses in '%s'\n",line);
      fclose(maps_file);
      close(pagemap_fd);
      exit(EXIT_FAILURE);
    }

    printf("%s\n",line);

    // address conversion for a single mapping in maps_file
    // each mapping exists in the space defined
    // by this range: start_addr to stop_addr
    for(off_t cur_addr = start_addr; cur_addr < stop_addr; cur_addr += pagesize){

      off_t virt_pn = cur_addr / pagesize;
      off_t offset =  virt_pn * sizeof(map_entry_t);

      map_entry_t entry;
    
      ssize_t ret = pread(pagemap_fd, &entry, sizeof(map_entry_t), offset);
      if(ret != sizeof(map_entry_t)){
        printf("requested %ld from fd %d, read %ld bytes\n",
               sizeof(map_entry_t),pagemap_fd,ret);
        perror("Failed to read enough bytes");
        exit(EXIT_FAILURE);
      }

      // off_t ret = lseek(pagemap_fd, offset, SEEK_SET);
      // if(ret != offset){
      //   printf("requested seek to offset %lx, returned %ld\n",
      //          offset,ret);
      //   perror("Failed to seek");
      //   exit(EXIT_FAILURE);
      // }
    
      // ssize_t nread = read(pagemap_fd, &entry, sizeof(map_entry_t));
      // if(nread != sizeof(map_entry_t)){
      //   printf("requested %ld from fd %d, read %ld bytes\n",
      //          sizeof(map_entry_t),pagemap_fd,nread);
      //   perror("Failed to read enough bytes");
      //   exit(EXIT_FAILURE);
      // }

      // printf("+ vaddr: %lx   pagemap off: %lx  entry.bytes: %lx\n",
      //        cur_addr, offset, entry.bytes);
      printf("| vpn: %lx  ",virt_pn);
      print_map_entry(entry,"");
    }
    printf("\n");
  }

  return 0;
}
