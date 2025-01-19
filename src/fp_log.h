#pragma once

/**
 * Structured logging into a temporary data structure.
 *
 * Logs are flushed at a specified time, e.g. after rendering a frame.
 */

#include "fp_allocator.h"

#include <Windows.h>

struct LogEntry {
	i16 length;
	char message[0];
};

struct Log {
	ArenaAllocator allocator;

	void add(char* message, i32 length) {
		if (length > 60000) {
			DebugBreak();
		}

		LogEntry* entry = (LogEntry*)allocator.allocate(sizeof(LogEntry) + length + 1);
		entry->length = length;
		CopyMemory(entry->message, message, length);
	}

	LogEntry* beginFlush() {
		if (allocator.used == 0) return nullptr;

		// Mark the end with a dummy
		add("", 0);

		return (LogEntry*)allocator.data;
	}

	void endFlush() {
		allocator.reset();
	}
};

LogEntry* next(LogEntry* entry) {
	if (entry->length == 0) return nullptr;

	u8* base = (u8*)entry;
	base += sizeof(LogEntry) + entry->length + 1;
	return (LogEntry*)base;
}

Log createLog(void* logMemory, int logMemorySize) {
	Log log = {};

	log.allocator = createArenaAllocator(logMemory, logMemorySize);

	return log;
}

char* printSingle(char* buffer, int value) {
	char temp[16] = {};

	int i = 0;
	bool wasNegative = value < 0;
	if (wasNegative) {
		value = -value;
	}

	do {
		int digit = value % 10;
		value = value / 10;

		temp[i] = digit + '0';
		i = i + 1;
	} while (value > 0);

	if (wasNegative) {
		temp[i] = '-';
		i = i + 1;
	}

	i = i - 1;
	while (i >= 0) {
		*buffer = temp[i];
		i = i - 1;
		buffer += 1;
	}
	return buffer;
}

char* printSingle(char* buffer, const char* string) {
	while (*string) {
		*buffer = *string;
		++string;
		++buffer;
	}
	return buffer;
}

char* print(char* buffer) { 
	*buffer = '\0';
	return buffer + 1; 
}

template <typename T, typename... Types>
char* print(char* buffer, T var1, Types... var2)
{
	buffer = printSingle(buffer, var1);

	return print(buffer, var2...);
}
