//
// Created by CammyBoBammy on 5/10/2017.
//

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
