#include <stdio.h>
#include <conio.h>
#include <windows.h>
#include <strsafe.h>
#include <stdbool.h>

/* FreeRTOS.org includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "serial.h"

/* Demo includes. */
#include "supporting_functions.h"
#define MAX_SERIAL_PORT		10
void vAdapterTask(void *pvParameters) {
	char *serial_list[MAX_SERIAL_PORT] = { NULL };
	char **p_ptr;
	pvParameters = pvParameters;

	while (true) {
		//polling the window api to get the stm32 ports
		if (serial_devices_get(&serial_list)) {
			p_ptr = serial_list;
			if (*p_ptr != NULL) {
				printf("\nlist of stm32 comports:\n");
				while (*p_ptr) {
					printf("%s\n", *p_ptr++);
				}
			}
		}
		vTaskDelay(100);
	}
}

int main()
{	
	//task to monitor connection
	xTaskCreate(vAdapterTask, "connection", 1024, NULL, 1, NULL);
	vTaskStartScheduler();
	for (;; );
}
