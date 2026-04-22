#pragma once

float rndNormal();
float rndGaussLim(float mean, float lim);
float rndRayleigh(float mean);
float rndUniform();
float rndUShaped();
int   rndPoisson(float mean);

float blocksToSeconds(float blocks);
int   secondsToBlocks(float sec);
