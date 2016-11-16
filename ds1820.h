/**
 * Interface for DS1820 one-wire bus temp sensors
 */
#ifndef __DS1820_H__
#define __DS1820_H_

void ds1820_search_bus();
char ds1820_start_temps();
char ds1820_read_temps();

#endif  // __DS1820_H_
