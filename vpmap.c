// vpmap.c: initial checkin

#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// The following two structs use bitfields to enable easier parsing of
// the data provided in /proc/<pid>/maps that will be a 64-bit number
// with bits representing a aspects of the page. The bit layouts are
// extracted from the kernel docs on the /proc interface to page
// tables which is here:
// https://docs.kernel.org/admin-guide/mm/pagemap.html

// NOTE: may move the structs to a header file at some point

typedef struct {
  uint64_t start_addr;      // The first address in the range.
  uint64_t end_addr;        // The last address in the range.
  char mode[5];             // The mode in which the file is opened (rwxp).
  uint64_t offset;          // The start offset in bytes within the file that is mapped.
  uint32_t major_id;        // Together with minor_id represents the device that holds the file.
  uint32_t minor_id;        // 08:30 for the device that holds the root filesystem, 00:00 for non-file mappings.
  uint64_t inode_id;        // ID of a struct containing some filesystem metadata.
  char *path;               // The path to the file, or something like [heap] or [stack]
                            //   Must be freed at the moment, could probably be replaced by a
                            //   fixed length array.
} maps_entry_t;

// map entry which is present in memory (not swapped out)
typedef struct {
  uint64_t pfn     : 55;    // Bits 0-54 page frame number (PFN) if present                             
  uint32_t dirty   :  1;    // Bit 55 pte is soft-dirty (see Soft-Dirty PTEs)                           
  uint32_t exclu   :  1;    // Bit 56 page exclusively mapped (since 4.2)                               
  uint32_t wprot   :  1;    // Bit 57 pte is uffd-wp write-protected (since 5.13) (see Userfaultfd)     
  uint32_t zeros   :  3;    // Bits 58-60 zero                                                          
  uint32_t isanon  :  1;    // Bit 61 page is file-page or shared-anon (since 3.5)                      
  uint32_t swapped :  1;    // Bit 62 page swapped                                                      
  uint32_t present :  1;    // Bit 63 page present                                                      
} entry_present_t;

// map entry which is swapped out
typedef struct {
  uint32_t swaptyp :  5;    // Bits 0-4 swap type if swapped
  uint64_t pfn     : 50;    // Bits 5-54 swap offset if swapped
  uint32_t dirty   :  1;    // Bit 55 pte is soft-dirty (see Soft-Dirty PTEs)                           
  uint32_t exclu   :  1;    // Bit 56 page exclusively mapped (since 4.2)                               
  uint32_t wprot   :  1;    // Bit 57 pte is uffd-wp write-protected (since 5.13) (see Userfaultfd)     
  uint32_t zeros   :  3;    // Bits 58-60 zero                                                          
  uint32_t isanon  :  1;    // Bit 61 page is file-page or shared-anon (since 3.5)                      
  uint32_t swapped :  1;    // Bit 62 page swapped                                                      
  uint32_t present :  1;    // Bit 63 page present                                                      
} entry_swapped_t;

// union of the two struct types so that one can access via
// map_entry_t entry;
// entry.present.pfn == ???;
typedef union {
  entry_present_t present;
  entry_swapped_t swapped;
} pm_entry_t;

// NOTE: These functions should probably be moved along with structs above.

