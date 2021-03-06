#include <string.h>

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "extmod/misc.h"
#include "usb.h"
#include "uart.h"

// this table converts from HAL_StatusTypeDef to POSIX errno
const byte mp_hal_status_to_errno_table[4] = {
    [HAL_OK] = 0,
    [HAL_ERROR] = MP_EIO,
    [HAL_BUSY] = MP_EBUSY,
    [HAL_TIMEOUT] = MP_ETIMEDOUT,
};

NORETURN void mp_hal_raise(HAL_StatusTypeDef status) {
    mp_raise_OSError(mp_hal_status_to_errno_table[status]);
}

int mp_hal_stdin_rx_chr(void) {
    for (;;) {
#if 0
#ifdef USE_HOST_MODE
        pyb_usb_host_process();
        int c = pyb_usb_host_get_keyboard();
        if (c != 0) {
            return c;
        }
#endif
#endif

        #if MICROPY_HW_ENABLE_USB
        byte c;
        if (usb_vcp_recv_byte(&c) != 0) {
            return c;
        }
        #endif
        if (MP_STATE_PORT(pyb_stdio_uart) != NULL && uart_rx_any(MP_STATE_PORT(pyb_stdio_uart))) {
            return uart_rx_char(MP_STATE_PORT(pyb_stdio_uart));
        }
        int dupterm_c = mp_uos_dupterm_rx_chr();
        if (dupterm_c >= 0) {
            return dupterm_c;
        }
        MICROPY_EVENT_POLL_HOOK
    }
}

void mp_hal_stdout_tx_str(const char *str) {
    mp_hal_stdout_tx_strn(str, strlen(str));
}

void mp_hal_stdout_tx_strn(const char *str, size_t len) {
    if (MP_STATE_PORT(pyb_stdio_uart) != NULL) {
        uart_tx_strn(MP_STATE_PORT(pyb_stdio_uart), str, len);
    }
#if 0 && defined(USE_HOST_MODE) && MICROPY_HW_HAS_LCD
    lcd_print_strn(str, len);
#endif
    #if MICROPY_HW_ENABLE_USB
    if (usb_vcp_is_enabled()) {
        usb_vcp_send_strn(str, len);
    }
    #endif
    mp_uos_dupterm_tx_strn(str, len);
}

// Efficiently convert "\n" to "\r\n"
void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
    const char *last = str;
    while (len--) {
        if (*str == '\n') {
            if (str > last) {
                mp_hal_stdout_tx_strn(last, str - last);
            }
            mp_hal_stdout_tx_strn("\r\n", 2);
            ++str;
            last = str;
        } else {
            ++str;
        }
    }
    if (str > last) {
        mp_hal_stdout_tx_strn(last, str - last);
    }
}

void mp_hal_ticks_cpu_enable(void) {
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        #if defined(__CORTEX_M) && __CORTEX_M == 7
        // on Cortex-M7 we must unlock the DWT before writing to its registers
        DWT->LAR = 0xc5acce55;
        #endif
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

void mp_hal_gpio_clock_enable(GPIO_TypeDef *gpio) {
    if (0) {
    #ifdef __HAL_RCC_GPIOA_CLK_ENABLE
    } else if (gpio == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOB_CLK_ENABLE
    } else if (gpio == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOC_CLK_ENABLE
    } else if (gpio == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOD_CLK_ENABLE
    } else if (gpio == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOE_CLK_ENABLE
    } else if (gpio == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOF_CLK_ENABLE
    } else if (gpio == GPIOF) {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOG_CLK_ENABLE
    } else if (gpio == GPIOG) {
        #if defined(STM32L476xx) || defined(STM32L486xx)
        // Port G pins 2 thru 15 are powered using VddIO2 on these MCUs.
        HAL_PWREx_EnableVddIO2();
        #endif
        __HAL_RCC_GPIOG_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOH_CLK_ENABLE
    } else if (gpio == GPIOH) {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOI_CLK_ENABLE
    } else if (gpio == GPIOI) {
        __HAL_RCC_GPIOI_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOJ_CLK_ENABLE
    } else if (gpio == GPIOJ) {
        __HAL_RCC_GPIOJ_CLK_ENABLE();
    #endif
    #ifdef __HAL_RCC_GPIOK_CLK_ENABLE
    } else if (gpio == GPIOK) {
        __HAL_RCC_GPIOK_CLK_ENABLE();
    #endif
    }
}

void mp_hal_pin_config(mp_hal_pin_obj_t pin_obj, uint32_t mode, uint32_t pull, uint32_t alt) {
    GPIO_TypeDef *gpio = pin_obj->gpio;
    uint32_t pin = pin_obj->pin;
    mp_hal_gpio_clock_enable(gpio);
    gpio->MODER = (gpio->MODER & ~(3 << (2 * pin))) | ((mode & 3) << (2 * pin));
    gpio->OTYPER = (gpio->OTYPER & ~(1 << pin)) | ((mode >> 2) << pin);
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(3 << (2 * pin))) | (2 << (2 * pin)); // full speed
    gpio->PUPDR = (gpio->PUPDR & ~(3 << (2 * pin))) | (pull << (2 * pin));
    gpio->AFR[pin >> 3] = (gpio->AFR[pin >> 3] & ~(15 << (4 * (pin & 7)))) | (alt << (4 * (pin & 7)));
}

bool mp_hal_pin_config_alt(mp_hal_pin_obj_t pin, uint32_t mode, uint32_t pull, uint8_t fn, uint8_t unit) {
    const pin_af_obj_t *af = pin_find_af(pin, fn, unit);
    if (af == NULL) {
        return false;
    }
    mp_hal_pin_config(pin, mode, pull, af->idx);
    return true;
}

void mp_hal_pin_config_speed(mp_hal_pin_obj_t pin_obj, uint32_t speed) {
    GPIO_TypeDef *gpio = pin_obj->gpio;
    uint32_t pin = pin_obj->pin;
    gpio->OSPEEDR = (gpio->OSPEEDR & ~(3 << (2 * pin))) | (speed << (2 * pin));
}
