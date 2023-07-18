/**
 * @brief
 *
 * // Quite OK RTOS scheduler is a very simple real-time, pre-emptive, tickless tasker
 * Design goals:
 *  - Easily portable (limited assembly)
 *  - Tick-less
 *  - Preemptive
 *  - Only one inter-thread resource: mailboxes (no mutex, semaphore...)

 */

#include "ost_hal.h"
#include "debug.h"
#include "qor.h"
#include <stdlib.h>

// Raspberry Pico SDK
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico.h"
#include "pico/critical_section.h"
#include "hardware/exception.h"
#include "RP2040.h"

void qor_switch_context();
static void timer_stop();

// ===========================================================================================================
// ARM GENERIC
// ===========================================================================================================
inline static void enable_irq()
{
    __asm volatile("cpsie i");
}

inline static void disable_irq()
{
    __asm volatile("cpsid i");
}

void qor_sleep_ms(uint8_t svc, uint32_t ms)
{

    __wfi;
}

static inline uint32_t qor_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();
    disable_irq();
    return primask;
}

void qor_exit_critical(uint32_t status)
{
    __set_PRIMASK(status);
}

__attribute__((naked)) void PendSV_Handler()
{
    qor_switch_context();
}

static const bool qor_inside_interrupt(void)
{
    uint32_t ulCurrentInterrupt;
    bool xReturn;

    // Obtain the number of the currently executing interrupt
    __asm volatile("mrs %0, ipsr"
                   : "=r"(ulCurrentInterrupt)::"memory");
    return ulCurrentInterrupt == 0 ? false : true;
}

void qor_svc_call(void)
{
    timer_stop();
    volatile uint32_t *icsr = (void *)0xE000ED04;
    // Pend a PendSV exception using by writing 1 to PENDSVSET at bit 28
    *icsr = 0x1 << 28;
    // flush pipeline to ensure exception takes effect before we
    // return from this routine
    __asm("isb");
}

// ===========================================================================================================
// RASPBERRY PICO
// ===========================================================================================================

static volatile uint32_t timer_period;

#define ALARM_NUM 0
#define ALARM_IRQ TIMER_IRQ_0

#include "hardware/structs/systick.h"

//-----------------------------------------------------------------------------

static void timer_irq(void)
{
    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
    qor_svc_call();
}

static void timer_set_alam(uint32_t delay_ms)
{
    if (delay_ms > 0)
    {
        // Alarm is only 32 bits so if trying to delay more
        // than that need to be careful and keep track of the upper
        // bits
        uint64_t target = timer_hw->timerawl + delay_ms * 1000;
        timer_hw->alarm[ALARM_NUM] = (uint32_t)target;
        // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
        hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
    }
}

static void timer_stop()
{
    hw_clear_bits(&timer_hw->inte, 1u << ALARM_NUM);
}

//-----------------------------------------------------------------------------
static void timer_init()
{
    // Set irq handler for alarm irq
    irq_set_exclusive_handler(ALARM_IRQ, timer_irq);
    // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
    // Enable the alarm irq
    irq_set_enabled(ALARM_IRQ, true);
}

// ===========================================================================================================
// GLOBAL AND STATIC VARIABLES
// ===========================================================================================================

static uint32_t Stacks[MAXNUMTHREADS][STACKSIZE];

/* Pointer to the currently running thread */
qor_tcb_t *RunPt = NULL;
static qor_tcb_t *TcbHead = NULL;
static qor_tcb_t *IdleTcb = NULL;
static thread_func_t IdleTask = NULL;

/* The variable ActiveTCBsCount tracks the number of TCBs in use by the OS */
static uint32_t ActiveTCBsCount = 0;

// ===========================================================================================================
// Quite Ok RTOS private and public functions
// ===========================================================================================================
void qor_init(uint32_t scheduler_frequency_hz)
{
    exception_set_exclusive_handler(PENDSV_EXCEPTION, PendSV_Handler);
}

