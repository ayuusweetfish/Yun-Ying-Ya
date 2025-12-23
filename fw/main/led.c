#include "main.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <math.h>

#define USE_LED_STRIP 0
#define USE_LED_PWM   1

static const char *TAG = "LED";
#define PIN_ONBOARD_LED GPIO_NUM_48
// Remember to close the solder bridge

#if USE_LED_STRIP
#include "led_strip.h"
static led_strip_handle_t led_strip;
// Needs extra component `esp_driver_rmt`
#endif

#if USE_LED_PWM
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "driver/mcpwm_timer.h"
#include "driver/mcpwm_oper.h"
#include "driver/mcpwm_cmpr.h"
#include "driver/mcpwm_gen.h"
#include "driver/mcpwm_sync.h"
#include "driver/mcpwm_cap.h"
#include "esp_sleep.h"
#include "soc/ledc_reg.h"
#endif

static inline void output_tint(float r, float g, float b);
static void led_task_fn(void *_unused);

static TaskHandle_t led_task_handle;
static SemaphoreHandle_t led_mutex;

void led_init()
{
#if USE_LED_STRIP
  /// LED strip common configuration
  led_strip_config_t strip_config = {
    .strip_gpio_num = PIN_ONBOARD_LED,  // The GPIO that connected to the LED strip's data line
    .max_leds = 1,                 // The number of LEDs in the strip,
    .led_model = LED_MODEL_SK6812, // LED strip model, it determines the bit timing
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color component format is G-R-B
    .flags = {
      .invert_out = false, // don't invert the output signal
    }
  };

  /// RMT backend specific configuration
  led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency: 10MHz
    .mem_block_symbols = 64,           // the memory size of each RMT channel, in words (4 bytes)
    .flags = {
      .with_dma = false,  // DMA feature is available on chips like ESP32-S3/P4
    }
  };

  /// Create the LED strip object
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

  ESP_LOGI(TAG, "Initialised LED with RMT backend");
#endif

#if USE_LED_PWM
  ESP_ERROR_CHECK(ledc_timer_config(&(ledc_timer_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_11_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 8000,
    .clk_cfg = LEDC_USE_XTAL_CLK,
  }));
  static const struct { ledc_channel_t ch; int pin; } channels[3] = {
    {LEDC_CHANNEL_0, PIN_R},
    {LEDC_CHANNEL_1, PIN_G},
    {LEDC_CHANNEL_2, PIN_B},
  };
  for (int i = 0; i < 3; i++)
    ESP_ERROR_CHECK(ledc_channel_config(&(ledc_channel_config_t){
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = channels[i].ch,
      .timer_sel = LEDC_TIMER_0,
      .intr_type = LEDC_INTR_DISABLE,
      .gpio_num = channels[i].pin,
      .duty = (1 << 11),
      .hpoint = 0,
    }));

  // TODO: Move I2S-related setup elsewhere!
  // TODO: Work around bit timing issue after reset
