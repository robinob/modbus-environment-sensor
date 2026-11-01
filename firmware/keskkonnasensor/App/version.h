/**
 * @file    version.h
 * @brief   Firmware version, readable over Modbus (input register 20).
 *
 * Raise MINOR for every release that only adds things, MAJOR when the
 * register map changes in a way that can break an existing master.
 */
#ifndef VERSION_H
#define VERSION_H

#define FW_VERSION_MAJOR 0u
#define FW_VERSION_MINOR 1u

#endif /* VERSION_H */
