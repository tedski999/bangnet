#ifndef FREQUENCY_DETECTOR_H
#define FREQUENCY_DETECTOR_H

#include <SSD1306.h>
#include <arduinoFFT.h>

#define SAMPLES 512 //must be 2^x number
#define SAMPLING_FREQUENCY 512 //2x the expected frequency
#define MICROPHONE_PIN 13
#define PEAK_SAMPLES 8
#define PEAK_THRESHOLD 1.4

#define XRES 128
#define YRES 64
 
arduinoFFT fft;
 
unsigned int samplingPeriod;
unsigned long microSeconds;
  
double vReal[SAMPLES];
double vImag[SAMPLES];
double peak_record[PEAK_SAMPLES];
byte peak_record_i = 0;
double peak;

double image[XRES];

bool thunder_test = false;

void defaultThunderDetectionCallback() {
    thunder_test = true;
}

void (*thunderDetectionCallback)() = &defaultThunderDetectionCallback;
 
void setupFrequency() 
{
    samplingPeriod = round(1000000*(1.0/SAMPLING_FREQUENCY)); //Period in microseconds
    fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQUENCY);
}

bool isOutlierPeak(byte j) {
    double average = 0;
    bool hasZero = false;
    for (int i = 0; i < PEAK_SAMPLES; i++) {
        if (i != j) {
            average += peak_record[i];
        }
        if (peak_record[i] == 0) {
            hasZero = true;
        }
    }
    average = average / (PEAK_SAMPLES-1);

    return !hasZero && peak_record[j] > average*PEAK_THRESHOLD;
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
    double average = 0;
    for (int i = 1; i < XRES; i++) {
        peakValue = max(peakValue, image[i]);
        average += image[i]/XRES;
    }
    peak_record_i = (peak_record_i+1) % PEAK_SAMPLES;
    peak_record[peak_record_i] = average;
    //peak_record[peak_record_i] = peakValue;

    if (isOutlierPeak(peak_record_i)) {
        thunderDetectionCallback();
    }
    //thunderDetectionCallback();

    double min = 1000 * step;
    //peakValue = max(peakValue, min);
    peakValue = 2000;


    display->clear();
    for (int i = 0; i < XRES; i++) {
        double y = (image[i] / peakValue) * YRES;
        display->setPixel(i, y);
    }
    String string = String(peak_record[peak_record_i]);
        
    if (thunder_test) {
        string = string + " - thunder";
        thunder_test = false;
    }
    display->drawString(0, 54, string);
        
    display->display();
}

void setThunderDetectionCallback(void (*f)()) {
    thunderDetectionCallback = f;
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