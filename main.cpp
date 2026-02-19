#include <iostream>
#include <cstdlib>
#include <iterator>
#include <thread>
#include <chrono>
#include <array>
#include <cmath>
#include <algorithm>
#include <vector>
#include <sys/ioctl.h>
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <string>

using namespace std;
using namespace std::chrono_literals;

int numRows = 47;
int numCols = 204;

int screenCenterX = numCols / 2;
int screenCenterY = numRows / 2;

float xScale = 2;
int defaultScale = 25;
int scale = 25; 
int cameraDistance = 5;

int rowScale;
int colScale;

float angleX = 0;
float angleY = 0;
float angleZ = 0;

float cameraZ = 10.0f;
auto interval = 100ms;
vector<char> screenBuffer;

volatile sig_atomic_t FLAG_SIGWINCH = 1;

char faceChars[6] = {'.','-','=','+','#','@'};

array<float, 24> vertices = {
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f
};

array<float, 24> transformedVertices = {};

const int edges[12][2] = {
    {0,1}, {1,3}, {3,2}, {2,0}, // Front square
    {4,5}, {5,7}, {7,6}, {6,4}, // Back square
    {0,4}, {1,5}, {2,6}, {3,7}  // Middle
};

const int triangles[12][3] = {
    // Front
    {0,1,2}, {1,3,2},
    // Back
    {5,4,7}, {4,6,7},
    // Left
    {4,0,6}, {0,2,6},
    // Right
    {1,5,3}, {5,7,3},
    // Top
    {2,3,6}, {3,7,6},
    // Bottom
    {4,5,0}, {5,1,0}
};

void handleSIGWINCH(int) {
    FLAG_SIGWINCH = 1;
}

array<int,16> projectVertices() {
    array<int, 16> projectedVertices = {};
    for (int n = 0; n < 8; n++) {
        float z = transformedVertices[3 * n + 2];
        float cameraScale = cameraDistance / (cameraZ - z);
        projectedVertices[2 * n] = static_cast<int>(transformedVertices[3 * n] * scale * cameraScale * xScale) + screenCenterX;
        projectedVertices[2 * n + 1] = static_cast<int>(-transformedVertices[3 * n + 1] * scale * cameraScale) + screenCenterY;
    }
    return projectedVertices;
}

void rotatePoints() {
    float sinX = sin(angleX);
    float cosX = cos(angleX);
    float sinY = sin(angleY);
    float cosY = cos(angleY);
    float sinZ = sin(angleZ);
    float cosZ = cos(angleZ);

    float cosZsinY = cosZ * sinY;
    float cosZcosY = cosZ * cosY;

    float x1 = cosZ * cosY;
    float x2 = cosZsinY * sinX - sinZ * cosX;
    float x3 = cosZsinY * cosX + sinZ * sinX;

    float y1 = sinZ * cosY;
    float y2 = sinZ * sinY * sinX + cosZ * cosX;
    float y3 = sinZ * sinY * cosX - cosZ * sinX;
    
    float z2 = sinX * cosY;
    float z3 = cosX * cosY;

    for (int n = 0; n < 8; n++) {
        float x = vertices[3 * n];
        float y = vertices[3 * n + 1];
        float z = vertices[3 * n + 2];

        transformedVertices[3 * n] = x * x1 + y * x2 + z * x3;
        transformedVertices[3 * n + 1] = x * y1 + y * y2 + z * y3;
        transformedVertices[3 * n + 2] = -x * sinY + y * z2 + z * z3;
    }
}

void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, char pixel) {
    int minX = max(0, min({x1, x2, x3}));
    int maxX = min(numCols - 1, max({x1, x2, x3}));
    int minY = max(0, min({y1, y2, y3}));
    int maxY = min(numRows - 1, max({y1, y2, y3}));
    
    auto edge = [](int x0,int y0,int x1,int y1,int x,int y){
        return (x - x0)*(y1 - y0) - (y - y0)*(x1 - x0);
    };

    for(int y = minY; y <= maxY; y++) {
        for(int x = minX; x <= maxX; x++) {

            int w0 = edge(x2,y2,x3,y3,x,y);
            int w1 = edge(x3,y3,x1,y1,x,y);
            int w2 = edge(x1,y1,x2,y2,x,y);

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                screenBuffer[y * numCols + x] = pixel;
            }
        }
    }
}

bool isFrontFace(int x1, int y1, int x2, int y2, int x3, int y3)
{
    int crossProduct = (x2 - x1)*(y3 - y1) - (y2 - y1)*(x3 - x1);
    return crossProduct < 0;
}


int main() {    
    cout << "\033[?25l";
    std::ios::sync_with_stdio(false);
    signal(SIGWINCH, handleSIGWINCH);

    while (true) {
        if (FLAG_SIGWINCH) {
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            numRows = w.ws_row;
            numCols = w.ws_col;
            screenCenterX = numCols / 2;
            screenCenterY = numRows / 2;
            FLAG_SIGWINCH = 0;

            rowScale = defaultScale * numRows / 47;
            if (numCols > 95) {
                colScale = defaultScale;
            } else {
                colScale = defaultScale * numCols / 95;
            }
            scale = min(rowScale, colScale);
        }

        size_t requiredSize = numRows * numCols;
        if (screenBuffer.size() != requiredSize) {
            screenBuffer.resize(requiredSize, ' ');
        } else {
            std::memset(screenBuffer.data(), ' ', requiredSize);
        }

        rotatePoints();
        auto projectedVertices = projectVertices();

        for (int n = 0; n < 12; n++) {
            int v1 = triangles[n][0];
            int v2 = triangles[n][1];
            int v3 = triangles[n][2];

            int x1 = projectedVertices[2 * v1];
            int y1 = projectedVertices[2 * v1 + 1];

            int x2 = projectedVertices[2 * v2];
            int y2 = projectedVertices[2 * v2 + 1];

            int x3 = projectedVertices[2 * v3];
            int y3 = projectedVertices[2 * v3 + 1];

            if (isFrontFace(x1,y1,x2,y2,x3,y3)) {
                char pixel = faceChars[n >> 1];
                fillTriangle(x1,y1,x2,y2,x3,y3,pixel);
            }
        }

        write(STDOUT_FILENO, "\033[H", 3);
        write(STDOUT_FILENO, screenBuffer.data(), requiredSize);


        static float t = 0;
        t += interval.count() * 0.001f;

        angleX = 100 * 0.03f * sin(t * 0.5f + 1);
        angleY = 100 * 0.04f * cos(t * 0.4f + 2);
        angleZ = 100 * 0.05f * sin(t * 0.3f + 3);

        this_thread::sleep_for(interval);
    }

    return 0;
}