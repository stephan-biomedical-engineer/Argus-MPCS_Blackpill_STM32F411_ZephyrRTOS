#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include "motor_driver.h"
#include <math.h> // Para M_PI

LOG_MODULE_REGISTER(motor_driver, LOG_LEVEL_INF);

static const struct pwm_dt_spec pwm_dev = PWM_DT_SPEC_GET(DT_ALIAS(motor_pwm)); // Era motor_pulse
static const struct gpio_dt_spec dir_pin = GPIO_DT_SPEC_GET(DT_ALIAS(motor_dir), gpios); // Este estava certo (motor-dir -> motor_dir)
static const struct gpio_dt_spec en_pin = GPIO_DT_SPEC_GET(DT_ALIAS(motor_en), gpios);   // Era motor_ena

/* CONFIGURAÇÃO MECÂNICA - CALIBRE ISTO! */
#define STEPS_PER_REV      200.0f  // Motor de 1.8 graus
#define MICROSTEPPING      16.0f   // Configuração do Driver TB67S109
#define LEAD_SCREW_PITCH   2.0f    // Passo do fuso (mm por volta)
#define TOTAL_STEPS_PER_MM ((STEPS_PER_REV * MICROSTEPPING) / LEAD_SCREW_PITCH)

int motor_init(void)
{
    if (!pwm_is_ready_dt(&pwm_dev) || !gpio_is_ready_dt(&dir_pin) || !gpio_is_ready_dt(&en_pin)) 
    {
        LOG_ERR("Erro na inicialização dos pinos do motor");
        return -1;
    }
    gpio_pin_configure_dt(&dir_pin, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&en_pin, GPIO_OUTPUT_INACTIVE); 
    
    return 0;
}

void motor_enable(bool enable)
{
    gpio_pin_set_dt(&en_pin, enable ? 1 : 0);
    if (!enable) motor_stop();
}

void motor_stop(void)
{
    pwm_set_pulse_dt(&pwm_dev, 0);
}

void motor_run(uint32_t flow_rate_ml_h, uint8_t syringe_diameter)
{
    if (flow_rate_ml_h == 0 || syringe_diameter == 0) {
        motor_stop();
        return;
    }

    /* 1. Área da Seringa (mm^2) = PI * (d/2)^2 */
    /* Convertendo para cm^2 para facilitar ml: 1 cm^3 = 1 ml */
    /* Diâmetro vem em mm. Raio = d/2. */
    float radius_cm = (float)syringe_diameter / 20.0f; // mm -> cm
    float area_cm2 = 3.14159f * radius_cm * radius_cm;

    /* 2. Velocidade Linear (cm/h) = Vazão (ml/h) / Área (cm^2) */
    float speed_cm_h = (float)flow_rate_ml_h / area_cm2;
    
    /* 3. Velocidade Linear (mm/s) */
    /* cm/h -> mm/s: *10 / 3600 */
    float speed_mm_s = (speed_cm_h * 10.0f) / 3600.0f;

    /* 4. Frequência (Hz) = mm/s * steps/mm */
    uint32_t hz = (uint32_t)(speed_mm_s * TOTAL_STEPS_PER_MM);

    if (hz < 1) hz = 1;

    /* Configura PWM */
    uint32_t period_ns = 1000000000U / hz;
    pwm_set_dt(&pwm_dev, period_ns, period_ns / 2); // 50% duty

    // Define direção (Fixo por enquanto, ou parametrizar se precisar aspirar)
    gpio_pin_set_dt(&dir_pin, 1); 
    
    // LOG_INF("Motor: %d ml/h (Dia: %d) -> %d Hz", flow_rate_ml_h, syringe_diameter, hz);
}