// vpmap.c: initial checkin

#include <stdio.h>

// The following two structs use bitfields to enable easier parsing of
// the data provided in /proc/<pid>/maps that will be a 64-bit number
// with bits representing a aspects of the page. The bit layouts are
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
} map_entry_t;

int main(int argc, char *argv[]){
  return 0;
}