void qor_exit_loop()
{
    for (;;)
        ;
}

extern void qor_go();

uint32_t *qor_initialize_stack(uint32_t *top_of_stack, thread_func_t task, void *args)
{
    // ARM  Calling convention: the folowwing registers are automatically saved onto the stack by the processor (in this ordoer on the stack)
    // DDI0419C_arm_architecture_v6m_reference_manual-1.pdf  B1.5.6 Exception entry behavior
    top_of_stack--;
    /* From the "STM32 Cortex-M4 Programming Manual" on page 23:
     * attempting to execute instructions when  the T bit is 0 results in a fault or lockup */
    *top_of_stack = 0x01000000; /* Thumb Bit (PSR) */
    top_of_stack--;
    *top_of_stack = (uint32_t)task; // PC Program Counter (R15)
    top_of_stack--;
    *top_of_stack = (uint32_t)qor_exit_loop; /* (LR) Link Register (Return address) R14 */
    top_of_stack -= 5;                       // skip R12, R3, R2, R1
    *top_of_stack = (uint32_t)args;          // R0
    top_of_stack -= 8;                       // R11 -> R4
    return top_of_stack;
}

void qor_create_thread(qor_tcb_t *tcb, thread_func_t task, uint8_t priority, const char *name)
{
    assert_or_panic(ActiveTCBsCount >= 0 && ActiveTCBsCount < MAXNUMTHREADS);
    disable_irq();

    tcb->state = qor_tcb_state_active;
    tcb->wait_time = 0;
    tcb->state = qor_tcb_state_active;
    tcb->priority = priority;
    tcb->name = name;
    tcb->next = NULL;
    tcb->mbox = NULL;
    tcb->wait_next = NULL;
    tcb->sp = qor_initialize_stack(&Stacks[ActiveTCBsCount][STACKSIZE], task, (void *)name);

    if (TcbHead == NULL)
    {
        TcbHead = tcb;
    }
    else
    {
        // Go to the end of the queue
        qor_tcb_t *t = TcbHead;
        while (t->next != NULL)
        {
            t = t->next;
        }
        // Add TCB to the end of the queue
        t->next = tcb;
    }
    ActiveTCBsCount++;

    enable_irq();
}

bool qor_start(qor_tcb_t *idle_tcb, thread_func_t idle_task)
{
    assert_or_panic(ActiveTCBsCount > 0);

    if ((idle_task == NULL) || (idle_tcb == NULL))
    {
        return false;
    }

    qor_create_thread(idle_tcb, idle_task, 0, "IdleTask");

    // FIXME: use the scheduler to find the best first thread to start
    IdleTcb = idle_tcb;
    IdleTask = idle_task;
    RunPt = TcbHead;

    timer_init();

    /* Prevent the timer's ISR from firing before start is called */
    disable_irq();

    qor_go();

    /* This statement should not be reached */
    ost_hal_panic();

    return true;
}

void qor_scheduler(void)
{
    /*
        La stratégie est la suivante:
        - On va circuler parmi tous les TCB (liste chaînée)
        - On va retenir le TCB dont la priorité est la plus élevée parmi les actifs
        - On va retenir le TCB dont la priorité est la plus élevée parmi les endormis
        - On va élir le TCB actif trouvé, sinon l'endormi, et finalement la tâche idle si aucune autre tâche n'est éligible
     */
    uint32_t max_priority = 0;
    qor_tcb_t *best_active = NULL;
    qor_tcb_t *best_sleeping = NULL;
    qor_tcb_t *t = TcbHead;

    uint32_t next_alarm = 60000; // default alarm is next is 60 seconds

    uint64_t ts = time_us_64() / 1000; // in ms

    while (t != NULL)
    {
        uint64_t final = t->ts + t->wait_time;

        // First look if the task can be woken-up
        if (final <= ts)
        {
            t->wait_time = 0;
        }

        if ((t->priority > max_priority) &&
            (t->wait_time == 0))
        {
            max_priority = t->priority;
            if (t->state == qor_tcb_state_active)
            {
                best_active = t;
            }
            else
            {
                best_sleeping = t;
            }
        }

        // Compute the minimal alarm delay asked
        if (t->wait_time > 0)
        {
            // We have a sleep order, compute distance to final absolute timestamp
            uint64_t diff = final - ts;
            if (diff < next_alarm)
            {
                next_alarm = diff;
            }
        }

        t = t->next;
    }

    if (best_active != NULL)
    {
        RunPt = best_active;
    }
    else if (best_sleeping != NULL)
    {
        RunPt = best_sleeping;
    }
    else
    {
        RunPt = IdleTcb;
    }

    timer_set_alam(next_alarm);
}

