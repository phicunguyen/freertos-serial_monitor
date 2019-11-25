/**
 * @file inv_hal_serial_api.h
 *
 * @brief register access APIs
 *
 *****************************************************************************/

#ifndef INV_HAL_SERIAL_API_H
#define INV_HAL_SERIAL_API_H
#include <stdio.h>
#include <conio.h>

 /* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/***** #include statements ***************************************************/
/***** local type definitions ************************************************/
/***** public functions ******************************************************/

bool serial_devices_get(char **p_connection[10]);

#endif //SI_HAL_SERIAL_API_H
