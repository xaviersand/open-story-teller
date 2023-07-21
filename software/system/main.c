
#include "ost_hal.h"
#include "debug.h"
#include "filesystem.h"
#include "picture.h"
#include "qor.h"
#include "rotary-button.h"

#define RUN_TESTS 1

#ifndef RUN_TESTS
int main(void)
{
    // Low level initialization, mainly platform stuff
    // After this call, debug_printf *MUST* be available
    ost_system_initialize();
    debug_printf("\r\n [OST] Starting OpenStoryTeller tests: V%d.%d\r\n", 1, 0);

    // File system access
    filesystem_mount();

    // Display
    ost_display_initialize();
    decompress();

    // Audio

    // Tasker
    ost_tasker_init();

    for (;;)
    {
    }

    return 0;
}
#else

// Raspberry Pico SDK
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico.h"
#include "pico/stdlib.h"

#include "sdcard.h"

const uint16_t tones[3][8] =
    {
        {0xff, 131, 147, 165, 175, 196, 220, 247},
        {0xff, 262, 294, 330, 349, 392, 440, 494},
        {0xff, 524, 988, 660, 698, 784, 880, 988},
};

const uint8_t Happy_birthday[] =
    {
        6, 1, 3, 5, 1, 1, 3, 1, 2, 5, 1, 2, 1, 2, 2, 6, 1, 1, 5, 1, 1, 6, 1, 4, 3, 1, 1,
        5, 1, 1, 6, 1, 1, 5, 1, 2, 3, 1, 2, 1, 1, 1, 6, 0, 1, 5, 1, 1, 3, 1, 1, 2, 1, 4,
        2, 1, 3, 3, 1, 1, 5, 1, 2, 5, 1, 1, 6, 1, 1, 3, 1, 2, 2, 1, 2, 1, 1, 4, 5, 1, 4,
        3, 1, 1, 2, 1, 1, 1, 1, 1, 6, 0, 1, 1, 1, 1, 5, 0, 8, 0};

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "audio_player.h"

void ost_hal_panic()
{
}

extern void qor_sleep();

static qor_mbox_t b;

typedef struct
{
    uint8_t ev;
} ost_event_t;

ost_event_t ev_queue[10];

qor_tcb_t tcb1;
qor_tcb_t tcb2;
qor_tcb_t tcb3;
qor_tcb_t idle;

void UserTask_1(void *args)
{
    //  InstrumentTriggerPE11_Init();
    //  uint32_t count = 0;

    qor_mbox_init(&b, (void **)&ev_queue, 10);
    while (1)
    {

        ost_hal_gpio_set(OST_GPIO_DEBUG_LED, 1);

        //        qor_sleep();
        ost_event_t *e = NULL;
        qor_mbox_wait(&b, (void **)&e, 3);

        for (int i = 0; i < 65500; i++)
        {
            for (int j = 0; j < 100; j++)
                ;
        }

        //        ost_system_delay_ms(500);
        ost_hal_gpio_set(OST_GPIO_DEBUG_LED, 0);
        //        ost_system_delay_ms(500);

        for (int i = 0; i < 65500; i++)
        {
            for (int j = 0; j < 100; j++)
                ;
        }
    }
}

void UserTask_2(void *args)
{
    static ost_event_t wake_up;

    wake_up.ev = 34;

    while (1)
    {
        for (int i = 0; i < 65500; i++)
        {
            for (int j = 0; j < 300; j++)
                ;
        }
        debug_printf("X\n");
        for (int i = 0; i < 65500; i++)
        {
            for (int j = 0; j < 100; j++)
                ;
        }

        qor_mbox_notify(&b, (void **)&wake_up, 1);
    }
}

void UserTask_3(void *args)
{
    int cpt = 0;

    while (1)
    {

        ost_hal_gpio_set(OST_GPIO_DEBUG_LED, 0);
        qor_sleep(500);
        ost_hal_gpio_set(OST_GPIO_DEBUG_LED, 1);
        qor_sleep(500);

        // if (++cpt >= 10)
        // {
        //     cpt = 0;
        //     debug_printf("SU: %d, SO: %d\r\n", tcb3.stack_usage, tcb3.so);
        // }
    }
}

void IdleTaskFunction(void *args)
{
    while (1)
    {
        // Instrumentation, power saving, os functions won't work here
        // __asm volatile("wfi");
    }
}

int main()
{
    // timer_hw->inte = 0;
    // timer_hw->alarm[3] = 0;
    // timer_hw->dbgpause = 1;

    ost_system_initialize();

    // 1. Test the printf output
    debug_printf("\r\n [OST] Starting OpenStoryTeller tests: V%d.%d\r\n", 1, 0);
    /*
      filesystem_mount();

      picture_show("example.bmp");
  */
    // ost_audio_play("out2.wav");

    qor_init(THREADFREQ);

    // qor_create_thread(&tcb1, UserTask_1, 2, "UserTask_1");
    // qor_create_thread(&tcb2, UserTask_2, 1, "UserTask_2");
    qor_create_thread(&tcb3, UserTask_3, 3, "UserTask_3");

    qor_start(&idle, IdleTaskFunction);

    for (;;)
    {

        // ost_hal_audio_loop();

        // ost_hal_gpio_set(OST_GPIO_DEBUG_LED, 1);
        // ost_system_delay_ms(1000);
        // ost_hal_gpio_set(OST_GPIO_DEBUG_LED, 0);
        // ost_system_delay_ms(1000);
    }
    return 0;
}
#endif
