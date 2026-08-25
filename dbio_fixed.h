/**
 * @file dbio_fixed.h
 * @brief Отладочный ввод-вывод через UART1 (исправленная версия dbio.h).
 *
 * ВНИМАНИЕ: модуль полностью заменяет dbio.c / dbio.h и экспортирует те же
 * имена функций. Одновременно собирать dbio.c и dbio_fixed.c нельзя —
 * исключите dbio.c из проекта и замените в main.c
 * `#include "dbio.h"` на `#include "dbio_fixed.h"`.
 *
 * Совместимость с прежним API:
 *   - dbio_getstring()  — сигнатура не изменилась;
 *   - dbio_putstring()  — параметр стал `const char*` (прежние вызовы
 *                         компилируются без правок);
 *   - dbio_init()       — теперь возвращает код ошибки; прежний вызов
 *                         `dbio_init();` остаётся валидным.
 *   Числовые значения кодов возврата (0 / -1 / -2) сохранены, поэтому
 *   проверки вида `if(dbio_getstring(...) > 0)` работают как раньше.
 */

#ifndef DBIO_FIXED_H
#define	DBIO_FIXED_H

#include "system.h"

#define     TxIntEn()            U1_TxIntEn()
#define     RxIntEn()            U1_RxIntEn()
#define     TxIntDis()           U1_TxIntDis()
#define     RxIntDis()           U1_RxIntDis()
#define     CheckTxPermission()  U1_CheckTxPermission()
#define     SendByte(byte)       U1_SendByte(byte)
#define     GetByte()            U1_GetByte()

/**
 * @enum DBIO_STATUS
 * @brief Коды возврата dbio_getstring() / dbio_putstring().
 *
 * Положительное значение — количество принятых/поставленных в очередь
 * символов без учёта завершающего '\0'.
 */
typedef enum DBIO_STATUS {
    DBIO_PENDING    =  0,  ///< getstring: приём строки продолжается
                           ///< putstring: отправлять нечего (пустая строка)
    DBIO_PARAM_ERR  = -1,  ///< Некорректные аргументы
    DBIO_NO_SPACE   = -1,  ///< putstring: нет места в очереди передачи
    DBIO_NO_DATA    = -2,  ///< getstring: приёмный буфер пуст
} DBIO_STATUS;

/**
 * @enum DBIO_INIT_STATUS
 * @brief Коды возврата dbio_init().
 */
typedef enum DBIO_INIT_STATUS {
    DBIO_INIT_OK      = 0, ///< Инициализация выполнена
    DBIO_INIT_BUF_ERR = 1, ///< Не удалось инициализировать кольцевой буфер
    DBIO_INIT_CBK_ERR = 2, ///< Таблица обработчиков прерываний исчерпана
} DBIO_INIT_STATUS;

/**
 * @brief Инициализация буферов, обработчиков прерываний и UART1.
 * @return Код #DBIO_INIT_STATUS
 */
uint8_t dbio_init(void);

/**
 * @brief Неблокирующий приём строки по паузе между символами.
 *
 * Строка отдаётся, когда объём данных в приёмном буфере не менялся в течение
 * @p timeout мс, либо сразу, если данных набралось больше, чем помещается
 * в @p str.
 *
 * @param[out] str    Буфер вызывающего, не менее @p Nmax байт
 * @param[in]  Nmax   Размер буфера ВМЕСТЕ с местом под '\0' (>= 2)
 * @param[in]  timeout Межсимвольная пауза, мс
 * @return Длина строки без '\0' (> 0), либо код #DBIO_STATUS
 *
 * @note Состояние таймаута (`timetmp` / `buf_len_prev`) статическое и общее
 *       для всех вызовов, поэтому функция рассчитана на единственного
 *       вызывающего. При вызове из разных мест с разными @p Nmax поведение
 *       таймаута будет некорректным (переполнения буфера при этом не будет).
 */
int16_t dbio_getstring(char* str, uint16_t Nmax, uint16_t timeout);

/**
 * @brief Постановка строки в очередь передачи (неблокирующая).
 *
 * @param[in] str  Строка; копируется в очередь целиком до '\0' или до @p Nmax
 * @param[in] Nmax Максимальное число просматриваемых символов
 * @return Число поставленных в очередь символов, либо код #DBIO_STATUS
 */
int16_t dbio_putstring(const char* str, uint16_t Nmax);

/** @brief Число аппаратных переполнений приёмника (OERR). */
uint16_t dbio_get_hw_overruns(void);
/** @brief Число ошибок кадра (FERR) — обычно рассинхрон скорости обмена. */
uint16_t dbio_get_hw_framing_errors(void);
/** @brief Число байт, отброшенных из-за переполнения приёмного кольца. */
uint16_t dbio_get_rx_dropped(void);

#endif	/* DBIO_FIXED_H */