if (0) {
  gpio_set_direction(PIN_I2S_BCK, GPIO_MODE_DISABLE);
  gpio_set_direction(PIN_I2S_WS, GPIO_MODE_DISABLE);
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

  ESP_ERROR_CHECK(ledc_timer_config(&(ledc_timer_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_4_BIT,
    .timer_num = LEDC_TIMER_1,
    .freq_hz = 1024000,
    .clk_cfg = LEDC_USE_XTAL_CLK,
  }));
  ESP_ERROR_CHECK(ledc_channel_config(&(ledc_channel_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_3,
    .timer_sel = LEDC_TIMER_1,
    .gpio_num = PIN_I2S_BCK,
    .duty = 0b1000,
  }));
  ESP_ERROR_CHECK(ledc_timer_config(&(ledc_timer_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_10_BIT,
    .timer_num = LEDC_TIMER_2,
    .freq_hz = 16000,
    .clk_cfg = LEDC_USE_XTAL_CLK,
  }));
  ESP_ERROR_CHECK(ledc_channel_config(&(ledc_channel_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_4,
    .timer_sel = LEDC_TIMER_2,
    .gpio_num = PIN_I2S_WS,
    .duty = 0b1000000000 - 2,
  }));

void reset_timers()
{
  portDISABLE_INTERRUPTS(); // No need for a critical section

  // Reset counters to minimise desync
  /*
    Equivalent to the following, but with finer timing control:
      REG_SET_BIT(LEDC_LSTIMER2_CONF_REG, LEDC_LSTIMER2_RST);
      REG_SET_BIT(LEDC_LSTIMER1_CONF_REG, LEDC_LSTIMER1_RST);
      REG_CLR_BIT(LEDC_LSTIMER2_CONF_REG, LEDC_LSTIMER2_RST);
      REG_CLR_BIT(LEDC_LSTIMER1_CONF_REG, LEDC_LSTIMER1_RST);
  */
  // Notice that the timer clocks runs slower than system clock,
  // so sub-counter desync may happen.
  // WS needs to fall slightly before/in sync with BCK,
  // we would like to avoid accidental "after" cases,
  // so reset timer 2 first to avoid potential desync.
  uint32_t cfg1 = REG_READ(LEDC_LSTIMER1_CONF_REG);
  uint32_t cfg2 = REG_READ(LEDC_LSTIMER2_CONF_REG);
  uint32_t cfg1_rst = cfg1 | LEDC_LSTIMER1_RST;
  uint32_t cfg2_rst = cfg2 | LEDC_LSTIMER2_RST;
  uint32_t cfg1_rst_clr = cfg1 & ~LEDC_LSTIMER1_RST;
  uint32_t cfg2_rst_clr = cfg2 & ~LEDC_LSTIMER2_RST;
  asm volatile (
    "nop\n" "nop\n"
    "isync\n"
    "s32i %[value_2a], %[addr], %[offs_2]\n" "memw\n"
    "s32i %[value_1a], %[addr], %[offs_1]\n" "memw\n"
    "s32i %[value_2b], %[addr], %[offs_2]\n" "memw\n"
    "s32i %[value_1b], %[addr], %[offs_1]\n" "memw\n"
    :: [addr] "r" (LEDC_LSTIMER1_CONF_REG),
       [offs_1] "i" (0),
       [offs_2] "i" ((uint8_t *)LEDC_LSTIMER2_CONF_REG - (uint8_t *)LEDC_LSTIMER1_CONF_REG),
       [value_1a] "r" (cfg1_rst), [value_1b] "r" (cfg1_rst_clr),
       [value_2a] "r" (cfg2_rst), [value_2b] "r" (cfg2_rst_clr)
    : "memory"
  );

  // Spin to ensure timer reset
  while (REG_READ(LEDC_LSTIMER1_VALUE_REG) == 0) { }
  while (REG_READ(LEDC_LSTIMER2_VALUE_REG) == 0) { }

  // Find the maximum difference for the same reason as above (desync)
  // as this will be subtracted from WS's H-point and we prefer that WS rise/fall early
  uint32_t cnt_diff_max;

  for (int i = 0; i < 8; i++) {
    uint32_t t1_cnt, t2_cnt;
    asm volatile (
      "nop\n" "nop\n"
      "isync\n"
      "nop\n" "nop\n"
      "l32i %[t1_cnt], %[addr], %[offs_1]\n" "memw\n"
      "l32i %[t2_cnt], %[addr], %[offs_2]\n" "memw\n"
      : [t1_cnt] "=&r" (t1_cnt),
        [t2_cnt] "=&r" (t2_cnt)
      : [addr] "r" (LEDC_LSTIMER1_VALUE_REG),
        [offs_1] "i" (0),
        [offs_2] "i" ((uint8_t *)LEDC_LSTIMER2_VALUE_REG - (uint8_t *)LEDC_LSTIMER1_VALUE_REG)
    );

    t2_cnt %= 16;
    uint32_t cnt_diff = (t2_cnt - t1_cnt + 16) % 16;
    if (i == 0 || (cnt_diff - cnt_diff_max + 16) % 16 < 8)
      cnt_diff_max = cnt_diff;

    portENABLE_INTERRUPTS();
    ESP_LOGI(TAG, "Timer 1 CNT %u, Timer 2 CNT %u, diff = %u",
      (unsigned)t1_cnt, (unsigned)t2_cnt, (unsigned)cnt_diff);
    portDISABLE_INTERRUPTS();
  }

  portENABLE_INTERRUPTS();
  ESP_LOGI(TAG, "Max diff = %u", (unsigned)cnt_diff_max);
  REG_WRITE(LEDC_LSCH4_HPOINT_REG, (1024 + (-2 - cnt_diff_max)) % 1024);
  REG_SET_BIT(LEDC_LSCH4_CONF0_REG, LEDC_PARA_UP_LSCH4);
}

  reset_timers();

  ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_ON));
  for (int i = 0; i < 3; i++) {
    ESP_ERROR_CHECK(gpio_sleep_sel_dis(channels[i].pin));
    ESP_ERROR_CHECK(gpio_set_drive_capability(channels[i].pin, 1));
  }

  ESP_ERROR_CHECK(gpio_sleep_sel_dis(PIN_I2S_BCK));
  ESP_ERROR_CHECK(gpio_sleep_sel_dis(PIN_I2S_WS));

  ESP_LOGI(TAG, "Initialised LED with PWM controller");
