#ifndef PWMAUDIO_H
#define PWMAUDIO_H

int audioPinSlice;

void pwmIrqHandler() 
{
    pwm_clear_irq(audioPinSlice);
    pwm_set_gpio_level(AUDIO_PIN, sid_calc() / 64);
}

void pwmInit()
{
    gpio_set_function(AUDIO_PIN, GPIO_FUNC_PWM);
    audioPinSlice = pwm_gpio_to_slice_num(AUDIO_PIN);
    //Complete PWM cycle -> IRQ
    pwm_clear_irq(audioPinSlice);
    pwm_set_irq_enabled(audioPinSlice, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwmIrqHandler);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    //Setup PWM for audio output
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, (static_cast<float>(clock_get_hz(clk_sys)) / SAMPLE_RATE / 1023));
    pwm_config_set_wrap(&config, 1023);
    pwm_init(audioPinSlice, &config, true);
    pwm_set_gpio_level(AUDIO_PIN, 512);
}

#endif //PWMAUDIO_H
