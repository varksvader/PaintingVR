/*
 * Copyright 2017 SDH Inc. All rights reserved.
 *
 * The framework of this app is from the github repository android-sdk-googlevr. Code that relates
 * to setting up the VR environment, initiating sensors on a Daydream ready phone, and enabling the
 * controller handler events are all from Google Inc. 
 *
 * Additional elements added such as extended color palettes, voice command prompts,
 * different brush types (opacity, depth, size), paint strokes, erasing, saving, menu pop ups, and
 * head tracking are created by Varnika Sinha, Sarah Dogar, and Cameron Harvey.
 *
 * This app is intended to launch for Daydream users and possibly other VR set users as well. The
 * app Imagine is the sole product of SDH, whose creators are Varnika Sinha, Sarah Dogar, and
 * Cameron Harvey. Redistribution of this code or alteration of it is prohibited and may only be
 * done with permission.
 *
 * This app is the product of a MAMS group project for the program Apps for Good. To learn more
 * about MAMS, Massachusetts Academy of Math and Science, please go to
 *
 *      http://massacademy.org/
 *
 * To learn more about Apps for Good, please go to
 *
 *      http://www.appsforgood.org
 *
 * The goal of this project is to create a better VR Painter with 360 span for a wider range of
 * audiences with minimal excess hardware.
 *
 * This code has been created in colloboration with Jinchao Yang. For any questions, he can be contacted at:
 *
 *      jyang4@wpi.edu
 *
 * This code includes no framework from Google Inc., this is the product of only the colloborators and Jinchao Yang.
 *
 */

#include <cstring>
#include "vr/gvr/demos/controller_paint/jni/demoapp.h"
#include "saving.h"

 void write(std::ofstream &output, int &integer, char* buffer){
     memcpy(buffer,&integer, sizeof(int));
     output.write(buffer, sizeof(int));
 }

void write(std::ofstream &output, float &floating, char* buffer){
    memcpy(buffer,&floating, sizeof(float));
    output.write(buffer, sizeof(float));
}

int readInt(std::ifstream &input, char*buffer){
    input.get(buffer, sizeof(int));
    int temp = 0;
    memcpy(&temp, buffer, sizeof(int));
    return temp;
}

float readFloat(std::ifstream &input, char*buffer){
    input.get(buffer, sizeof(float));
    float temp = 0;
    memcpy(&temp, buffer, sizeof(float));
    return temp;
}

//file name is hardcoded for now, but could be changed later
void DemoApp::Save(const std::vector<VboInfo> committed_vbos_) {
    std::vector<VboInfo> saved_committed_vbos_ = committed_vbos_;
    std::ofstream outputFile;
    std::string name = "output.txt";
    outputFile.open(name, std::ofstream::binary | std::ofstream::out);
    char buffer[60];
    for (int count = 0; count < saved_committed_vbos_.size(); count++) {
        write(outputFile, saved_committed_vbos_[count].vertex_count, buffer);
        write(outputFile, saved_committed_vbos_[count].color, buffer);
        for (int i = 0; i < saved_committed_vbos_[count].vertex_count; i++)
            write(outputFile, vertices[count][i], buffer);
    }
    outputFile.close();

}

void DemoApp::Load() {
    /*   for (auto it:committed_vbos_) {
           glDeleteBuffers(1, &it.vbo);
       }
       committed_vbos_.clear();
       */
    std::ifstream inputFile;
    std::string name = "output.txt";
    inputFile.open(name, std::ifstream::binary|std::ifstream::in);
    int num;
    int color;
    float* vertex;
    
    std::vector<float*> tempFloat;
    std::vector<int> nums;
    std::vector<int> colors;
    char buffer[60];
    while (inputFile.good()) {
        num = readInt(inputFile, buffer);
        color = readInt(inputFile, buffer);
        vertex = new float[num];
        for(int i = 0; i < num; i++)
            vertex[i] = readFloat(inputFile, buffer);
        VboInfo load;
        load.vetext_count = num;
        load.color = color;
        glGenBuffers(1, &load.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, load.vbo);
        glBufferData(GL_ARRAY_BUFFER, num * sizeof(float),
                 vertex, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        vertices.pushBack(vertex);
    }
    inputFile.close();
}
