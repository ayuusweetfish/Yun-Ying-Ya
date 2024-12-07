#include "main.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <math.h>

#define USE_LED_STRIP 1
#define USE_LED_PWM   1

static const char *TAG = "LED";
#define PIN_ONBOARD_LED GPIO_NUM_48
// Remember to close the solder bridge

#if USE_LED_STRIP
#include "led_strip.h"
static led_strip_handle_t led_strip;
#endif

#if USE_LED_PWM
#include "driver/ledc.h"
#include "driver/gpio.h"
#endif

static inline void output_tint(float r, float g, float b);
static void led_task_fn(void *_unused);

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
if (1) {
  ESP_ERROR_CHECK(ledc_timer_config(&(ledc_timer_config_t){
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 4000,
    .clk_cfg = LEDC_AUTO_CLK,
  }));
  static const struct { ledc_channel_t ch; int pin; } channels[3] = {
    {LEDC_CHANNEL_0, 17},
    {LEDC_CHANNEL_1, 18},
    {LEDC_CHANNEL_2, 21},
  };
  for (int i = 0; i < 3; i++)
    ESP_ERROR_CHECK(ledc_channel_config(&(ledc_channel_config_t){
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = channels[i].ch,
      .timer_sel = LEDC_TIMER_0,
      .intr_type = LEDC_INTR_DISABLE,
      .gpio_num = channels[i].pin,
      .duty = (1 << 13),
      .hpoint = 0,
    }));
} else {
  gpio_config(&(gpio_config_t){
    .pin_bit_mask = (1 << 17) | (1 << 18) | (1 << 21),
    .mode = GPIO_MODE_OUTPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
  });
}

  ESP_LOGI(TAG, "Initialised LED with PWM controller");
#endif

  output_tint(0, 0, 0);
  // High-priority
  xTaskCreate(led_task_fn, "LED task", 4096, NULL, 8, NULL);
}

static inline void output_tint(float r, float g, float b)
{
#if USE_LED_STRIP
  led_strip_set_pixel(led_strip, 0, r, g, b);
  led_strip_refresh(led_strip);
#endif

#if USE_LED_PWM
if (1) {
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (1 << 13) - (int)roundf(r * (1 << 13)));
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, (1 << 13) - (int)roundf(g * (1 << 13)));
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, (1 << 13) - (int)roundf(b * (1 << 13)));
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
} else {
  gpio_set_level(17, !(r >= 0.5f));
  gpio_set_level(18, !(g >= 0.5f));
  gpio_set_level(21, !(b >= 0.5f));
}
#endif
}

// ======== Program parsing ======== //

struct tint { float r, g, b; };

struct program_op {
  int duration;
  enum op_type_t {
    OP_TYPE_DELAY,
    OP_TYPE_FADE,
    OP_TYPE_BREATHE,
  } type;
  int args[3];
};
#define MAX_DURATION 120000
#define MAX_ARG 100000
#define N_MAX_OPS 256
struct program {
  int n;
  int total_dur;
  struct program_op ops[N_MAX_OPS];
};

static inline bool parse_program(struct program *p, const char *s)
{
  p->n = 0;
  p->total_dur = 0;

  while (*s != '\0') {
    int duration = 0;
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
      int arg = 0;
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
  int last_t, dt;
  struct tint last_tint;
};

static struct tint program_eval(const struct program *p, struct program_ptr *ptr, int t)
{
  int dt = t - ptr->last_t;
  dt += ptr->dt;
  while (dt > 0 && ptr->op < p->n) {
    int op_dur = p->ops[ptr->op].duration;
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
static int since = 0;

static struct program run_program;
static struct program_ptr run_program_ptr;

static enum led_state_t last_state;
static int transition_dur = 0;
static int last_since_delta;

void led_set_state(enum led_state_t state, int transition)
{
  last_state = cur_state;
  transition_dur = transition;
  last_since_delta = since;

  cur_state = state;
  since = 0;
}

static inline struct tint state_render(enum led_state_t state, int time)
{
  switch (state) {
  case LED_STATE_IDLE:
    return (struct tint){ 0, 0, 0 };

  case LED_STATE_CONN_CHECK:
    return (struct tint){ 0, 0, 1 };

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

  default:
    return (struct tint){ 0, 0, 0 };
  }
}

void led_task_fn(void *_unused)
{
  while (true) {
    since += 10;
    struct tint t = state_render(cur_state, since);
    if (transition_dur > 0) {
      if (since >= transition_dur) {
        // Finish transition
        transition_dur = 0;
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

    output_tint(t.r, t.g, t.b);

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

bool led_set_program(const char *program_source)
{
  run_program_ptr = (struct program_ptr){ 0 };
  return parse_program(&run_program, program_source);
}