void qor_sleep(uint32_t sleep_duration_ms)
{
    uint32_t status = qor_enter_critical();
    RunPt->state = qor_tcb_state_sleep;
    RunPt->ts = time_us_64() / 1000;
    RunPt->wait_time = sleep_duration_ms;
    qor_exit_critical(status);
    qor_svc_call(); // call scheduler, recompute next timeout
}

// ===========================================================================================================
// MAILBOX IMPLEMENTATION
// ===========================================================================================================

void qor_mbox_init(qor_mbox_t *mbox, void **msgBuffer, uint32_t maxCount)
{
    mbox->msgBuffer = msgBuffer;
    mbox->maxCount = maxCount;
    mbox->read = 0;
    mbox->read = 0;
    mbox->head = NULL;
}

uint32_t qor_mbox_wait(qor_mbox_t *mbox, void **msg, uint32_t wait_ms)
{
    uint32_t status = qor_enter_critical();

    // No any data, block on that resource
    if (mbox->count == 0)
    {
        if (wait_ms > 0)
        {
            RunPt->mbox = mbox;
            mbox->head = RunPt;
            qor_sleep(wait_ms);
        }
        else
        {
            qor_exit_critical(status);
            return QOR_MBOX_ERROR;
        }
    }

    status = qor_enter_critical();

    --mbox->count;
    *msg = mbox->msgBuffer[mbox->read++];
    if (mbox->read >= mbox->maxCount)
    {
        mbox->read = 0;
    }
    qor_exit_critical(status);
    return QOR_MBOX_OK;
}

uint32_t qor_mbox_notify(qor_mbox_t *mbox, void *msg, uint32_t notifyOption)
{
    uint32_t status = qor_enter_critical();

    if (mbox->count >= mbox->maxCount)
    {
        qor_exit_critical(status);
        return QOR_MBOX_FULL;
    }
    if (notifyOption == QOR_MBOX_OPTION_SEND_FRONT)
    {
        if (mbox->read <= 0)
        {
            mbox->read = mbox->maxCount - 1;
        }
        else
        {
            --mbox->read;
        }
        mbox->msgBuffer[mbox->read] = msg;
    }
    else
    {
        mbox->msgBuffer[mbox->write++] = msg;
        if (mbox->write >= mbox->maxCount)
        {
            mbox->write = 0;
        }
    }
    mbox->count++;

    // We warn all waiting threads that a new message is available
    qor_tcb_t *t = mbox->head;
    if (t != NULL)
    {
        t->wait_time = 0;
    }

    qor_exit_critical(status);
    qor_svc_call(); // call scheduler
    return QOR_MBOX_OK;
}

void qor_mbox_get_stats(qor_mbox_t *mbox, mbox_stats_t *info)
{
    uint32_t status = qor_enter_critical();

    info->count = mbox->count;
    info->maxCount = mbox->maxCount;

    info->taskCount = 0;
    qor_tcb_t *head = mbox->head;
    while (head != NULL)
    {
        info->taskCount++;
        head = head->wait_next;
    }

    qor_exit_critical(status);
}