#endif

  output_tint(0, 0, 0);

  // See "Rationale and analysis for concurrency" below.
  // Create lock
  led_mutex = xSemaphoreCreateMutex();
  assert(led_mutex != NULL);
  // High-priority
  xTaskCreate(led_task_fn, "LED task", 4096, NULL, 8, &led_task_handle);
}

static inline void output_tint(float r, float g, float b)
{
#if USE_LED_STRIP
  led_strip_set_pixel(led_strip, 0, r, g, b);
  led_strip_refresh(led_strip);
#endif

#if USE_LED_PWM
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (1 << 11) - (int)roundf(r * cbrtf(r) * 1.00f * (1 << 11)));
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, (1 << 11) - (int)roundf(g * cbrtf(g) * 0.98f * (1 << 11)));
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, (1 << 11) - (int)roundf(b * cbrtf(b) * 0.98f * (1 << 11)));
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
#endif
}

// ======== Program parsing ======== //

struct tint { float r, g, b; };

struct program_op {
  uint32_t duration;
  enum op_type_t {
    OP_TYPE_DELAY,
    OP_TYPE_FADE,
    OP_TYPE_BREATHE,
  } type;
  uint32_t args[3];
};
#define MAX_DURATION 120000
#define MAX_ARG 100000
#define N_MAX_OPS 256
struct program {
  uint32_t n;
  uint32_t total_dur;
  struct program_op ops[N_MAX_OPS];
};

static inline bool parse_program(struct program *p, const char *s)
{
  p->n = 0;
  p->total_dur = 0;

  while (*s != '\0') {
    uint32_t duration = 0;
    while (!(*s >= '0' && *s <= '9') && *s != '\0') s++;
    if (*s == '\0') break;
    while (*s >= '0' && *s <= '9') {
      duration = duration * 10 + *s - '0';
      if (duration > MAX_DURATION) return false;
      s++;
    }
    p->ops[p->n].duration = duration;

    while (!(*s >= 'A' && *s <= 'Z') && *s != '\0') s++;
    if (*s == '\0') return false;

    enum op_type_t type;
    int argc;
    if (*s == 'D') { type = OP_TYPE_DELAY; argc = 0; }
    else if (*s == 'F') { type = OP_TYPE_FADE; argc = 3; }
    else if (*s == 'B') { type = OP_TYPE_BREATHE; argc = 3; }
    else return false;
    s++;
    p->ops[p->n].type = type;

    for (int i = 0; i < argc; i++) {
      uint32_t arg = 0;
      while (!(*s >= '0' && *s <= '9') && *s != '\0') s++;
      if (*s == '\0') return false;
      while (*s >= '0' && *s <= '9') {
        arg = arg * 10 + *s - '0';
        if (arg > MAX_ARG) return false;
        s++;
      }
      p->ops[p->n].args[i] = arg;
    }

    p->n++;
    p->total_dur += duration;
    if (p->total_dur > MAX_DURATION) return false;
  }

  return true;
}

struct program_ptr {
  int op;
  uint32_t last_t, dt;
  struct tint last_tint;
};