/**
 * @brief Opens pagemap and maps for the given PID.
 * @param pid A string containing the PID number.
 * @param maps_fd A pointer to an integer to contain the maps file descriptor.
 * @param pm_fd A pointer to an integer to contain the pagemap file descriptor.
*/
void proc_open(char *pid, int *maps_fd, int *pm_fd)
{
  char maps_path[strlen(pid) + 11];
  char pm_path[strlen(pid) + 14];
  sprintf(maps_path, "/proc/%s/maps", pid);
  sprintf(pm_path, "/proc/%s/pagemap", pid);

	*maps_fd = open(maps_path, O_RDONLY);
  *pm_fd = open(pm_path, O_RDONLY);

	if (*maps_fd < 0) {
		perror(maps_path);
		exit(EXIT_FAILURE);
	}

  if (*pm_fd < 0) {
    perror(pm_path);
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Reads one entry of pagemap. Exits if a read fails.
 * @param pm_fd The file descriptor for the pagemap.
 * @param buf The buffer to be written to.
 * @param vaddr The virtual address to be read from the pagemap.
*/
void pmread(int pm_fd, pm_entry_t *buf, uint64_t vaddr)
{
  long bytes = pread(pm_fd, buf, 8, (off_t)(vaddr / PAGE_SIZE * 8));
  
  if (bytes < 0) {
    fprintf(stderr, "Failed read from pagemap");
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Prints the physical address of a pagemap entry.
 * Can also print NOT PRESENT, or SWAPPED.
 * @param entry A pagemap entry struct that has already been filled by pmread.
 * @param vaddr The virtual adress of the
*/
void print_phys_addr(const pm_entry_t *entry)
{
  if (!entry->present.present) {
    printf(" | ENTRY: %016lx | NOT PRESENT\n", *(uint64_t *)entry);
    return;
  }

  // TODO: Handle further description for non-present pages.
  if (entry->present.swapped) {
    printf(" | ENTRY: %016lx | SWAPPED\n", *(uint64_t *)entry);
    return;
  }

  uint64_t phys_addr = entry->present.pfn * PAGE_SIZE;
  printf(" | ENTRY: %016lx | PFN: %lx\n", *(uint64_t *)entry, phys_addr);
}

/**
 * @brief Parses one line of maps into a maps_entry buffer struct.
 * @param entry A maps_entry struct that will be written to.
 * @param maps The file descriptor for maps.
*/
void maps_parseln(maps_entry_t *entry, FILE **maps)
{
  // Assumes that an entry is less than 512 bytes long, this could be fixed.
  char buff[512];
  char pathbuff[512];

  fgets(buff, 512, *maps);
  // Some lines of maps don't have a path at the end, this if handles that. 
  if (strlen(buff) < 73) {
    sscanf(buff, "%lx-%lx %s %lx %x:%x %lx",
      &entry->start_addr, &entry->end_addr, entry->mode, &entry->offset,
      &entry->major_id, &entry->minor_id, &entry->inode_id);
    pathbuff[0] = '\0';
  } else {
    sscanf(buff, "%lx-%lx %s %lx %x:%x %lx %s",
      &entry->start_addr, &entry->end_addr, entry->mode, &entry->offset,
      &entry->major_id, &entry->minor_id, &entry->inode_id, pathbuff);
  }
  
  entry->path = malloc(strlen(pathbuff));
  strcpy(entry->path, pathbuff);
};

/**
 * @brief Walks through the maps file and prints the physical address number
 * in a table.
 * @param maps_fd The file descriptor for maps.
 * @param pm_fd The file descirptor for the pagemap.
*/
void maps_parse(int maps_fd, int pm_fd)
{
  FILE *maps = fdopen(maps_fd, "r");
  maps_entry_t maps_entry;
  pm_entry_t pm_entry;
  uint64_t phys_addr;

  char maps_buff[512];

  maps_parseln(&maps_entry, &maps);
  while (!feof(maps)) {
    // Print one entry of maps
    sprintf(maps_buff, "%lx-%lx %s %08lx %02x:%02x %lx %s",
      maps_entry.start_addr, maps_entry.end_addr, maps_entry.mode,
      maps_entry.offset, maps_entry.major_id, maps_entry.minor_id,
      maps_entry.inode_id, maps_entry.path);
    printf("%-115s", maps_buff);

    // Read the starting virtual address from that maps entry, and search
    // for it in pagemap.
    pmread(pm_fd, &pm_entry, maps_entry.start_addr);
    print_phys_addr(&pm_entry);

    // Parse the next entry
    free(maps_entry.path);
    maps_parseln(&maps_entry, &maps);
  }
};

int main(int argc, char *argv[]){
  if (argc != 2) {
    puts("Usage: sudo ./vpmap.out <pid>\n");
    return 0;
  }

  int maps_fd, pm_fd;
  proc_open(argv[1], &maps_fd, &pm_fd);
  maps_parse(maps_fd, pm_fd);
  return 0;
}
