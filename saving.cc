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
 * This code has been created in collaboration with Jinchao Yang. For any questions, he can be contacted at:
 *
 *      jyang4@wpi.edu
 *
 * This code includes no framework from Google Inc. This code is the product of only the collaborators and Jinchao Yang.
 *
 */

#include <cstring>
#include "vr/gvr/demos/controller_paint/jni/demoapp.h"
#include <android/log.h>
#include <string>

void write(std::ofstream &output, int integer, char *buffer) {
    memcpy(buffer, &integer, sizeof(int));
    output.write(buffer, sizeof(int));
}

void write(std::ofstream &output, float floating, char *buffer) {
    memcpy(buffer, &floating, sizeof(float));
    output.write(buffer, sizeof(float));
}

int readInt(std::ifstream &input, char *buffer) {
    input.read(buffer, sizeof(int));
    int temp = 0;
    memcpy(&temp, buffer, sizeof(int));
    return temp;
}

float readFloat(std::ifstream &input, char *buffer) {
    input.read(buffer, sizeof(float));
    float temp = 0;
    memcpy(&temp, buffer, sizeof(float));
    return temp;
}

//file name is hardcoded for now, but could be changed later
void DemoApp::Save(const std::vector<VboInfo> committed_vbos_) {
    __android_log_write(ANDROID_LOG_VERBOSE, "SAVE", "Save start");
    std::vector<VboInfo> saved_committed_vbos_ = committed_vbos_;
    std::ofstream outputFile;
    std::string name = "output.txt";
    if(!outputFile.good())
        __android_log_write(ANDROID_LOG_VERBOSE, "SAVE", "File bad");
    outputFile.open(name, std::ofstream::binary | std::ofstream::out);
    char buffer[60];
    write(outputFile, (int)saved_committed_vbos_.size(), buffer);
    __android_log_print(ANDROID_LOG_VERBOSE, "SAVE", "Vertices Count: %d",
                        (int)saved_committed_vbos_.size());
    for (int count = 0; count < saved_committed_vbos_.size(); count++) {
        VboInfo &vboInfo = saved_committed_vbos_[count];
        write(outputFile, vboInfo.vertex_count, buffer);
        __android_log_print(ANDROID_LOG_VERBOSE, "SAVE", "Vertex Count: %d", vboInfo.vertex_count);

        write(outputFile, vboInfo.color, buffer);
        __android_log_print(ANDROID_LOG_VERBOSE, "SAVE", "Color: %d", vboInfo.color);

        for (int i = 0; i < vboInfo.vertex_count; i++) {
            write(outputFile, vertices[count][i], buffer);
            //__android_log_print(ANDROID_LOG_VERBOSE, "SAVE", "Vertex[%d]: %f", i, vertices[count][i] );
        }
    }
    outputFile.close();
    __android_log_write(ANDROID_LOG_VERBOSE, "SAVE", "Save end");
}

void DemoApp::Load() {
    /*   for (auto it:committed_vbos_) {
           glDeleteBuffers(1, &it.vbo);
       }
       committed_vbos_.clear();
       */
    __android_log_write(ANDROID_LOG_VERBOSE, "LOAD", "Load start");
    std::ifstream inputFile;
    std::string name = "output.txt";
    inputFile.open(name, std::ifstream::binary | std::ifstream::in);
    int num;
    int color;
    float *vertex;

    std::vector<float *> tempFloat;
    std::vector<int> nums;
    std::vector<int> colors;
    char buffer[60];
    if(!inputFile.good())
        __android_log_write(ANDROID_LOG_VERBOSE, "LOAD", "File bad");
    int arraySize = readInt(inputFile, buffer);
    __android_log_print(ANDROID_LOG_VERBOSE, "LOAD", "Vertices Count: %d", arraySize);
    for (int x = 0; x < arraySize; x++) {
        num = readInt(inputFile, buffer);
        __android_log_print(ANDROID_LOG_VERBOSE, "LOAD", "Vertex Count: %d", num);
        color = readInt(inputFile, buffer);
        __android_log_print(ANDROID_LOG_VERBOSE, "LOAD", "Vertex Count: %d", color);
        vertex = new float[num];
        for (int i = 0; i < num; i++) {
            vertex[i] = readFloat(inputFile, buffer);
            // __android_log_print(ANDROID_LOG_VERBOSE,"LOAD", "Vertex[%d]: %f", i, vertex[i]);
        }
        VboInfo load;
        load.vertex_count = num;
        load.color = color;
        glGenBuffers(1, &load.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, load.vbo);
        glBufferData(GL_ARRAY_BUFFER, num * sizeof(float),
                     vertex, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        committed_vbos_.push_back(load);
        vertices.push_back(vertex);
    }
    inputFile.close();
    __android_log_write(ANDROID_LOG_VERBOSE, "LOAD", "Load end");
}
