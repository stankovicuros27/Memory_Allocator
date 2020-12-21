#include <stdlib.h>
#include <windows.h>

#include "slab.h"
#include "test.h"


void run_threads(void(*work)(void*), struct data_s* data, int num) {
	HANDLE* threads = (HANDLE *)malloc(sizeof(HANDLE) * num);
	struct data_s* private_data = (struct data_s*)malloc(sizeof(struct data_s) * num);
	for (int i = 0; i < num; i++) {
		private_data[i] = *(struct data_s*) data;
		private_data[i].id = i + 1;
		threads[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)work, &private_data[i], 0, NULL);
	}

	for (int i = 0; i < num; i++) {
		WaitForSingleObject(threads[i], INFINITE);
		CloseHandle(threads[i]);
	}
	free(threads);
	free(private_data);
}

