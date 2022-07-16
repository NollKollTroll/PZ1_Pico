#ifndef PWMAUDIO_H
#define PWMAUDIO_H

int audioPinSlice;

void pwmIrqHandler() 
{
    pwm_clear_irq(audioPinSlice);
    //div by 64 to get 10 bits, add 512 to change from signed to unsigned
    pwm_set_gpio_level(AUDIO_PIN, (sid_calc() / 64) + 512);
}

void pwmInit(uint32_t SampleRate)
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
    //10-bit output, 1024 levels -> 1023
    pwm_config_set_clkdiv(&config, (static_cast<float>(clock_get_hz(clk_sys)) / SampleRate / 1023));
    pwm_config_set_wrap(&config, 1023);
    pwm_init(audioPinSlice, &config, true);
    //half of 1024 -> 512
    pwm_set_gpio_level(AUDIO_PIN, 512);
}

#endif //PWMAUDIO_H
