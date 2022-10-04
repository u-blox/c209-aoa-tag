/*
 * Copyright 2022 u-blox
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

#define MAJOR_VERSION 2
#define MINOR_VERSION 0
#define SUB_MINOR_VERSION 0


#define TO_STRING_NX(x) #x
#define TO_STRING(x) TO_STRING_NX(x)

#if !defined(BUILD_NUMBER)
const char ubxVersionString[] = TO_STRING(MAJOR_VERSION) "." TO_STRING(
                                       MINOR_VERSION) "." TO_STRING(SUB_MINOR_VERSION);
#else
const char ubxVersionString[] = TO_STRING(MAJOR_VERSION) "." TO_STRING(
                                       MINOR_VERSION) "." TO_STRING(SUB_MINOR_VERSION) "-" TO_STRING(BUILD_NUMBER);
#endif