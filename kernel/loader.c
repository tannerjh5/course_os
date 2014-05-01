#include "include/loader.h"
#include "include/mem_alloc.h"
#include "include/klibc.h"
#include "include/process.h"
#include <stdint.h>
#include "include/elf.h"

//Worked on by Kaelen Haag and Jeremy Wenzel
//We determine what the size of our process is going to be
//Determine the size of the segments that need to be loaded in (has p_type == PT_LOAD)
//Account for the other stuff we'll need (Stack, Heap, spacing)
os_size_t det_proc_size(Elf_Ehdr *h, Elf_Phdr ph[])
{
	os_size_t process_size = 0;
	int i = 0;
	//Add up the size of all the necessary sections that 
	for(; i < (h->e_phnum); i++)
	{
		if(ph[i].p_type == PT_LOAD)
		{
			process_size += ph[i].p_memsz;
		}
	}
	
	process_size +=  USER_PROC_STACK_SIZE;
	process_size +=  4096; //This is going to be the heap size (need to determine what this should be
	
	//Padding we want to be able to have some padding between certain sections of the program 
	//mainly the stack from the other parts
	process_size += 0x1000;
	
	return process_size;
	
	
}


void allocate_process_memory(pcb *pcb_p, Elf_Ehdr *h, Elf_Phdr ph[], uint32_t * file_pointer)
{
	os_size_t process_size = det_proc_size(h, ph);
	uint32_t * process_mem = kmalloc(process_size * 4);
	uint32_t * current_pointer = process_mem;
	//We're gonna copy each segment into memory at a position. Calculate what needs to done to change
	//the entry_point to account for where we've placed something in memory
	
	//memcpy(process_mem, pointer to read only segment, size of RO Segment);
	//memcpy(process_mem + size of RO Segment, pointer to Read and write segment, size of R&W segment)	
	//make a pointer to heap process_mem + size of RO segment + size of R&W segment + maybe some padding
	//make a pointer to stack around the top of our allocated memory
	/* TOP | STACK |
	*      | ***** |
	*      | ***** | } size = USER_PROC_STACK_SIZE 
	*      | ***** | Grows down
	*      | ***** |
	*      | ***** |
	*      | *Pad* | //Separates stack from others so no overwriting is done
	*      | ***** |
    *      | ***** |
    *      | ***** | } size = HEAP_SIZE NEED TO DECIDE
	*      | Heap  |
	*      | *Pad* |
	*      |  R&W  |
	*      |  Seg  |
	*      | ***** |
	*      |  RO   |
	*      |  Seg  |
	*      | ***** | <- process_mem	
	*/
	//Puts all the relevant segments into allocated region
	int i = 0;
	Boolean first_seg = TRUE;
	uint32_t entry_point_offset = 0;
	for(; i < (h->e_phnum); i++)
        {
		if(ph[i].p_type == PT_LOAD)
		{
			if(first_seg)
			{
				entry_point_offset = h->e_entry - ph[i].p_vaddr;
				first_seg = FALSE;
			}
				os_memcpy(file_pointer + ph[i].p_offset, current_pointer, (os_size_t)ph[i].p_memsz);
				current_pointer = current_pointer + ph[i].p_memsz;
		}
	}
		
	current_pointer += 4096;//Heap 
	pcb_p->heap_p = current_pointer;

	current_pointer += USER_PROC_STACK_SIZE + 0x1000; //Stack pointer	
	
	pcb_p->R13 = *current_pointer;
	pcb_p->R15 = (uint32_t)(entry_point_offset + process_mem);
	//entry_point_offset = entry_point_elf - addr_first;			
	//entry_point = process_mem + entry_point_offset	
}	

//Probably want to return a memory address rather than nothing.
//Take a process control block and pointer to the start of an ELF file in memory.
void load_file(pcb * process_control_block, uint32_t * file_pointer)
{
	Elf_Ehdr *h = (Elf_Ehdr *)kmalloc( sizeof(Elf_Ehdr));
	os_printf("%x\n", h);
	int i = read_elf_header(h, (unsigned char *)file_pointer);
	
	Elf_Phdr * ph = (Elf_Phdr *) kmalloc(h->e_phnum * sizeof(Elf_Phdr));	


	read_program_header_table(h, ph, (unsigned char *)file_pointer);
	
	
	Elf_Shdr *sh = (Elf_Shdr *) kmalloc(h->e_shnum * sizeof(Elf_Shdr));
	read_section_header_table(h, sh, file_pointer);
	parse_section_header_names(h, sh, file_pointer);
	
	allocate_process_memory(process_control_block, h, ph, file_pointer);
}
