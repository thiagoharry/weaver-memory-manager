#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <windows.h> // Include 'GetSystemInfo'
#endif
#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h> // Include 'sysconf' || 'getpagesize'
#include <pthread.h>
#endif
#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
#endif

#include "../src/memory.h"

int numero_de_testes = 0, acertos = 0, falhas = 0;

size_t count_utf8_code_points(const char *s) {
    size_t count = 0;
    while (*s) {
        count += (*s++ & 0xC0) != 0x80;
    }
    return count;
}

void assert(char *descricao, bool valor){
  char pontos[72], *s = descricao;
  size_t tamanho_string = 0;
  int i;
  while(*s)
    tamanho_string += (*s++ & 0xC0) != 0x80;
  pontos[0] = ' ';
  for(i = 1; i < 71 - tamanho_string; i ++)
    pontos[i] = '.';
  pontos[i] = '\0';
  numero_de_testes ++;
  printf("%s%s", descricao, pontos);
  if(valor){
#if defined(__unix__) && !defined(__EMSCRIPTEN__)
    printf("\e[32m[OK]\033[0m\n");
#else
    printf("[OK]\n");
#endif
    acertos ++;
  }
  else{
#if defined(__unix__) && !defined(__EMSCRIPTEN__)
    printf("\033[0;31m[FAIL]\033[0m\n");
#else
    printf("[FAIL]\n");
#endif
    falhas ++;
  }
}

void imprime_resultado(void){
  printf("\n%d tests: %d sucess, %d fails\n\n",
	 numero_de_testes, acertos, falhas);
}

/* Globais */
size_t page_size;
static void set_page_size(void){
#if defined(__unix__)
  page_size = sysconf(_SC_PAGESIZE);
#endif
#if defined(__APPLE__)
  page_size = getpagesize();
#endif
#if defined(_WIN32)
  {
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    page_size = info.dwPageSize;
  }
#endif
#if defined(__EMSCRIPTEN__)
  page_size = 64 * 1024; // 64 KiB
#endif
  printf("Page size: %d\n", (int) page_size);
}


/* Início dos Testes */
struct arena_header{
  #if defined(__unix__) || defined(__APPLE__)
  pthread_mutex_t mutex;
#endif
#if defined(_WIN32)
  CRITICAL_SECTION mutex;
#endif
  void *left_free, *right_free;
  void *left_point, *right_point;
  size_t remaining_space, total_size, right_allocations, left_allocations;
#if defined(W_DEBUG_MEMORY)
  size_t smallest_remaining_space;
#endif
};

void test_Wcreate_arena(void){
  size_t size_header = sizeof(struct arena_header);
  void *arena1, *arena2, *arena3;
  size_t real_size1, real_size2;
  struct arena_header *header1, *header2;
  arena1 = _Wcreate_arena(0);
  arena2 = _Wcreate_arena(10 * page_size - 1);
  arena3 = _Wcreate_arena(8);
  header1 = (struct arena_header *) arena1;
  header2 = (struct arena_header *) arena2;
  real_size1 = header1 -> remaining_space + size_header;
  real_size2 = header2 -> remaining_space + size_header;
  assert("Arena size is consistent",
	 header1 -> total_size == real_size1 &&
	 header2 -> total_size == real_size2);
  assert("Arena minimal size is >= header size and multiple of page size",
	 arena1 != NULL &&
	 real_size1 >= size_header &&
	 real_size1  %  page_size == 0);
  assert("Arena size is always multiple of page size",
	 arena2 != NULL &&
	 real_size2 == 10 * page_size);
  assert("Arena has pointers to free space initialized",
	 header2 -> right_free == (void *)
	 (((char *) arena2) + real_size2 - 1) &&
	 header1 -> right_free == (void *)
	 (((char *) arena1) + real_size1 - 1) &&
	 header2 -> left_free == (void *)
	 ((char *) arena2 + size_header) &&
	 header1 -> left_free == (void *)
	 ((char *) arena1 + size_header) &&
	 header1 -> left_point == NULL &&
	 header2 -> left_point == NULL &&
	 header1 -> right_point == NULL &&
	 header2 -> right_point == NULL);
  assert("No memory leak in creation and destruction of arena",
	 _Wdestroy_arena(arena3));
  _Wdestroy_arena(arena1);
  _Wdestroy_arena(arena2);
}

