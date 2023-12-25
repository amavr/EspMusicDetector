#include <arduinoFFT.h>

#define SAMPLES 512             // Must be a power of 2
#define SAMPLING_FREQ 40000     // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AUDIO_IN_PIN GPIO_NUM_9 // Signal in on this pin

#define NUM_BANDS 8 // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE 20000   // Used as a crude noise filter, values below this are ignored

// Sampling and FFT stuff
unsigned int sampling_period_us;
byte peak[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // The length of these arrays must be >= NUM_BANDS
int bandValues[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

const uint8_t BAND_NUM = 8;
int steps[BAND_NUM] = {3, 6, 13, 27, 55, 112, 229, 256};
long bands[BAND_NUM] = {0, 0, 0, 0, 0, 0, 0, 0};

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

long avgMax = 0;

void loop()
{
    // Reset bandValues[]
    for (int i = 0; i < NUM_BANDS; i++)
    {
        bandValues[i] = 0;
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

    // ВЫЧМСЛЕНИЕ БПФ
    FFT.DCRemoval();
    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();

    // ГРУППИРОВКА ДАННЫХ ПО ПОЛОСАМ
    int step_prev = 2;
    long curMax = NOISE * 2;
    for (int s = 0; s < BAND_NUM; s++)
    {
        bands[s] = 0;
        int step = steps[s] - step_prev;
        for (int i = step_prev; i < steps[s]; i++)
        {
            if (vReal[i] > NOISE)
            {
                bands[s] += (long) vReal[i];
            }
        }
        bands[s] = bands[s] / step;
        step_prev = steps[s];

        if(bands[s] > curMax) curMax = (long) bands[s];
    }

    // УСТАНОВКА ЧУВСТВИТЕЛЬНОСТИ ЧЕРЕЗ УСРЕД.МАКС.ЗНАЧЕНИЯ
    // адаптивный коэф.
    uint8_t k = abs(curMax - avgMax) > 1000 ? 3 : 4;
    // бегущее среднее * 2^(k + 1)
    avgMax += ((curMax << (k + 1)) - avgMax) >> k;
    // восстановленное бегущее среднее
    long maxVal = avgMax >> (k + 1);

    // МАСШТАБИРОВАНИЕ к 0..255
    for (int s = 0; s < BAND_NUM; s++)
    {
        bands[s] = map(constrain(bands[s], NOISE, maxVal), NOISE, maxVal, 0, 255);
    }


    Serial.println("loop");

    Serial.print(">max:");
    Serial.println(maxVal);
    Serial.printf(">0:%d\n", bands[0]);
    Serial.printf(">1:%d\n", bands[1]);
    Serial.printf(">2:%d\n", bands[2]);
}