static struct tint program_eval(const struct program *p, struct program_ptr *ptr, uint32_t t)
{
  uint32_t dt = t - ptr->last_t;
  dt += ptr->dt;
  while (dt > 0 && ptr->op < p->n) {
    uint32_t op_dur = p->ops[ptr->op].duration;
    if (dt > op_dur) {
      dt -= op_dur;
      if (p->ops[ptr->op].type == OP_TYPE_FADE)
        ptr->last_tint = (struct tint){
          .r = (float)p->ops[ptr->op].args[0] / 1000,
          .g = (float)p->ops[ptr->op].args[1] / 1000,
          .b = (float)p->ops[ptr->op].args[2] / 1000,
        };
      ptr->op++;
    } else {
      break;
    }
  }
  ptr->last_t = t;
  ptr->dt = dt;
  if (ptr->op >= p->n) return ptr->last_tint;

  // `dt` is the time inside the current operation
  const struct program_op *op = &p->ops[ptr->op];
  switch (op->type) {
    case OP_TYPE_DELAY:
      return ptr->last_tint;

    case OP_TYPE_FADE: {
      float progress = (float)dt / op->duration;
      if (progress < 0.5) progress = progress * progress * 2;
      else progress = 1 - (1 - progress) * (1 - progress) * 2;
      struct tint t1 = (struct tint){
        .r = (float)op->args[0] / 1000,
        .g = (float)op->args[1] / 1000,
        .b = (float)op->args[2] / 1000,
      };
      return (struct tint){
        .r = ptr->last_tint.r + (t1.r - ptr->last_tint.r) * progress,
        .g = ptr->last_tint.g + (t1.g - ptr->last_tint.g) * progress,
        .b = ptr->last_tint.b + (t1.b - ptr->last_tint.b) * progress,
      };
    }

    case OP_TYPE_BREATHE: {
      float progress = (float)dt / op->duration;
      progress = (1 - cosf(progress * (float)(M_PI * 2))) / 2;
      struct tint t1 = (struct tint){
        .r = (float)op->args[0] / 1000,
        .g = (float)op->args[1] / 1000,
        .b = (float)op->args[2] / 1000,
      };
      return (struct tint){
        .r = ptr->last_tint.r + (t1.r - ptr->last_tint.r) * progress,
        .g = ptr->last_tint.g + (t1.g - ptr->last_tint.g) * progress,
        .b = ptr->last_tint.b + (t1.b - ptr->last_tint.b) * progress,
      };
    }

    default:
      return ptr->last_tint;
  }
}

// ======== States and transitions ======== //

static enum led_state_t cur_state = LED_STATE_IDLE;
static uint32_t since = 0;

static struct program run_program;
static struct program_ptr run_program_ptr;

static enum led_state_t last_state;
static uint32_t transition_dur = 0;
static uint32_t last_since_delta;

/*
Rationale and analysis for concurrency.

Step 1.
In addition to the mutex covering state data (state and program),
we would like to avoid the very edge case where update task decides
that it should to sleep, but before it has the chance to actually do so,
the main task preempts and "wakes up" the update task (a no-op at this time),
and then the update task goes to sleep.

Step 2.
A binary semaphore solves the problem, but the current consumption is higher
with the semaphore compared to `vTaskSuspend()` (by ~4 mA); light sleep seems
to be entered less frequently due to `xSemaphoreTake()` and `ulTaskNotifyTake()`
blocking not being treated as a completely suspended thread (suspectedly
due to the timeout, although it's set to `portMAX_DELAY` and while FreeRTOS docs
mention `INCLUDE_vTaskSuspend` in this case, the task does not actually
enter the suspended state -- blocked state instead).

As the frame refresh task is of high priority, we simply check whether it is
running and yield if positive. Actually, it's like a type of mutex lock ^ ^

Ref (in esp-idf/components/freertos/FreeRTOS-Kernel):
  include/freertos/semphr.h: xSemaphoreTake()
  queue.c: xQueueSemaphoreTake()
  tasks.c: vTaskPlaceOnEventList()
  tasks.c: prvAddCurrentTaskToDelayedList()

  include/freertos/task.h: ulTaskNotifyTake()
  tasks.c: ulTaskGenericNotifyTake()
*/