void test_using_arena(void){
  bool error = false;
  void *arena = _Wcreate_arena(10 * page_size);
  int i;
  for(i = sizeof(struct arena_header); i < 10 * page_size; i ++)
    ((char *) arena)[i] = 'A';
  for(i = sizeof(struct arena_header); i < 10 * page_size; i ++)
    if(((char *) arena)[i] != 'A')
      error = true;
  assert("Write and read data in arena", !error);
  _Wdestroy_arena(arena);
}

void test_alignment(void){
  int i, align = 1;
  void *arena = _Wcreate_arena(2048);
  long long a[14];
  for(i = 0; i < 7; i ++){
    align *= 2;
    a[i] = (long long) _Walloc(arena, align, 0, 4);
  }
  align  = 1;
  for(i = 7; i < 14; i ++){
    align *= 2;
    a[i] = (long long) _Walloc(arena, align, 1, 4);
  }
  assert("Testing left memory alignment",
	 a[0] % 2 == 0 && a[1] % 4 == 0 && a[2] % 8 == 0 && a[3] % 16 == 0 &&
	 a[4] % 32 == 0 && a[5] % 64 == 0 && a[6] % 128 == 0);
  assert("Testing right memory alignment",
	 a[7] % 2 == 0 && a[8] % 4 == 0 && a[9] % 8 == 0 && a[10] % 16 == 0 &&
	 a[11] % 32 == 0 && a[12] % 64 == 0 && a[13] % 128 == 0);
  _Wdestroy_arena(arena);
}

void test_allocation(void){
  int i, j;
  bool allocations_inside_arena = true, can_read_write = true,
    success_in_allocation = true, correct_address = true;
  char *a[20];
  void *arena = _Wcreate_arena(16 * 20 + sizeof(struct arena_header *));
  void *arena2 = _Wcreate_arena(page_size);
  void *p = _Walloc(arena, 0, 1, page_size + 1);
  char write = 'a';
  struct arena_header *header = (struct arena_header *) arena;
  size_t size = header -> remaining_space;
  for(i = 0; i < 10; i ++){
    a[i] = (char *) _Walloc(arena, 0, 0, 16);
    if(a[i] == NULL)
      success_in_allocation = false;
    if(i > 0 && ((char *) a[i] != (char *) a[i-1] + 16)){
      correct_address = false;
    }
    size -= 16;
  }
  for(i = 10; i < 20; i ++){
    a[i] = (char *) _Walloc(arena, 0, 1, 16);
    if(a[i] == NULL)
      success_in_allocation = false;
    if(i > 10 && ((char *) a[i] != (char *) a[i-1] - 16)){
      correct_address = false;
    }
    size -= 16;
  }
  for(i = 0; i < 20; i ++){
    if((char *) a[i] < ((char *) arena + sizeof(struct arena_header)) ||
       (char *) a[i] + 15 >= (char *) arena + header -> total_size)
      allocations_inside_arena = false;
    for(j = 0; j < 16; j ++)
      a[i][j] = write;
    write ++;
  }
  write = 'a';
  for(i = 0; i < 20; i ++){
    for(j = 0; j < 16; j ++)
      if(a[i][j] != write){
	can_read_write = false;
      }
    write ++;
  }
  assert("Sequential allocations are successful", success_in_allocation);
  assert("Arena keeps the remaining space size updated",
	 header -> remaining_space == size);
  assert("Allocation returns correct addresses", correct_address);
  assert("Allocated arena is inside arena boundaries",
	 allocations_inside_arena);
  assert("Can read and write in allocated memory", can_read_write);
  assert("Impossible allocations return NULL", p == NULL);
  assert("Impossible allocations don't leak memory",
	 _Wdestroy_arena(arena2));
  assert("Memory leaks are detectable",	!_Wdestroy_arena(arena));
}

struct memory_point{
  size_t allocations; // Left or right
  struct memory_point *last_memory_point;
};

void test_memorypoint(void){
  void *arena = _Wcreate_arena(10 * page_size);
  struct arena_header *header = (struct arena_header *) arena;
  bool pointers_ok = true;
  _Wmempoint(arena, 0, 0);
  _Wmempoint(arena, 0, 1);
  pointers_ok = pointers_ok &&  header -> right_point != NULL;
  pointers_ok = pointers_ok &&  header -> left_point != NULL;
  _Wtrash(arena, 0);
  _Wtrash(arena, 1);
  assert("Testing trashable heap", pointers_ok && _Wdestroy_arena(arena));
}

