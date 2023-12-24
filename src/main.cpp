#include <arduinoFFT.h>

#define SAMPLES 1024        // Must be a power of 2
#define SAMPLING_FREQ 40000 // Hz, must be 40000 or less due to ADC conversion time. Determines maximum frequency that can be analysed by the FFT Fmax=sampleF/2.
#define AMPLITUDE 1000      // Depending on your audio source level, you may need to alter this value. Can be used as a 'sensitivity' control.
#define AUDIO_IN_PIN GPIO_NUM_9      // Signal in on this pin

#define NUM_BANDS 8                               // To change this, you will need to change the bunch of if statements describing the mapping from bins to bands
#define NOISE 500                                  // Used as a crude noise filter, values below this are ignored

// Sampling and FFT stuff
unsigned int sampling_period_us;
byte peak[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // The length of these arrays must be >= NUM_BANDS
int bandValues[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
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

    // Compute FFT
    FFT.DCRemoval();
    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();

    // Analyse FFT results
    for (int i = 2; i < (SAMPLES / 2); i++)
    { // Don't use sample 0 and only first SAMPLES/2 are usable. Each array element represents a frequency bin and its value the amplitude.
        if (vReal[i] > NOISE)
        { // Add a crude noise filter

            // 8 bands, 12kHz top band
              if (i<=3 )           bandValues[0]  += (int)vReal[i];
              if (i>3   && i<=6  ) bandValues[1]  += (int)vReal[i];
              if (i>6   && i<=13 ) bandValues[2]  += (int)vReal[i];
              if (i>13  && i<=27 ) bandValues[3]  += (int)vReal[i];
              if (i>27  && i<=55 ) bandValues[4]  += (int)vReal[i];
              if (i>55  && i<=112) bandValues[5]  += (int)vReal[i];
              if (i>112 && i<=229) bandValues[6]  += (int)vReal[i];
              if (i>229          ) bandValues[7]  += (int)vReal[i];

        }
    }
    Serial.println("loop");

    Serial.print(">1:");
    Serial.println(bandValues[1]);
    Serial.print(">3:");
    Serial.println(bandValues[3]);
    Serial.print(">5:");
    Serial.println(bandValues[5]);

}
