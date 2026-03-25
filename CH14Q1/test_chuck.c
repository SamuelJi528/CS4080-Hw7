#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
  int line;
  int count;
} LineEntry;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  LineEntry* lineEntries;
  int lineEntryCount;
  int lineEntryCapacity;
} Chunk;

void initChunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lineEntries = NULL;
  chunk->lineEntryCount = 0;
  chunk->lineEntryCapacity = 0;
}

void freeChunk(Chunk* chunk) {
  free(chunk->code);
  free(chunk->lineEntries);
  initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
    chunk->code = (uint8_t*)realloc(chunk->code, chunk->capacity);
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
    chunk->lineEntryCapacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
    chunk->lineEntries = (LineEntry*)realloc(chunk->lineEntries,
        chunk->lineEntryCapacity * sizeof(LineEntry));
  }

  LineEntry* entry = &chunk->lineEntries[chunk->lineEntryCount++];
  entry->line = line;
  entry->count = 1;
}

int getLine(Chunk* chunk, int instruction) {
  int offset = 0;
  for (int i = 0; i < chunk->lineEntryCount; i++) {
    if (instruction < offset + chunk->lineEntries[i].count) {
      return chunk->lineEntries[i].line;
    }
    offset += chunk->lineEntries[i].count;
  }
  return -1;
}

int main() {
  Chunk chunk;
  initChunk(&chunk);

  printf("Writing 3 instructions on line 1:\n");
  writeChunk(&chunk, 0x01, 1);
  writeChunk(&chunk, 0x02, 1);
  writeChunk(&chunk, 0x03, 1);

  printf("Writing 2 instructions on line 2:\n");
  writeChunk(&chunk, 0x04, 2);
  writeChunk(&chunk, 0x05, 2);

  printf("Writing 1 instruction on line 3:\n");
  writeChunk(&chunk, 0x06, 3);

  printf("Writing 4 instructions on line 5:\n");
  writeChunk(&chunk, 0x07, 5);
  writeChunk(&chunk, 0x08, 5);
  writeChunk(&chunk, 0x09, 5);
  writeChunk(&chunk, 0x0A, 5);

  printf("\nCompressed line entries (run-length encoded):\n");
  printf("Total entries: %d\n", chunk.lineEntryCount);
  for (int i = 0; i < chunk.lineEntryCount; i++) {
    printf("  Entry %d: line %d, count %d\n", i,
           chunk.lineEntries[i].line,
           chunk.lineEntries[i].count);
  }

  printf("\nRetrieving line numbers for each instruction:\n");
  for (int i = 0; i < chunk.count; i++) {
    int line = getLine(&chunk, i);
    printf("  Instruction %d: line %d\n", i, line);
  }

  printf("\nMemory comparison:\n");
  printf("  Old approach: %d ints x 4 bytes = %d bytes\n",
         chunk.count, chunk.count * 4);
  printf("  New approach: %d entries x 8 bytes = %d bytes\n",
         chunk.lineEntryCount, chunk.lineEntryCount * 8);

  freeChunk(&chunk);
  return 0;
}
