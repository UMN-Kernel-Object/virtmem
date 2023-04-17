# virtmem
Code and Documentation produced by the Virtual Memory subbroup

## Manifest

| File           | Description                                                          |
|----------------|----------------------------------------------------------------------|
| vpmap.c        | New program to map ranges of virtual addresses to physical locations |
|----------------|----------------------------------------------------------------------|
| memory_parts.c | Sample program to create memory and analyze via vpmap.c              |
| gettysburg.txt | Used in memory_parts.c in a memory map                               |
|----------------|----------------------------------------------------------------------|
| ref_code1.c    | Reference implementation on converting memory to physical addresses  |
| ref_code2.c    | Reference implementation on converting memory to physical addresses  |


## References
- [Kernel Docs on /proc FS Memory Maps](https://docs.kernel.org/admin-guide/mm/pagemap.html)
- [Blog Post on Related Program](https://fivelinesofcode.blogspot.com/2014/03/how-to-translate-virtual-to-physical.html)
- [Stack Overflow Post on Related Program](https://stackoverflow.com/questions/5748492/is-there-any-api-for-determining-the-physical-address-from-virtual-address-in-li)