void test_memorypoint2(void){
  void *arena = _Wcreate_arena(10 * page_size), *p1, *p2, *p3, *p4;
  struct arena_header *header = (struct arena_header *) arena;
  size_t space = header -> remaining_space;
  // Left arena
  p1 = _Walloc(arena, 0, 0, 8 * page_size);
  _Wtrash(arena, 0);
  p1 = _Walloc(arena, 0, 0, 8 * page_size);
  _Wtrash(arena, 0);
  p2 = _Walloc(arena, 0, 0, 4 * page_size);
  _Wmempoint(arena, 0, 0);
  p2 = _Walloc(arena, 0, 0, 4 * page_size);
  _Wtrash(arena, 0);
  p2 = _Walloc(arena, 0, 0, 4 * page_size);
  _Wtrash(arena, 0);
  // Right arena
  p3 = _Walloc(arena, 0, 1, 8 * page_size);
  _Wtrash(arena, 1);
  p3 = _Walloc(arena, 0, 1, 8 * page_size);
  _Wtrash(arena, 1);
  p4 = _Walloc(arena, 0, 1, 4 * page_size);
  _Wmempoint(arena, 0, 1);
  p4 = _Walloc(arena, 0, 1, 4 * page_size);
  _Wtrash(arena, 1);
  p4 = _Walloc(arena, 0, 1, 4 * page_size);
  _Wtrash(arena, 1);
  assert("Trash function freeing space", p1 != NULL && p2 != NULL &&
	 p3 != NULL && p4 != NULL);
  assert("Memory is cleaned after trash function",
	 header -> remaining_space == space &&
	 header -> left_allocations == 0 &&
	 header -> right_allocations == 0 &&
	 header -> right_point == NULL &&
	 header -> left_point == NULL);
  _Wdestroy_arena(arena);
}

void test_memorypoint3(void){
  void *arena = _Wcreate_arena(10 * page_size);
  char *p1, *p2, *p3, *p4;
  bool integrity_ok = true;
  int i;
  // Left memory
  _Wmempoint(arena, 4, 0);
  p1 = (char *) _Walloc(arena, 4, 0, 16);
  for(i = 0; i < 16; i ++)
    p1[i] = 'A';
  _Wmempoint(arena, 4, 0);
  p2 = _Walloc(arena, 4, 0, 16);
  for(i = 0; i < 16; i ++)
    p2[i] = 'B';
  _Wtrash(arena, 0);
  p2 = _Walloc(arena, 4, 0, 16);
  for(i = 0; i < 16; i ++)
    p2[i] = 'C';
  for(i = 0; i < 16; i ++)
    if(p1[i] != 'A')
      integrity_ok = false;
  for(i = 0; i < 16; i ++)
    if(p2[i] != 'C')
      integrity_ok = false;
  _Wtrash(arena, 0);
  // Right memory
  _Wmempoint(arena, 4, 1);
  p3 = (char *) _Walloc(arena, 4, 1, 16);
  for(i = 0; i < 16; i ++)
    p3[i] = 'A';
  _Wmempoint(arena, 4, 1);
  p4 = _Walloc(arena, 4, 1, 16);
  for(i = 0; i < 16; i ++)
    p4[i] = 'B';
  _Wtrash(arena, 1);
  p4 = _Walloc(arena, 4, 1, 16);
  for(i = 0; i < 16; i ++)
    p4[i] = 'C';
  for(i = 0; i < 16; i ++)
    if(p3[i] != 'A')
      integrity_ok = false;
  for(i = 0; i < 16; i ++)
    if(p4[i] != 'C')
      integrity_ok = false;
  _Wtrash(arena, 1);
  assert("Testing integrity with memory points", integrity_ok &&
	 _Wdestroy_arena(arena));
}

void test_memorypoint4(void){
  void *arena1 = _Wcreate_arena(10 * page_size);
  void *arena2 = _Wcreate_arena(10 * page_size);
  _Wmempoint(arena1, 2, 0);
  _Wtrash(arena2, 0);
  _Wtrash(arena2, 0);
  _Wtrash(arena2, 0);
  _Wtrash(arena2, 1);
  _Wtrash(arena2, 1);
  _Wtrash(arena2, 1);
  assert("Memory leaks from memory points are detectable",
	 !_Wdestroy_arena(arena1));
  assert("Wtrash function in empty memory stacks don't cause problems",
	 _Wdestroy_arena(arena2));
}

