#include <arduinoFFT.h>

#define SAMPLES 512             // Must be a power of 2
#define SAMPLING_FREQ 40000     // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AUDIO_IN_PIN GPIO_NUM_9 // Signal in on this pin

#define NOISE 4000 // Used as a crude noise filter, values below this are ignored

// Sampling and FFT stuff
unsigned int sampling_period_us;

const uint8_t BAND_NUM = 3;
int steps[BAND_NUM] = {5, 96, 256};
long bands[BAND_NUM] = {0, 0, 0};
long avgMax[BAND_NUM] = {0, 0, 0};
long noises[BAND_NUM] = {32000, 20000, 4000};

double vReal[SAMPLES];
double vImag[SAMPLES];
unsigned long newTime;
arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

void setup()
{
    pinMode(AUDIO_IN_PIN, INPUT);
    Serial.begin(115200);

    sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
}

void loop()
{
    // Reset bandValues[]
    for (int i = 0; i < BAND_NUM; i++)
    {
        bands[i] = 0;
    }

    // Sample the audio pin
    for (int i = 0; i < SAMPLES; i++)
    {
        newTime = micros();
        vReal[i] = analogRead(AUDIO_IN_PIN); // A conversion takes about 9.7uS on an ESP32
        vImag[i] = 0;
        while ((micros() - newTime) < sampling_period_us)
        { /* chill */
        }
    }

    // ВЫЧИСЛЕНИЕ БПФ
    FFT.DCRemoval();
    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();

    // ГРУППИРОВКА ДАННЫХ ПО ПОЛОСАМ
    long curMax = NOISE << 4;
    for (int i = 2; i < SAMPLES / 2; i++)
    {
        if (i <= 4 && vReal[i] > noises[0])
        {
            bands[0] += (long)vReal[i];
        }
        else if (i > 7 && i < 150 && vReal[i] > noises[1])
        {
            bands[1] += (long)vReal[i];
        }
        else if (i > 200 && vReal[i] > noises[2])
        {
            bands[2] += (long)vReal[i];
        }
    }

    Serial.println("loop");

    // Serial.print(">max:");
    // Serial.println(maxVal / 1000);
    // Serial.printf(">0:%d\n", bands[0] / 1000);
    // Serial.printf(">1:%d\n", bands[1] / 1000);
    // Serial.printf(">2:%d\n", bands[2] / 1000);

    // МАСШТАБИРОВАНИЕ к 0..255
    for (int i = 0; i < BAND_NUM; i++)
    {
        // УСТАНОВКА ЧУВСТВИТЕЛЬНОСТИ ЧЕРЕЗ УСРЕД.МАКС.ЗНАЧЕНИЯ
        curMax = max(noises[i], bands[i]);
        // адаптивный коэф.
        uint8_t k = abs(curMax - avgMax[i]) > 10000 ? 2 : 4;
        // бегущее среднее * 2^(k + 1)
        avgMax[i] += ((curMax << (k + 1)) - avgMax[i]) >> k;
        // восстановленное бегущее среднее
        long maxVal = (avgMax[i] >> (k + 1)) * 3;

        bands[i] = map(constrain(bands[i], 0, maxVal), 0, maxVal, 0, 255);
    }

    // Serial.print(">max:");
    // Serial.println(maxVal / 1000);
    Serial.printf(">0:%d\n", bands[0]);
    Serial.printf(">1:%d\n", bands[1]);
    Serial.printf(">2:%d\n", bands[2]);
}
