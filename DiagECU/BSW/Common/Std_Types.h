/**********************************************************
 * @file    Std_Types.h
 * @brief   Kiểu dữ liệu chuẩn AUTOSAR (AUTOSAR Standard Types)
 *
 * @version 1.0.0
 * @date    20/6/2026
 * @author  Duy Dang
 **********************************************************/

#ifndef STD_TYPES_H
#define STD_TYPES_H

#include <stdint.h>

/* ===========================================================
 * Hằng số Boolean
 * ===========================================================*/
#ifndef FALSE
#define FALSE (0u)
#endif

#ifndef TRUE
#define TRUE (1u)
#endif

/* ===========================================================
 * Trạng thái Module (STD_ON / STD_OFF)
 * ===========================================================*/
#ifndef STD_ON
#define STD_ON (1u)
#endif

#ifndef STD_OFF
#define STD_OFF (0u)
#endif

/* ===========================================================
 * Kiểu trả về chuẩn (Std_ReturnType)
 * ===========================================================*/
typedef uint8_t Std_ReturnType;

#define E_OK     ((Std_ReturnType)0x00u)
#define E_NOT_OK ((Std_ReturnType)0x01u)

#ifndef NULL_PTR
#define NULL_PTR ((void *)0)
#endif

/* ===========================================================
 * Kiểu Boolean (boolean)
 * ===========================================================*/
typedef uint8_t boolean;

/* ===========================================================
 * Kiểu số nguyên có kích thước cố định
 * ===========================================================*/
typedef uint8_t   uint8;      /* Số nguyên không dấu 8-bit  (0..255)        */
typedef uint16_t  uint16;     /* Số nguyên không dấu 16-bit (0..65535)      */
typedef uint32_t  uint32;     /* Số nguyên không dấu 32-bit (0..4294967295) */
typedef uint64_t  uint64;     /* Số nguyên không dấu 64-bit                 */

typedef int8_t    int8;       /* Số nguyên có dấu 8-bit  (-128..127)        */
typedef int16_t   int16;      /* Số nguyên có dấu 16-bit (-32768..32767)    */
typedef int32_t   int32;      /* Số nguyên có dấu 32-bit                    */
typedef int64_t   int64;      /* Số nguyên có dấu 64-bit                    */

/* ===========================================================
 * Kiểu số thực dấu phẩy động
 * ===========================================================*/
typedef float     float32;    /* Số thực 32-bit (IEEE 754 single precision)  */
typedef double    float64;    /* Số thực 64-bit (IEEE 754 double precision)  */

#endif /* STD_TYPES_H */