void led_set_state(enum led_state_t state, int transition)
{
  xSemaphoreTake(led_mutex, portMAX_DELAY);

  last_state = cur_state;
  transition_dur = transition;
  last_since_delta = since;

  cur_state = state;
  since = 0;

  if (state == LED_STATE_RUN)
    run_program_ptr = (struct program_ptr){ 0 };

  xSemaphoreGive(led_mutex);

  while (eTaskGetState(led_task_handle) == eRunning) taskYIELD();
  vTaskResume(led_task_handle);
}

static inline struct tint state_render(enum led_state_t state, uint32_t time)
{
  switch (state) {
  case LED_STATE_IDLE:
    return (struct tint){ 0, 0, 0 };

  case LED_STATE_STARTUP:
    return (struct tint){ 0.5f, 0.5f, 0.5f };

  case LED_STATE_CONN_CHECK:
    return (struct tint){ 0, 0.4f, 0.8f };

  case LED_STATE_SPEECH:
    return (struct tint){ 1, 1, 1 };

  case LED_STATE_WAIT_RESPONSE: {
    float t = (float)time / 2000.0f * (float)(M_PI * 2);
    float r = (sinf(t) + 1) / 2;
    float g = (sinf(t + (float)(M_PI * 2 / 3)) + 1) / 2;
    float b = (sinf(t + (float)(M_PI * 4 / 3)) + 1) / 2;
    return (struct tint){ r, g, b };
  }

  case LED_STATE_RUN: {
    if (time < 500) return (struct tint){ 0, 0, 0 };
    return program_eval(&run_program, &run_program_ptr, time - 500);
  }

  case LED_STATE_ERROR:
    return (struct tint){ 1, 0.2f, 0 };

  case LED_STATE_SIGNAL:
    return (struct tint){ 0.05f, 0.05f, 0 };

  case LED_STATE_CALIBRATE: {
    float t = fmod((float)time / 3000.0f, 3);
    float r = 0, g = 0, b = 0;
    if (t < 1) r = (1 - cosf(t * (float)(M_PI * 2))) / 2;
    else if (t < 2) g = (1 - cosf((t - 1) * (float)(M_PI * 2))) / 2;
    else b = (1 - cosf((t - 2) * (float)(M_PI * 2))) / 2;
    r = 0.5 + 0.5 * r;
    g = 0.5 + 0.5 * g;
    b = 0.5 + 0.5 * b;
    return (struct tint){ r, g, b };
  }

  default:
    return (struct tint){ 0, 0, 0 };
  }
}

static void led_task_fn(void *_unused)
{
#define INTERVAL 20
  while (true) {
    xSemaphoreTake(led_mutex, portMAX_DELAY);
    // As the tint may depend on the program, we lock this entire section

    since += INTERVAL;
    struct tint t = state_render(cur_state, since);
    bool suspend_task = false;
    if (transition_dur > 0) {
      if (since >= transition_dur) {
        // Finish transition; keep tint for current state
        transition_dur = 0;
        if (cur_state == LED_STATE_IDLE) suspend_task = true;
      } else {
        // Fade
        float progress = (float)since / transition_dur;
        progress = (progress < 0.5 ?
          progress * progress * 2 :
          1 - (1 - progress) * (1 - progress) * 2);
        struct tint t_last = state_render(last_state, since + last_since_delta);
        t.r = t_last.r + (t.r - t_last.r) * progress;
        t.g = t_last.g + (t.g - t_last.g) * progress;
        t.b = t_last.b + (t.b - t_last.b) * progress;
      }
    }
    xSemaphoreGive(led_mutex);

    output_tint(t.r, t.g, t.b);

    if (suspend_task) {
      vTaskSuspend(led_task_handle);
    }
    vTaskDelay(INTERVAL / portTICK_PERIOD_MS);
  }
}

bool led_set_program(const char *program_source)
{
  run_program_ptr = (struct program_ptr){ 0 };
  return parse_program(&run_program, program_source);
}

uint32_t led_get_program_duration()
{
  return run_program.total_dur;
}