#define THREAD_NUMBER 1000

#if defined(_WIN32)
DWORD _WINAPI thread_function(void *arena){
#else
void *thread_function(void *arena){
#endif
  void *data = NULL;
  int size, i;
  if(rand() % 10){
    size = rand() % 128;
    data = _Walloc(arena, 4, rand() % 2, size);
    if(data != NULL)
      for(i = 0; i < size; i ++)
	((char *) data)[i] = 'W';
  }
  else{
    _Wmempoint(arena, rand() % 2, 0);
  }
#if defined(_WIN32)
  return 0;
#else
  return NULL;
#endif
}
  
void test_threads(void){
#if defined(_WIN32)
  HANDLE thread[THREAD_NUMBER];
#else
  pthread_t thread[THREAD_NUMBER];
#endif
  int i;
  void *arena1 = _Wcreate_arena(500 * THREAD_NUMBER);
  for(i = 0; i < THREAD_NUMBER; i ++)
#if defined(_WIN32)
    thread[i] = CreateThread(NULL, 0, thread_function, arena1, 0, NULL);
#else
    pthread_create(&(thread[i]), NULL, thread_function, arena1);
#endif
  for(i = 0; i < THREAD_NUMBER; i ++)
#if defined(_WIN32)
    _WaitForSingleObject(thread[i], INFINITE);
#else
    pthread_join(thread[i], NULL);  
#endif
  for(i = 0; i < THREAD_NUMBER; i ++){
    _Wtrash(arena1, 0);
    _Wtrash(arena1, 1);
  }
  assert("Memory manager works when used in multiple threads",
	 _Wdestroy_arena(arena1));
  
}

void test_memorypoint5(void){
  void *arena = _Wcreate_arena(10 * page_size);
  void *t1, *t2;
  bool integrity_ok = true;
  // Left memory
  t1 = ((struct arena_header *) arena) -> left_free;
  _Wmempoint(arena, 4, 0);
  _Walloc(arena, 4, 0, 16);
  t2 = ((struct arena_header *) arena) -> left_free;
  _Wmempoint(arena, 4, 0);
  _Walloc(arena, 4, 0, 16);
  _Wtrash(arena, 0);
  if(t2 != ((struct arena_header *) arena) -> left_free){
    printf("%p != %p\n", t2, ((struct arena_header *) arena) -> left_free);
    integrity_ok = false;
  }
  _Wtrash(arena, 0);
  if(t1 != ((struct arena_header *) arena) -> left_free){
    printf("%p != %p\n", t1, ((struct arena_header *) arena) -> left_free);
    integrity_ok = false;
  }
  // Right memory
  t1 = ((struct arena_header *) arena) -> right_free;
  _Wmempoint(arena, 4, 1);
  _Walloc(arena, 4, 1, 16);
  t2 = ((struct arena_header *) arena) -> right_free;
  _Wmempoint(arena, 4, 1);
  _Walloc(arena, 4, 1, 16);
  _Wtrash(arena, 1);
  if(t2 != ((struct arena_header *) arena) -> right_free){
    printf("(3) %p != %p\n", t2, ((struct arena_header *) arena) -> right_free);
    integrity_ok = false;
  }
  _Wtrash(arena, 1);
  if(t1 != ((struct arena_header *) arena) -> right_free)
    integrity_ok = false;
  assert("Testing restauration of pointers after Wtrash", integrity_ok &&
	 _Wdestroy_arena(arena));
}
 
int main(int argc, char **argv){
  int semente;
  if(argc > 1)
    semente = atoi(argv[1]);
  else
    semente = time(NULL);
  srand(semente);
  printf("Starting tests. Seed: %d\n", semente);
  set_page_size();
  test_Wcreate_arena();
  test_using_arena();
  test_alignment();
  test_allocation();
  test_memorypoint();
  test_memorypoint2();
  test_memorypoint3();
  test_memorypoint4();
  test_memorypoint5();
#if !defined(__EMSCRIPTEN__)
  test_threads();
#endif
  imprime_resultado();
  return 0;
}
