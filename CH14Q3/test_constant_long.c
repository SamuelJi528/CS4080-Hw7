#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define HEAP_SIZE (1024 * 1024)

typedef struct Block {
  size_t size;
  int free;
  struct Block* next;
} Block;

static char* heap = NULL;
static Block* freeList = NULL;

void initHeap() {
  heap = (char*)malloc(HEAP_SIZE);
  freeList = (Block*)heap;
  freeList->size = HEAP_SIZE - sizeof(Block);
  freeList->free = 1;
  freeList->next = NULL;
}

void* myAlloc(size_t size) {
  Block* curr = freeList;
  
  while (curr) {
    if (curr->free && curr->size >= size) {
      if (curr->size > size + sizeof(Block)) {
        Block* newBlock = (Block*)((char*)(curr + 1) + size);
        newBlock->size = curr->size - size - sizeof(Block);
        newBlock->free = 1;
        newBlock->next = curr->next;
        curr->next = newBlock;
        curr->size = size;
      }
      curr->free = 0;
      return (void*)(curr + 1);
    }
    curr = curr->next;
  }
  return NULL;
}

void myFree(void* ptr) {
  if (!ptr) return;
  Block* block = (Block*)ptr - 1;
  block->free = 1;
  
  if (block->next && block->next->free) {
    block->size += sizeof(Block) + block->next->size;
    block->next = block->next->next;
  }
  
  Block* prev = freeList;
  if (prev == block) return;
  while (prev && prev->next != block) {
    prev = prev->next;
  }
  if (prev && prev->free) {
    prev->size += sizeof(Block) + block->size;
    prev->next = block->next;
  }
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    myFree(pointer);
    return NULL;
  }
  
  if (pointer == NULL) {
    return myAlloc(newSize);
  }
  
  void* newPtr = myAlloc(newSize);
  if (newPtr == NULL) return NULL;
  
  memcpy(newPtr, pointer, oldSize < newSize ? oldSize : newSize);
  myFree(pointer);
  return newPtr;
}

typedef struct {
  int line;
  int count;
} LineEntry;

typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_LONG,
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  LineEntry* lineEntries;
  int lineEntryCount;
  int lineEntryCapacity;
} Chunk;

typedef double Value;

typedef struct {
  int count;
  int capacity;
  Value* values;
} ValueArray;

void initChunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lineEntries = NULL;
  chunk->lineEntryCount = 0;
  chunk->lineEntryCapacity = 0;
}

void freeChunk(Chunk* chunk) {
  myFree(chunk->code);
  myFree(chunk->lineEntries);
  initChunk(chunk);
}

void initValueArray(ValueArray* array) {
  array->values = NULL;
  array->count = 0;
  array->capacity = 0;
}

void writeValueArray(ValueArray* array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    int newCapacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
    array->values = (Value*)reallocate(array->values, oldCapacity * sizeof(Value), newCapacity * sizeof(Value));
    array->capacity = newCapacity;
  }
  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray* array) {
  myFree(array->values);
  initValueArray(array);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    int newCapacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
    chunk->code = (uint8_t*)reallocate(chunk->code, oldCapacity, newCapacity);
    chunk->capacity = newCapacity;
  }

  chunk->code[chunk->count] = byte;
  chunk->count++;

  if (chunk->lineEntryCount > 0 &&
      chunk->lineEntries[chunk->lineEntryCount - 1].line == line) {
    chunk->lineEntries[chunk->lineEntryCount - 1].count++;
    return;
  }

  if (chunk->lineEntryCapacity < chunk->lineEntryCount + 1) {
    int oldCapacity = chunk->lineEntryCapacity;
    int newCapacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
    chunk->lineEntries = (LineEntry*)reallocate(chunk->lineEntries,
        oldCapacity * sizeof(LineEntry), newCapacity * sizeof(LineEntry));
    chunk->lineEntryCapacity = newCapacity;
  }

  LineEntry* entry = &chunk->lineEntries[chunk->lineEntryCount++];
  entry->line = line;
  entry->count = 1;
}

int addConstant(Chunk* chunk, ValueArray* constants, Value value) {
  writeValueArray(constants, value);
  return constants->count - 1;
}

void writeConstant(Chunk* chunk, ValueArray* constants, Value value, int line) {
  int constant = addConstant(chunk, constants, value);

  if (constant < 256) {
    writeChunk(chunk, OP_CONSTANT, line);
    writeChunk(chunk, (uint8_t)constant, line);
  } else {
    writeChunk(chunk, OP_CONSTANT_LONG, line);
    writeChunk(chunk, (constant >> 16) & 0xff, line);
    writeChunk(chunk, (constant >> 8) & 0xff, line);
    writeChunk(chunk, constant & 0xff, line);
  }
}

void printValue(Value value) {
  printf("%g", value);
}

void disassembleChunk(Chunk* chunk, ValueArray* constants, const char* name) {
  printf("== %s ==\n", name);
  
  int offset = 0;
  while (offset < chunk->count) {
    if (offset > 0) printf("\n");
    printf("%04d ", offset);

    uint8_t instruction = chunk->code[offset];
    
    if (instruction == OP_CONSTANT) {
      uint8_t constant = chunk->code[offset + 1];
      printf("%-16s %4d '", "OP_CONSTANT", constant);
      printValue(constants->values[constant]);
      printf("'");
      offset += 2;
    } else if (instruction == OP_CONSTANT_LONG) {
      int constant = (chunk->code[offset + 1] << 16) |
                     (chunk->code[offset + 2] << 8) |
                     chunk->code[offset + 3];
      printf("%-16s %4d '", "OP_CONSTANT_LONG", constant);
      printValue(constants->values[constant]);
      printf("'");
      offset += 4;
    } else if (instruction == OP_RETURN) {
      printf("%-16s", "OP_RETURN");
      offset += 1;
    } else {
      printf("Unknown opcode %d", instruction);
      offset += 1;
    }
  }
  printf("\n");
}

int main() {
  initHeap();
  
  Chunk chunk;
  ValueArray constants;
  initChunk(&chunk);
  initValueArray(&constants);

  printf("Adding 300 constants using writeConstant()...\n");
  for (int i = 0; i < 300; i++) {
    writeConstant(&chunk, &constants, i * 1.5, 1);
  }

  printf("Done. Chunk size: %d bytes\n", chunk.count);
  printf("Constants: %d\n\n", constants.count);

  printf("First 5 constants (using OP_CONSTANT):\n");
  disassembleChunk(&chunk, &constants, "Constants 0-4");

  printf("\n\nConstants 250-254 (using OP_CONSTANT_LONG):\n");
  int offset = 0;
  for (int i = 0; i < 250; i++) {
    if (chunk.code[offset] == OP_CONSTANT) {
      offset += 2;
    } else {
      offset += 4;
    }
  }

  Chunk tempChunk;
  tempChunk.code = &chunk.code[offset];
  tempChunk.count = chunk.count - offset;
  disassembleChunk(&tempChunk, &constants, "Constants 250-254");

  freeChunk(&chunk);
  freeValueArray(&constants);
  return 0;
}
