#ifndef FREQUENCY_DETECTOR_H
#define FREQUENCY_DETECTOR_H

#include <SSD1306.h>
#include <arduinoFFT.h>

#define SAMPLES 256 //must be 2^x number
#define SAMPLING_FREQUENCY 1024 //2x the expected frequency
#define MICROPHONE_PIN 13

#define XRES 128
#define YRES 64
 
arduinoFFT fft;
 
unsigned int samplingPeriod;
unsigned long microSeconds;
  
double vReal[SAMPLES];
double vImag[SAMPLES];
double peak;

double image[XRES];
 
void setupFrequency() 
{
    samplingPeriod = round(1000000*(1.0/SAMPLING_FREQUENCY)); //Period in microseconds
    fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);
}

void displayFrequency(SSD1306Wire *display) {
    int step = (SAMPLES/2) / XRES;
    int c = 0;
    for (int i = 0; i < (SAMPLES/2); i+=step) {
        image[c] = 0;
        for (int k = 0; k < step; k++) {
            image[c] = image[c] + vReal[i+k];
        }
        image[c] = image[c] / step;
        c++;
    }

    double peakValue = 0;
    for (int i = 1; i < XRES; i++) {
        peakValue = max(peakValue, image[i]);
    }


    display->clear();
    for (int i = 0; i < XRES; i++) {
        double y = (image[i] / peakValue) * YRES;
        display->setPixel(i, y);
    }
    display->display();
}

 
void sampleFrequency()
{  
    for(int i = 0; i < SAMPLES; i++)
    {
        microSeconds = micros();
     
        vReal[i] = analogRead(MICROPHONE_PIN);
        vImag[i] = 0;

        /*remaining wait time between samples if  necessary*/
        while(micros() < (microSeconds + samplingPeriod))
        {
          //do nothing
        }
    }
 
    /*Perform FFT on samples*/
    fft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    fft.Compute(FFT_FORWARD);
    fft.ComplexToMagnitude();

    peak = fft.MajorPeak();

    // /*Find peak frequency and print peak*/
    // double peak = fft.MajorPeak(vReal,  SAMPLES, SAMPLING_FREQUENCY);
    // Serial.println(peak);
}

#endif