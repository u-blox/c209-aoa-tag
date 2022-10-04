/*
 * Copyright 2021 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __AT_HOST_H
#define __AT_HOST_H

typedef void (*atOutput)(char *str);

/**
 * @brief   Init the test and configuration UART interface.
 * @details Enables the UART and initializes the command parser. Configuration and test AT commands will be responded to.
 *          If no valid command is sent within the first 10 seconds the UART will be disabled and no commands accepted.
 *          Once a valid AT command is sent the module, it will stay in test/configuration mode until reset, no 10 seconds limit.
 */
int atHostStart(void);

/**
 * @brief   Input an AT command to the AT command handler.
 * @details Must be a full AT command as fragmented AT commands are not supported.
 *          If it was a valid AT command it will be handled accordingly.
 *
 * @param   inAtBuf     pointer to the full AT command
 * @param   commandLen  The length of inAtBuf
 * @param   output      Function where the output from AT command handler will be sent. May be called multiple times.
 *                       Will not be called after atHostHandleCommand function exits.
 */
bool atHostHandleCommand(const uint8_t *const inAtBuf, uint32_t commandLen, atOutput output);

#endif