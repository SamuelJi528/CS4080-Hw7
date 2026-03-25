#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
  free(chunk->code);
  free(chunk->lineEntries);
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
    array->capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
    array->values = (Value*)realloc(array->values, array->capacity * sizeof(Value));
  }
  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray* array) {
  free(array->values);
  initValueArray(array);
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
